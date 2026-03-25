/*
 * bubblyVM -- Core Type Definitions
 * File: host/kernel/include/types.h
 *
 * Provides fixed-width integer types and common macros for
 * all kernel code. Does NOT include any standard library headers.
 * Everything here is freestanding and works without libc.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================
 * Compiler hints
 * ============================================================ */

#define PACKED          __attribute__((packed))
#define NORETURN        __attribute__((noreturn))
#define ALWAYS_INLINE   __attribute__((always_inline)) inline
#define UNUSED          __attribute__((unused))
#define ALIGNED(n)      __attribute__((aligned(n)))

/* ============================================================
 * Common size constants
 * ============================================================ */

#define KB  (1024ULL)
#define MB  (1024ULL * KB)
#define GB  (1024ULL * MB)

/* ============================================================
 * Port I/O inline functions
 * Inline here so any file that includes types.h gets them
 * ============================================================ */

static ALWAYS_INLINE void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

static ALWAYS_INLINE void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

static ALWAYS_INLINE uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

static ALWAYS_INLINE uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

/* Small I/O delay -- used after PIC commands to let hardware settle */
static ALWAYS_INLINE void io_wait(void) {
    outb(0x80, 0x00);
}

/* ============================================================
 * Interrupt enable/disable
 * ============================================================ */

static ALWAYS_INLINE void cli(void) { __asm__ volatile ("cli" ::: "memory"); }
static ALWAYS_INLINE void sti(void) { __asm__ volatile ("sti" ::: "memory"); }
static ALWAYS_INLINE void hlt(void) { __asm__ volatile ("hlt"); }

/* Halt forever -- used in panic */
static ALWAYS_INLINE NORETURN void halt_forever(void) {
    cli();
    for (;;) hlt();
}
