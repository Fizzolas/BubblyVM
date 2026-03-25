/*
 * bubblyVM -- Kernel Public Interface
 * File: host/kernel/include/kernel.h
 *
 * Declares all core kernel subsystem functions that other
 * files can call. Acts as the main internal API header.
 */

#pragma once

#include "types.h"

/* ============================================================
 * Serial / console output (kernel.c)
 * ============================================================ */
void serial_init(void);
void serial_putchar(char c);
void serial_print(const char *str);
void vga_clear(void);
void vga_putchar(char c);
void vga_print(const char *str);
void kprintf(const char *fmt, ...);

/* ============================================================
 * Panic (kernel.c)
 * Never returns. Prints message, halts all CPUs.
 * ============================================================ */
void kpanic(const char *msg) NORETURN;

/* ============================================================
 * GDT (gdt.c)
 * Global Descriptor Table -- defines kernel/user memory segments
 * ============================================================ */
void gdt_init(void);

/* ============================================================
 * IDT (idt.c)
 * Interrupt Descriptor Table -- hooks all 256 CPU interrupt vectors
 * ============================================================ */
void idt_init(void);

/* ============================================================
 * PIC (pic.c)
 * 8259A Programmable Interrupt Controller
 * Remaps hardware IRQs away from CPU exception vectors
 * ============================================================ */
void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);

/* ============================================================
 * Timer (timer.c)
 * PIT (Programmable Interval Timer) -- drives the system tick
 * ============================================================ */
void timer_init(uint32_t hz);
uint64_t timer_ticks(void);
void timer_sleep_ms(uint32_t ms);

/* ============================================================
 * Physical memory manager (pmm.c) -- Tier 0 stub
 * ============================================================ */
void pmm_init(uint32_t mb2_info_addr);
uint64_t pmm_total_bytes(void);
uint64_t pmm_free_bytes(void);
