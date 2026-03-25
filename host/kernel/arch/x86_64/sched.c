/*
 * bubblyVM -- Process Scheduler
 * File: host/kernel/arch/x86_64/sched.c
 *
 * Implements a simple round-robin preemptive scheduler.
 *
 * Design (Tier 1):
 *   - Fixed-size process table (MAX_PROCESSES slots)
 *   - Round-robin run queue: a circular linked list of READY processes
 *   - Time slice: each process gets time_slice_ms ticks before preemption
 *   - Idle process (PID 0): runs when nothing else is READY
 *   - sched_tick() called from IRQ0 -- decrements slice, switches if expired
 *
 * Locking:
 *   The scheduler uses a spinlock around run queue manipulation.
 *   IRQs are briefly disabled during context switch to prevent re-entry.
 *
 * Future (Tier 2+):
 *   - Priority queues / multilevel feedback
 *   - SMP per-CPU run queues
 *   - Real-time scheduling class
 */

#include "../../../include/types.h"
#include "../../../include/kernel.h"
#include "../../../include/process.h"
#include "../../../include/spinlock.h"

/* ============================================================
 * Process table and run queue
 * ============================================================ */

static process_t   proc_table[MAX_PROCESSES];
static process_t  *run_queue_head = NULL;   /* Front of READY queue */
static process_t  *current_proc   = NULL;   /* Currently running */
static spinlock_t  sched_lock     = SPINLOCK_INIT;
static uint32_t    next_pid       = 1;       /* PID allocator (0=idle) */

/* Kernel stack pool -- one stack per process slot */
static uint8_t kstacks[MAX_PROCESSES][KERNEL_STACK_SIZE] ALIGNED(16);

/* ============================================================
 * Context switch (defined in sched.asm)
 * ============================================================ */
extern void context_switch(cpu_context_t *old_ctx, cpu_context_t *new_ctx);

/* ============================================================
 * Run queue helpers
 * ============================================================ */

static void runq_push(process_t *p) {
    p->state = PROC_READY;
    p->next  = NULL;
    if (!run_queue_head) {
        run_queue_head = p;
        return;
    }
    /* Append to tail */
    process_t *t = run_queue_head;
    while (t->next) t = t->next;
    t->next = p;
}

static process_t *runq_pop(void) {
    if (!run_queue_head) return NULL;
    process_t *p   = run_queue_head;
    run_queue_head = p->next;
    p->next        = NULL;
    return p;
}

/* ============================================================
 * Idle process -- runs when nothing else is READY
 * Just halts the CPU until the next IRQ fires
 * ============================================================ */
static void idle_process(void) {
    for (;;) {
        sti();
        hlt();
        cli();
        sched_yield();
    }
}

/* ============================================================
 * sched_init -- Create idle process and initialize scheduler
 * ============================================================ */
void sched_init(void) {
    /* Zero process table */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        uint8_t *p = (uint8_t *)&proc_table[i];
        for (size_t j = 0; j < sizeof(process_t); j++) p[j] = 0;
    }

    /* Create idle process (PID 0) -- runs on this current stack */
    process_t *idle = &proc_table[0];
    idle->pid          = 0;
    idle->ppid         = 0;
    idle->state        = PROC_RUNNING;
    idle->priority     = 31;   /* Lowest priority */
    idle->time_slice_ms = 1;
    idle->kstack       = kstacks[0];
    idle->kstack_top   = (uint64_t)(kstacks[0] + KERNEL_STACK_SIZE);

    /* Copy name */
    const char *idlename = "[idle]";
    for (int i = 0; idlename[i] && i < PROCESS_NAME_LEN-1; i++)
        idle->name[i] = idlename[i];

    current_proc = idle;

    kprintf("[SCHED] Scheduler initialized. Idle process PID=0\n");
}

/* ============================================================
 * proc_create_kernel -- Create a new kernel-mode process
 * Allocates a PCB slot and sets up the initial stack frame
 * so context_switch() can start it cleanly.
 * ============================================================ */
process_t *proc_create_kernel(const char *name, void (*entry)(void),
                               uint32_t priority) {
    spinlock_acquire(&sched_lock);

    /* Find free slot */
    int slot = -1;
    for (int i = 1; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_UNUSED) { slot = i; break; }
    }
    if (slot == -1) {
        spinlock_release(&sched_lock);
        kprintf("[SCHED] ERROR: process table full\n");
        return NULL;
    }

    process_t *p = &proc_table[slot];
    /* Zero the PCB */
    uint8_t *pb = (uint8_t *)p;
    for (size_t i = 0; i < sizeof(process_t); i++) pb[i] = 0;

    p->pid           = next_pid++;
    p->ppid          = current_proc ? current_proc->pid : 0;
    p->priority      = priority;
    p->time_slice_ms = 10;   /* Default 10ms slice */
    p->ticks_slice   = 10;
    p->kstack        = kstacks[slot];
    p->kstack_top    = (uint64_t)(kstacks[slot] + KERNEL_STACK_SIZE);

    /* Copy name */
    for (int i = 0; name[i] && i < PROCESS_NAME_LEN-1; i++)
        p->name[i] = name[i];

    /*
     * Set up initial kernel stack so context_switch() works.
     * We push a fake return address pointing to proc_exit(),
     * so if the entry function ever returns, the process exits cleanly.
     * Stack grows downward: we write from the top.
     *
     * Stack layout when context_switch restores this process:
     *   [rsp+0] = entry function address  (context.rip)
     *   Stack looks like entry() was just called with proc_exit as caller
     */
    uint64_t *stk = (uint64_t *)(kstacks[slot] + KERNEL_STACK_SIZE);
    *--stk = (uint64_t)(uintptr_t)proc_exit; /* Return address if entry() returns */
    *--stk = (uint64_t)(uintptr_t)entry;     /* Initial RIP -- where to start */

    p->context.rsp = (uint64_t)(uintptr_t)stk;
    p->context.rip = (uint64_t)(uintptr_t)entry;

    /* Add to run queue */
    runq_push(p);

    spinlock_release(&sched_lock);

    kprintf("[SCHED] Created process '%s' PID=%u priority=%u\n",
            p->name, p->pid, p->priority);
    return p;
}

