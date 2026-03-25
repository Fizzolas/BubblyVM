/*
 * bubblyVM -- Interrupt Descriptor Table (IDT)
 * File: host/kernel/arch/x86_64/idt.c
 *
 * The IDT tells the CPU what function to call when any interrupt
 * or exception fires. There are 256 possible vectors:
 *
 *   Vectors  0-31:  CPU exceptions (divide by zero, page fault, etc.)
 *   Vectors 32-47:  Hardware IRQs from the PIC (remapped)
 *   Vectors 48-255: Available for software interrupts / future use
 *
 * Each IDT entry points to an interrupt stub in idt_stubs.asm.
 * The stub saves all registers, calls our C handler, then restores
 * and returns via IRETQ.
 *
 * CPU Exception Vector Reference:
 *   0  #DE  Divide-by-zero
 *   1  #DB  Debug
 *   2  --   Non-maskable interrupt (NMI)
 *   3  #BP  Breakpoint
 *   4  #OF  Overflow
 *   5  #BR  Bound range exceeded
 *   6  #UD  Invalid opcode
 *   7  #NM  Device not available (FPU)
 *   8  #DF  Double fault          (has error code)
 *   10 #TS  Invalid TSS           (has error code)
 *   11 #NP  Segment not present   (has error code)
 *   12 #SS  Stack fault           (has error code)
 *   13 #GP  General protection    (has error code)
 *   14 #PF  Page fault            (has error code)
 *   16 #MF  x87 FPU error
 *   17 #AC  Alignment check       (has error code)
 *   18 #MC  Machine check
 *   19 #XF  SIMD FPU exception
 */

#include "../../../include/types.h"
#include "../../../include/kernel.h"

/* ============================================================
 * IDT entry (gate descriptor) -- 16 bytes in 64-bit mode
 * ============================================================ */
typedef struct {
    uint16_t offset_low;    /* Bits 0-15 of handler address */
    uint16_t selector;      /* Code segment selector (0x08 = kernel CS) */
    uint8_t  ist;           /* Interrupt Stack Table index (0 = use RSP0) */
    uint8_t  type_attr;     /* Gate type + DPL + present bit */
    uint16_t offset_mid;    /* Bits 16-31 of handler address */
    uint32_t offset_high;   /* Bits 32-63 of handler address */
    uint32_t reserved;
} PACKED idt_entry_t;

/* IDTR register */
typedef struct {
    uint16_t limit;
    uint64_t base;
} PACKED idtr_t;

/* ============================================================
 * Interrupt frame pushed by CPU + our stub onto the stack
 * ============================================================ */
typedef struct {
    /* Saved general purpose registers (pushed by stub) */
    uint64_t r15, r14, r13, r12;
    uint64_t r11, r10, r9,  r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    /* Pushed by our stub: vector number and error code placeholder */
    uint64_t vector;
    uint64_t error_code;
    /* Pushed automatically by CPU on interrupt */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} PACKED interrupt_frame_t;

/* ============================================================
 * Gate type constants
 * ============================================================ */
#define IDT_INTERRUPT_GATE  0x8E  /* Present, DPL=0, 64-bit interrupt gate */
#define IDT_TRAP_GATE       0x8F  /* Same but doesn't clear IF flag */
#define IDT_USER_GATE       0xEE  /* Present, DPL=3, for int 0x80 syscall */

/* ============================================================
 * Static storage
 * ============================================================ */
#define IDT_ENTRIES 256

static idt_entry_t idt[IDT_ENTRIES];
static idtr_t      idtr;

/* ============================================================
 * External stub declarations (defined in idt_stubs.asm)
 * One stub per vector -- saves registers, calls idt_dispatch()
 * ============================================================ */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);
/* Hardware IRQs remapped to vectors 32-47 */
extern void isr32(void); extern void isr33(void); extern void isr34(void);
extern void isr35(void); extern void isr36(void); extern void isr37(void);
extern void isr38(void); extern void isr39(void); extern void isr40(void);
extern void isr41(void); extern void isr42(void); extern void isr43(void);
extern void isr44(void); extern void isr45(void); extern void isr46(void);
extern void isr47(void);

/* ============================================================
 * Helper: set one IDT gate
 * ============================================================ */
