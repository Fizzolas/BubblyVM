/*
 * bubblyVM -- 8259A Programmable Interrupt Controller (PIC)
 * File: host/kernel/arch/x86_64/pic.c
 *
 * The original IBM PC used two cascaded 8259A PIC chips to route
 * hardware interrupts (IRQs) to the CPU. On modern PCs, the APIC
 * replaces the 8259A, but the 8259A is still present in compatibility
 * mode and is what QEMU uses by default.
 *
 * PROBLEM: The 8259A maps IRQ0-7 to CPU vectors 8-15 by default.
 * CPU vectors 8-15 are also used by CPU exceptions (#DF, #TS, etc.).
 * This causes impossible-to-debug conflicts if we don't remap the PIC.
 *
 * SOLUTION: Remap IRQ0-7  to vectors 32-39
 *           Remap IRQ8-15 to vectors 40-47
 * Now hardware interrupts live above all CPU exception vectors.
 *
 * PIC I/O ports:
 *   Master PIC: command=0x20, data=0x21
 *   Slave  PIC: command=0xA0, data=0xA1
 */

#include "../../../include/types.h"
#include "../../../include/kernel.h"

#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1

/* Initialization Command Words */
#define PIC_ICW1_INIT   0x10    /* Start init sequence */
#define PIC_ICW1_ICW4   0x01    /* ICW4 will be sent */
#define PIC_ICW4_8086   0x01    /* 8086/88 mode */
#define PIC_EOI         0x20    /* End-of-interrupt command */

/* IRQ handler function pointer table */
#define IRQ_COUNT 16
typedef void (*irq_handler_t)(void);
static irq_handler_t irq_handlers[IRQ_COUNT];

/*
 * pic_init -- Remap and initialize both PIC chips
 * Must be called before idt_init() enables interrupts
 */
void pic_init(void) {
    /* Save current interrupt masks */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* Start initialization sequence (cascade mode) */
    outb(PIC1_CMD,  PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD,  PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();

    /* ICW2: Set interrupt vector offsets */
    outb(PIC1_DATA, 0x20);  /* Master: IRQ0-7 -> vectors 32-39 */
    io_wait();
    outb(PIC2_DATA, 0x28);  /* Slave:  IRQ8-15 -> vectors 40-47 */
    io_wait();

    /* ICW3: Tell master about slave on IRQ2, tell slave its cascade ID */
    outb(PIC1_DATA, 0x04);  /* Master: slave at IRQ2 (bit 2 = 0b100) */
    io_wait();
    outb(PIC2_DATA, 0x02);  /* Slave: cascade identity = 2 */
    io_wait();

    /* ICW4: Set 8086 mode */
    outb(PIC1_DATA, PIC_ICW4_8086);
    io_wait();
    outb(PIC2_DATA, PIC_ICW4_8086);
    io_wait();

    /* Restore saved masks (mask everything except IRQ0 timer initially) */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);

    /* Mask all IRQs by default -- subsystems unmask what they need */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    /* Zero handler table */
    for (int i = 0; i < IRQ_COUNT; i++)
        irq_handlers[i] = 0;
}

/*
 * pic_send_eoi -- Send End-of-Interrupt signal
 * Must be called at the END of every hardware IRQ handler
 * or the PIC will never send the next interrupt for that line
 */
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);  /* Slave PIC needs EOI too */
    outb(PIC1_CMD, PIC_EOI);
}

/* Mask (disable) a specific IRQ line */
void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t  bit;
    if (irq < 8) { port = PIC1_DATA; bit = irq; }
    else          { port = PIC2_DATA; bit = irq - 8; }
    outb(port, inb(port) | (1 << bit));
}

/* Unmask (enable) a specific IRQ line */
void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t  bit;
    if (irq < 8) { port = PIC1_DATA; bit = irq; }
    else          { port = PIC2_DATA; bit = irq - 8; }
    outb(port, inb(port) & ~(1 << bit));
}

/* Register a handler function for a specific IRQ */
void irq_register(uint8_t irq, irq_handler_t handler) {
    if (irq < IRQ_COUNT)
        irq_handlers[irq] = handler;
}

/* Called from idt_dispatch() for vectors 32-47 */
void irq_dispatch(uint8_t irq) {
    if (irq < IRQ_COUNT && irq_handlers[irq])
        irq_handlers[irq]();
    /* Always send EOI -- even if no handler registered */
    pic_send_eoi(irq);
}