/* ============================================================
 * schedule -- Pick next process and switch to it
 * Called with interrupts disabled
 * ============================================================ */
static void schedule(void) {
    process_t *prev = current_proc;
    process_t *next = runq_pop();

    if (!next) {
        /* Nothing ready -- keep running current (or idle) */
        return;
    }

    /* If current was running (not blocked/sleeping), re-queue it */
    if (prev && prev->state == PROC_RUNNING) {
        prev->state = PROC_READY;
        runq_push(prev);
    }

    next->state       = PROC_RUNNING;
    next->ticks_slice = next->time_slice_ms;
    current_proc      = next;

    /* Perform the actual context switch */
    context_switch(&prev->context, &next->context);
    /* After context_switch returns, we are back in prev's context */
}

/* ============================================================
 * sched_tick -- Called from IRQ0 timer handler every 1ms
 * Decrements current time slice; triggers preemption when expired
 * ============================================================ */
void sched_tick(void) {
    if (!current_proc) return;

    /* Wake sleeping processes whose alarm has expired */
    uint64_t now = timer_ticks();
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t *p = &proc_table[i];
        if (p->state == PROC_SLEEPING && now >= p->sleep_until) {
            p->state = PROC_READY;
            runq_push(p);
        }
    }

    current_proc->ticks_used++;
    if (current_proc->ticks_slice > 0)
        current_proc->ticks_slice--;

    if (current_proc->ticks_slice == 0)
        schedule();
}

/* ============================================================
 * sched_yield -- Voluntarily give up the CPU
 * ============================================================ */
void sched_yield(void) {
    cli();
    if (current_proc)
        current_proc->ticks_slice = 0;
    schedule();
    sti();
}

/* ============================================================
 * proc_exit -- Terminate calling process
 * ============================================================ */
void proc_exit(int code) {
    cli();
    current_proc->state     = PROC_ZOMBIE;
    current_proc->exit_code = code;
    kprintf("[SCHED] Process '%s' PID=%u exited with code %d\n",
            current_proc->name, current_proc->pid, code);
    schedule();
    /* Never reached */
    halt_forever();
}

/* ============================================================
 * proc_block / proc_unblock
 * ============================================================ */
void proc_block(process_t *p) {
    cli();
    p->state = PROC_BLOCKED;
    if (p == current_proc) schedule();
    sti();
}

void proc_unblock(process_t *p) {
    cli();
    if (p->state == PROC_BLOCKED) runq_push(p);
    sti();
}

/* ============================================================
 * proc_sleep_ms -- Sleep current process
 * ============================================================ */
void proc_sleep_ms(uint32_t ms) {
    cli();
    current_proc->sleep_until = timer_ticks() + ms;
    current_proc->state       = PROC_SLEEPING;
    schedule();
    sti();
}

/* ============================================================
 * Accessors
 * ============================================================ */
process_t *proc_current(void) { return current_proc; }

process_t *proc_find(uint32_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++)
        if (proc_table[i].pid == pid && proc_table[i].state != PROC_UNUSED)
            return &proc_table[i];
    return NULL;
}

static const char *state_name(proc_state_t s) {
    switch (s) {
        case PROC_UNUSED:   return "UNUSED";
        case PROC_RUNNING:  return "RUNNING";
        case PROC_READY:    return "READY";
        case PROC_BLOCKED:  return "BLOCKED";
        case PROC_SLEEPING: return "SLEEPING";
        case PROC_ZOMBIE:   return "ZOMBIE";
        default:            return "?";
    }
}

void proc_list(void) {
    kprintf("PID  PPID  PRI  TICKS      STATE     NAME\n");
    kprintf("---  ----  ---  ---------  --------  ----\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_t *p = &proc_table[i];
        if (p->state == PROC_UNUSED) continue;
        kprintf("%3u  %4u  %3u  %9u  %-8s  %s\n",
                p->pid, p->ppid, p->priority,
                (uint32_t)p->ticks_used,
                state_name(p->state),
                p->name);
    }
}
