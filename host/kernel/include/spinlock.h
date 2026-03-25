/*
 * bubblyVM -- Spinlock
 * File: host/kernel/include/spinlock.h
 *
 * Simple test-and-set spinlock for protecting shared kernel data.
 * Spinlocks are only safe when:
 *   - The critical section is very short (< a few hundred instructions)
 *   - Interrupts are disabled while holding the lock (to avoid deadlock
 *     if an IRQ tries to acquire the same lock)
 *
 * Usage:
 *   spinlock_t lock = SPINLOCK_INIT;
 *   spinlock_acquire(&lock);
 *   // ... critical section ...
 *   spinlock_release(&lock);
 */

#pragma once
#include "types.h"

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT { .locked = 0 }

static ALWAYS_INLINE void spinlock_acquire(spinlock_t *lock) {
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        while (lock->locked)
            __asm__ volatile ("pause"); /* Reduce bus contention while spinning */
    }
}

static ALWAYS_INLINE void spinlock_release(spinlock_t *lock) {
    __sync_lock_release(&lock->locked);
}
