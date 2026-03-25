/*
 * bubblyVM -- Process Model Public Interface
 * File: host/kernel/include/process.h
 *
 * Defines the process control block (PCB), process states,
 * and all process/scheduler API functions.
 */

#pragma once
#include "types.h"

/* ============================================================
 * Constants
 * ============================================================ */
#define MAX_PROCESSES       64      /* Max simultaneous processes (Tier 1) */
#define KERNEL_STACK_SIZE   (16*1024)
#define USER_STACK_SIZE     (64*1024)
#define PROCESS_NAME_LEN    32

/* ============================================================
 * Process states
 * ============================================================ */
typedef enum {
    PROC_UNUSED   = 0,  /* Slot is free */
    PROC_RUNNING  = 1,  /* Currently executing on CPU */
    PROC_READY    = 2,  /* In run queue, waiting for CPU */
    PROC_BLOCKED  = 3,  /* Waiting for I/O or event */
    PROC_SLEEPING = 4,  /* timer_sleep_ms() -- wake at tick N */
    PROC_ZOMBIE   = 5,  /* Exited but parent hasn't waited */
} proc_state_t;

/* ============================================================
 * Saved CPU context (all registers needed for context switch)
 * Layout must match context_switch() in sched.asm exactly
 * ============================================================ */
typedef struct {
    uint64_t r15, r14, r13, r12;
    uint64_t rbx, rbp;
    uint64_t rsp;   /* Stack pointer -- restored to switch stacks */
    uint64_t rip;   /* Instruction pointer -- where to resume */
} cpu_context_t;

/* ============================================================
 * Process Control Block (PCB)
 * One per process -- the kernel's complete record of a process
 * ============================================================ */
typedef struct process {
    /* Identity */
    uint32_t        pid;                    /* Process ID */
    uint32_t        ppid;                   /* Parent PID */
    char            name[PROCESS_NAME_LEN]; /* Human-readable name */

    /* State */
    proc_state_t    state;
    int             exit_code;              /* Set when state=ZOMBIE */
    uint64_t        sleep_until;            /* Wake tick (SLEEPING state) */

    /* CPU context -- saved/restored on context switch */
    cpu_context_t   context;

    /* Kernel stack (allocated at process creation) */
    uint8_t        *kstack;                 /* Base of kernel stack */
    uint64_t        kstack_top;             /* Initial RSP value */

    /* Scheduling */
    uint32_t        priority;               /* 0=highest, 31=lowest */
    uint64_t        ticks_used;             /* Total CPU ticks consumed */
    uint64_t        ticks_slice;            /* Remaining ticks in current slice */
    uint32_t        time_slice_ms;          /* Base time slice in ms */

    /* Linked list for run queue */
    struct process *next;
} process_t;

/* ============================================================
 * Scheduler / Process API
 * ============================================================ */

/* Initialize scheduler, create idle process (PID 0) */
void sched_init(void);

/* Called from timer IRQ0 handler -- drives preemption */
void sched_tick(void);

/* Yield CPU voluntarily (cooperative yield) */
void sched_yield(void);

/* Create a new kernel-mode process */
process_t *proc_create_kernel(const char *name, void (*entry)(void),
                               uint32_t priority);

/* Terminate calling process with exit code */
void proc_exit(int code) NORETURN;

/* Block/unblock a process */
void proc_block(process_t *p);
void proc_unblock(process_t *p);

/* Sleep current process for N milliseconds */
void proc_sleep_ms(uint32_t ms);

/* Get current running process */
process_t *proc_current(void);

/* Get process by PID (returns NULL if not found) */
process_t *proc_find(uint32_t pid);

/* Print process list to serial/VGA */
void proc_list(void);
