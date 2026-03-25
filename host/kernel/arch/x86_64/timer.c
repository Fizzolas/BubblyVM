/*
 * bubblyVM -- PIT Timer Driver (IRQ0)
 * File: host/kernel/arch/x86_64/timer.c
 *
 * The 8253/8254 Programmable Interval Timer (PIT) is the oldest
 * and most reliable timer source on x86 systems. It fires IRQ0
 * at a configurable rate derived from a 1.193182 MHz base clock.
 *
 * We program it to fire at 1000 Hz (every 1ms) for a 1ms system tick.
 * This gives us:
 *   - A monotonic tick counter (uptime in ms)
 *   - A basis for timer_sleep_ms()
 *   - Foundation for future scheduler time slicing
 *
 * PIT I/O ports:
 *   0x40: Channel 0 data (connected to IRQ0)
 *   0x41: Channel 1 data (historically RAM refresh -- unused)
 *   0x42: Channel 2 data (connected to PC speaker)
 *   0x43: Mode/command register
 *
 * The base frequency is 1193182 Hz.
 * Divisor = 1193182 / desired_hz
 * At 1000 Hz: divisor = 1193 (actual = 1193182/1193 = 1000.15 Hz, close enough)
 */

#include "../../../include/types.h"
#include "../../../include/kernel.h"

#define PIT_CHANNEL0    0x40
#define PIT_CMD         0x43
#define PIT_BASE_HZ     1193182UL

/* Monotonic tick counter -- incremented by IRQ0 handler */
static volatile uint64_t tick_count = 0;
static uint32_t ticks_per_ms = 1;  /* Updated in timer_init() */

/* ============================================================
 * IRQ0 handler -- called every timer tick
 * This runs in interrupt context -- keep it extremely short
 * ============================================================ */
static void timer_irq_handler(void) {
    tick_count++;
    /* Future: call scheduler here for preemptive multitasking */
}

/* ============================================================
 * timer_init -- Program the PIT and register IRQ0 handler
 * hz: desired tick frequency (recommend 1000 for 1ms resolution)
 * ============================================================ */
void timer_init(uint32_t hz) {
    if (hz == 0) hz = 1000;

    uint32_t divisor = (uint32_t)(PIT_BASE_HZ / hz);
    ticks_per_ms = hz / 1000;
    if (ticks_per_ms == 0) ticks_per_ms = 1;

    /*
     * PIT command byte:
     *   Bits 7-6: Channel select = 00 (channel 0)
     *   Bits 5-4: Access mode   = 11 (lobyte/hibyte)
     *   Bits 3-1: Mode          = 011 (square wave generator)
     *   Bit 0:    BCD/Binary    = 0 (binary)
     */
    outb(PIT_CMD, 0x36);

    /* Send divisor as low byte then high byte */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

    /* Register our IRQ0 handler and unmask it */
    extern void irq_register(uint8_t irq, void (*handler)(void));
    irq_register(0, timer_irq_handler);
    pic_unmask_irq(0);

    kprintf("[TIMER] PIT initialized at %u Hz (divisor %u)\n", hz, divisor);
}

/* Return current tick count (ticks since timer_init) */
uint64_t timer_ticks(void) {
    return tick_count;
}

/* Busy-wait for approximately ms milliseconds
 * Not suitable for use in interrupt context
 * Good enough for Tier 0 delays; replaced by sleep queue in Tier 1 */
void timer_sleep_ms(uint32_t ms) {
    uint64_t target = tick_count + (uint64_t)ms * ticks_per_ms;
    while (tick_count < target)
        __asm__ volatile ("pause");  /* Hint CPU we are spinning */
}
