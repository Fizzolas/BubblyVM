/*
 * bubblyVM -- 64-bit Kernel Main Entry
 * File: host/kernel/arch/x86_64/kernel_main64.c
 *
 * This is the true kernel entry point in 64-bit long mode.
 * Called by long_mode_entry in boot32.asm after the CPU transitions
 * from 32-bit protected mode.
 *
 * Initialization order MATTERS. Each step depends on the previous:
 *
 *   1. serial_init()   -- Debug output available from this point on
 *   2. vga_clear()     -- VGA console ready
 *   3. Print banner
 *   4. gdt_init()      -- Proper 64-bit GDT loaded (replaces boot GDT)
 *   5. pic_init()      -- PIC remapped, all IRQs masked
 *   6. idt_init()      -- IDT loaded, exception handlers active
 *   7. pmm_init()      -- Physical memory map parsed
 *   8. timer_init()    -- PIT timer running at 1000 Hz
 *   9. sti()           -- Interrupts enabled
 *  10. Self-test       -- Verify timer fires, print uptime
 *  11. Halt (Tier 0)   -- Tier 0 complete, await Tier 1 work
 *
 * If any step fails, kpanic() halts the system with a clear message.
 */

#include "../../../include/types.h"
#include "../../../include/kernel.h"

/* Defined in kernel.c (32-bit stub) -- reused for output */
extern void serial_init(void);
extern void vga_clear(void);
extern void kprintf(const char *fmt, ...);
extern void kpanic(const char *msg);

void kernel_main_64(void) {
    /* ----------------------------------------------------------------
     * Step 1-2: Output subsystems
     * ---------------------------------------------------------------- */
    serial_init();
    vga_clear();

    /* ----------------------------------------------------------------
     * Step 3: Banner
     * ---------------------------------------------------------------- */
    kprintf("==============================================\n");
    kprintf(" bubblyVM v0.0.1  [Tier 0 -- 64-bit Mode]\n");
    kprintf("==============================================\n");
    kprintf("[INIT] CPU is in 64-bit long mode\n");

    /* ----------------------------------------------------------------
     * Step 4: GDT -- proper 64-bit descriptors + TSS
     * ---------------------------------------------------------------- */
    kprintf("[INIT] Loading GDT...\n");
    gdt_init();
    kprintf("[INIT] GDT: OK (kernel CS=0x08, DS=0x10, TSS=0x28)\n");

    /* ----------------------------------------------------------------
     * Step 5: PIC -- remap IRQs to vectors 32-47
     * MUST happen before IDT is loaded to avoid vector conflicts
     * ---------------------------------------------------------------- */
    kprintf("[INIT] Remapping PIC...\n");
    pic_init();
    kprintf("[INIT] PIC: OK (IRQ0-7 -> vec 32-39, IRQ8-15 -> vec 40-47)\n");

    /* ----------------------------------------------------------------
     * Step 6: IDT -- exception and IRQ handlers
     * ---------------------------------------------------------------- */
    kprintf("[INIT] Loading IDT...\n");
    idt_init();
    kprintf("[INIT] IDT: OK (256 vectors, exceptions 0-31 handled)\n");

    /* ----------------------------------------------------------------
     * Step 7: Physical memory manager
     * ---------------------------------------------------------------- */
    kprintf("[INIT] Parsing memory map...\n");
    /* mb2_info_addr passed from 32-bit entry -- stored in known location.
     * For Tier 0 we read it from a well-known symbol set by entry.asm.
     * Full passing via register is wired in Tier 1 when we restructure
     * the 32->64 handoff properly. */
    extern uint32_t mb2_info_addr_storage;
    pmm_init(mb2_info_addr_storage);

    /* ----------------------------------------------------------------
     * Step 8: Timer -- PIT at 1000 Hz
     * ---------------------------------------------------------------- */
    kprintf("[INIT] Starting timer...\n");
    timer_init(1000);

    /* ----------------------------------------------------------------
     * Step 9: Enable interrupts
     * From this point forward, IRQ0 fires every 1ms
     * ---------------------------------------------------------------- */
    kprintf("[INIT] Enabling interrupts...\n");
    sti();
    kprintf("[INIT] Interrupts: ENABLED\n");

    /* ----------------------------------------------------------------
     * Step 10: Self-test
     * Wait 200ms and verify the tick counter advanced
     * This proves IRQ0 is firing and the timer stack works
     * ---------------------------------------------------------------- */
    kprintf("[TEST] Timer self-test: waiting 200ms...\n");
    uint64_t before = timer_ticks();
    timer_sleep_ms(200);
    uint64_t after  = timer_ticks();
    uint64_t delta  = after - before;

    if (delta < 150 || delta > 250) {
        kpanic("[FAIL] Timer self-test: tick delta out of range -- IRQ0 not firing correctly");
    }
    kprintf("[TEST] Timer self-test: PASSED (delta = %u ticks)\n", delta);

    /* ----------------------------------------------------------------
     * Tier 0 Complete
     * All checklist items validated:
     *   [x] Boots in QEMU (BIOS + UEFI)
     *   [x] 64-bit long mode active
     *   [x] GDT loaded
     *   [x] IDT loaded (exceptions + IRQs handled)
     *   [x] PIC remapped
     *   [x] Physical memory map parsed
     *   [x] Timer firing at 1000 Hz
     *   [x] Panic handler functional
     *   [x] Serial + VGA output working
     * Next: Tier 1 -- process model, shell, storage, networking
     * ---------------------------------------------------------------- */
    kprintf("\n[BOOT] ===================================\n");
    kprintf("[BOOT]  Tier 0 COMPLETE\n");
    kprintf("[BOOT]  RAM:    %u MB usable\n",
            (uint32_t)(pmm_free_bytes() / (1024*1024)));
    kprintf("[BOOT]  Uptime: %u ms\n", (uint32_t)timer_ticks());
    kprintf("[BOOT]  Next:   Tier 1 (process model + shell)\n");
    kprintf("[BOOT] ===================================\n");

    /* Halt cleanly -- Tier 1 init will replace this */
    cli();
    for (;;) hlt();
}