static void idt_set_gate(uint8_t vector, void (*handler)(void),
                          uint8_t type_attr, uint8_t ist) {
    uint64_t addr = (uint64_t)(uintptr_t)handler;
    idt[vector].offset_low  = (uint16_t)(addr & 0xFFFF);
    idt[vector].selector    = 0x08;   /* Kernel code segment */
    idt[vector].ist         = ist;
    idt[vector].type_attr   = type_attr;
    idt[vector].offset_mid  = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)(addr >> 32);
    idt[vector].reserved    = 0;
}

/* ============================================================
 * Exception names for panic messages
 * ============================================================ */
static const char *exception_names[32] = {
    "Divide-by-Zero (#DE)",
    "Debug (#DB)",
    "Non-Maskable Interrupt",
    "Breakpoint (#BP)",
    "Overflow (#OF)",
    "Bound Range Exceeded (#BR)",
    "Invalid Opcode (#UD)",
    "Device Not Available (#NM)",
    "Double Fault (#DF)",
    "Coprocessor Segment Overrun",
    "Invalid TSS (#TS)",
    "Segment Not Present (#NP)",
    "Stack Fault (#SS)",
    "General Protection Fault (#GP)",
    "Page Fault (#PF)",
    "Reserved (15)",
    "x87 FPU Error (#MF)",
    "Alignment Check (#AC)",
    "Machine Check (#MC)",
    "SIMD FPU Exception (#XF)",
    "Virtualization Exception (#VE)",
    "Control Protection (#CP)",
    "Reserved (22)", "Reserved (23)",
    "Reserved (24)", "Reserved (25)",
    "Reserved (26)", "Reserved (27)",
    "Reserved (28)", "Reserved (29)",
    "Reserved (30)", "Reserved (31)",
};

/* ============================================================
 * idt_dispatch -- called from every ISR stub
 * This is the single C entry point for all interrupts/exceptions
 * ============================================================ */
void idt_dispatch(interrupt_frame_t *frame) {
    uint64_t vec = frame->vector;

    if (vec < 32) {
        /* ---- CPU Exception ---- */
        kprintf("\n[EXCEPTION] %s\n", exception_names[vec]);
        kprintf("  Vector:     %u\n", vec);
        kprintf("  Error code: 0x%x\n", frame->error_code);
        kprintf("  RIP:        0x%x\n", frame->rip);
        kprintf("  CS:         0x%x\n", frame->cs);
        kprintf("  RFLAGS:     0x%x\n", frame->rflags);
        kprintf("  RSP:        0x%x\n", frame->rsp);
        kprintf("  RAX: 0x%x  RBX: 0x%x  RCX: 0x%x\n",
                frame->rax, frame->rbx, frame->rcx);
        kprintf("  RDX: 0x%x  RSI: 0x%x  RDI: 0x%x\n",
                frame->rdx, frame->rsi, frame->rdi);

        if (vec == 14) {
            /* Page fault: CR2 holds the faulting address */
            uint64_t cr2;
            __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
            kprintf("  CR2 (fault addr): 0x%x\n", cr2);
            kprintf("  PF flags: %s %s %s\n",
                    (frame->error_code & 1) ? "Protection" : "Not-present",
                    (frame->error_code & 2) ? "Write" : "Read",
                    (frame->error_code & 4) ? "User" : "Kernel");
        }

        kpanic("Unhandled CPU exception -- system halted");

    } else if (vec < 48) {
        /* ---- Hardware IRQ (32-47) ---- */
        uint8_t irq = (uint8_t)(vec - 32);

        /* Dispatch to registered IRQ handlers (Tier 1: timer is IRQ0) */
        extern void irq_dispatch(uint8_t irq);
        irq_dispatch(irq);

    } else {
        /* ---- Unknown / unhandled vector ---- */
        kprintf("[INT] Unhandled vector %u -- ignoring\n", vec);
    }
}

/* ============================================================
 * idt_init -- Build and load the IDT
 * Called once early in kernel_main_64()
 * ============================================================ */
void idt_init(void) {
    /* Zero the entire table first */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        uint8_t *p = (uint8_t *)&idt[i];
        for (size_t j = 0; j < sizeof(idt_entry_t); j++) p[j] = 0;
    }

    /* CPU exception handlers (vectors 0-31) */
    idt_set_gate(0,  isr0,  IDT_TRAP_GATE,      0);
    idt_set_gate(1,  isr1,  IDT_TRAP_GATE,      0);
    idt_set_gate(2,  isr2,  IDT_INTERRUPT_GATE, 0);
    idt_set_gate(3,  isr3,  IDT_TRAP_GATE,      0);
    idt_set_gate(4,  isr4,  IDT_TRAP_GATE,      0);
    idt_set_gate(5,  isr5,  IDT_TRAP_GATE,      0);
    idt_set_gate(6,  isr6,  IDT_TRAP_GATE,      0);
    idt_set_gate(7,  isr7,  IDT_TRAP_GATE,      0);
    idt_set_gate(8,  isr8,  IDT_INTERRUPT_GATE, 0); /* Double fault */
    idt_set_gate(9,  isr9,  IDT_TRAP_GATE,      0);
    idt_set_gate(10, isr10, IDT_TRAP_GATE,      0);
    idt_set_gate(11, isr11, IDT_TRAP_GATE,      0);
    idt_set_gate(12, isr12, IDT_TRAP_GATE,      0);
    idt_set_gate(13, isr13, IDT_TRAP_GATE,      0); /* GPF */
    idt_set_gate(14, isr14, IDT_TRAP_GATE,      0); /* Page fault */
    idt_set_gate(15, isr15, IDT_TRAP_GATE,      0);
    idt_set_gate(16, isr16, IDT_TRAP_GATE,      0);
    idt_set_gate(17, isr17, IDT_TRAP_GATE,      0);
    idt_set_gate(18, isr18, IDT_TRAP_GATE,      0);
    idt_set_gate(19, isr19, IDT_TRAP_GATE,      0);
    idt_set_gate(20, isr20, IDT_TRAP_GATE,      0);
    idt_set_gate(21, isr21, IDT_TRAP_GATE,      0);
    idt_set_gate(22, isr22, IDT_TRAP_GATE,      0);
    idt_set_gate(23, isr23, IDT_TRAP_GATE,      0);
    idt_set_gate(24, isr24, IDT_TRAP_GATE,      0);
    idt_set_gate(25, isr25, IDT_TRAP_GATE,      0);
    idt_set_gate(26, isr26, IDT_TRAP_GATE,      0);
    idt_set_gate(27, isr27, IDT_TRAP_GATE,      0);
    idt_set_gate(28, isr28, IDT_TRAP_GATE,      0);
    idt_set_gate(29, isr29, IDT_TRAP_GATE,      0);
    idt_set_gate(30, isr30, IDT_TRAP_GATE,      0);
    idt_set_gate(31, isr31, IDT_TRAP_GATE,      0);

    /* Hardware IRQ handlers (vectors 32-47) */
    idt_set_gate(32, isr32, IDT_INTERRUPT_GATE, 0);
    idt_set_gate(33, isr33, IDT_INTERRUPT_GATE, 0);
    idt_set_gate(34, isr34, IDT_INTERRUPT_GATE, 0);
    idt_set_gate(35, isr35, IDT_INTERRUPT_GATE, 0);
    idt_set_gate(36, isr36, IDT_INTERRUPT_GATE, 0);
    idt_set_gate(37, isr37, IDT_INTERRUPT_GATE, 0);
    idt_set_gate(38, isr38, IDT_INTERRUPT_GATE, 0);
    idt_set_gate(39, isr39, IDT_INTERRUPT_GATE, 0);
    idt_set_gate(40, isr40, IDT_INTERRUPT_GATE, 0);
    idt_set_gate(41, isr41, IDT_INTERRUPT_GATE, 0);
    idt_set_gate(42, isr42, IDT_INTERRUPT_GATE, 0);
    idt_set_gate(43, isr43, IDT_INTERRUPT_GATE, 0);
    idt_set_gate(44, isr44, IDT_INTERRUPT_GATE, 0);
    idt_set_gate(45, isr45, IDT_INTERRUPT_GATE, 0);
    idt_set_gate(46, isr46, IDT_INTERRUPT_GATE, 0);
    idt_set_gate(47, isr47, IDT_INTERRUPT_GATE, 0);

    /* Load IDTR */
    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base  = (uint64_t)(uintptr_t)idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr) : "memory");
}
