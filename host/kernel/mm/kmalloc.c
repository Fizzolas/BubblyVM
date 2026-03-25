/*
 * bubblyVM -- Kernel Heap Allocator
 * File: host/kernel/mm/kmalloc.c
 *
 * Provides kmalloc(size) / kfree(ptr) for arbitrary kernel allocations.
 *
 * Design: Free-list allocator with block headers
 *   - A fixed-size heap region is mapped from physical pages at init
 *   - Every allocation has a small header storing size + magic number
 *   - Free blocks are kept in a singly-linked free list
 *   - Allocation: first-fit search of free list
 *   - Freeing: prepend to free list, coalesce adjacent blocks
 *
 * Heap size: 4MB initial (expandable in Tier 2 with vmm)
 *
 * Why not a fancier allocator?
 *   At Tier 1, simplicity beats performance. The kernel doesn't do
 *   millions of small allocations. A free-list is debuggable and
 *   correct. Slab/buddy comes in Tier 2.
 *
 * Thread safety: spinlock around all operations.
 */

#include "../../include/types.h"
#include "../../include/kernel.h"
#include "../../include/mm.h"
#include "../../include/spinlock.h"

/* ============================================================
 * Heap configuration
 * ============================================================ */
#define HEAP_INITIAL_PAGES  1024          /* 4MB initial heap */
#define HEAP_SIZE           (HEAP_INITIAL_PAGES * PAGE_SIZE)
#define BLOCK_MAGIC_FREE    0xFEEDBEEFUL
#define BLOCK_MAGIC_USED    0xDEADC0DEUL
#define MIN_BLOCK_SIZE      32            /* Minimum allocation unit */
#define HEADER_SIZE         sizeof(block_header_t)

/* ============================================================
 * Block header -- sits immediately before every allocation
 * ============================================================ */
typedef struct block_header {
    uint32_t            magic;    /* BLOCK_MAGIC_FREE or BLOCK_MAGIC_USED */
    uint32_t            size;     /* Size of data region (not including header) */
    struct block_header *next;    /* Next block in free list (free blocks only) */
} block_header_t;

/* ============================================================
 * Heap storage
 * ============================================================ */
static uint8_t      heap_storage[HEAP_SIZE] ALIGNED(PAGE_SIZE);
static block_header_t *free_list = NULL;
static spinlock_t    heap_lock   = SPINLOCK_INIT;

/* Stats */
static uint64_t alloc_count = 0;
static uint64_t free_count  = 0;
static uint64_t bytes_used  = 0;

/* ============================================================
 * kmalloc_init -- Set up initial heap as one big free block
 * ============================================================ */
void kmalloc_init(void) {
    /* Zero the heap */
    for (size_t i = 0; i < HEAP_SIZE; i++) heap_storage[i] = 0;

    /* Create one free block covering the entire heap */
    free_list = (block_header_t *)heap_storage;
    free_list->magic = BLOCK_MAGIC_FREE;
    free_list->size  = HEAP_SIZE - HEADER_SIZE;
    free_list->next  = NULL;

    kprintf("[HEAP] Kernel heap: %u KB @ 0x%x\n",
            (uint64_t)(HEAP_SIZE / 1024),
            (uint64_t)(uintptr_t)heap_storage);
}

/* ============================================================
 * kmalloc -- Allocate size bytes, return aligned pointer
 * Returns NULL on failure.
 * ============================================================ */
void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    /* Round up to MIN_BLOCK_SIZE alignment */
    size = (size + MIN_BLOCK_SIZE - 1) & ~(size_t)(MIN_BLOCK_SIZE - 1);

    spinlock_acquire(&heap_lock);

    block_header_t *prev = NULL;
    block_header_t *curr = free_list;

    while (curr) {
        if (curr->magic != BLOCK_MAGIC_FREE) {
            spinlock_release(&heap_lock);
            kpanic("kmalloc: heap corruption detected (bad magic in free list)");
        }

        if (curr->size >= size) {
            /* Found a fitting block */
            uint32_t remaining = curr->size - (uint32_t)size;

            if (remaining > HEADER_SIZE + MIN_BLOCK_SIZE) {
                /* Split: carve off a new free block after this allocation */
                block_header_t *split = (block_header_t *)(
                    (uint8_t *)curr + HEADER_SIZE + size);
                split->magic = BLOCK_MAGIC_FREE;
                split->size  = remaining - HEADER_SIZE;
                split->next  = curr->next;

                /* Remove curr from free list, insert split */
                if (prev) prev->next = split;
                else       free_list = split;
            } else {
                /* Use entire block -- don't split (leftover too small) */
                if (prev) prev->next = curr->next;
                else       free_list = curr->next;
            }

            curr->magic = BLOCK_MAGIC_USED;
            curr->size  = (uint32_t)size;
            curr->next  = NULL;

            alloc_count++;
            bytes_used += size;

            spinlock_release(&heap_lock);
            return (void *)((uint8_t *)curr + HEADER_SIZE);
        }

        prev = curr;
        curr = curr->next;
    }

    spinlock_release(&heap_lock);
    kprintf("[HEAP] kmalloc(%u): OUT OF MEMORY\n", (uint64_t)size);
    return NULL;
}

/* ============================================================
 * kzalloc -- kmalloc + zero fill
 * ============================================================ */
void *kzalloc(size_t size) {
    void *p = kmalloc(size);
    if (p) {
        uint8_t *b = (uint8_t *)p;
        for (size_t i = 0; i < size; i++) b[i] = 0;
    }
    return p;
}

/* ============================================================
 * kfree -- Release a previously kmalloc'd pointer
 * Coalesces with adjacent free block if possible.
 * ============================================================ */
void kfree(void *ptr) {
    if (!ptr) return;

    block_header_t *hdr = (block_header_t *)((uint8_t *)ptr - HEADER_SIZE);

    spinlock_acquire(&heap_lock);

    if (hdr->magic != BLOCK_MAGIC_USED) {
        spinlock_release(&heap_lock);
        kpanic("kfree: double-free or heap corruption detected");
    }

    bytes_used  -= hdr->size;
    free_count++;

    hdr->magic = BLOCK_MAGIC_FREE;

    /* Prepend to free list */
    hdr->next  = free_list;
    free_list  = hdr;

    /* Coalesce: if the block immediately after us is also free, merge */
    block_header_t *next_blk = (block_header_t *)(
        (uint8_t *)hdr + HEADER_SIZE + hdr->size);
    uint8_t *heap_end = heap_storage + HEAP_SIZE;
    if ((uint8_t *)next_blk < heap_end && next_blk->magic == BLOCK_MAGIC_FREE) {
        /* Remove next_blk from free list */
        block_header_t *prev = NULL, *cur = free_list;
        while (cur && cur != next_blk) { prev = cur; cur = cur->next; }
        if (cur == next_blk) {
            if (prev) prev->next = cur->next;
            else       free_list = cur->next;
        }
        /* Merge */
        hdr->size += HEADER_SIZE + next_blk->size;
    }

    spinlock_release(&heap_lock);
}

/* ============================================================
 * kmalloc_stats -- Print heap usage
 * ============================================================ */
void kmalloc_stats(void) {
    kprintf("[HEAP] Allocs: %u  Frees: %u  In-use: %u bytes\n",
            alloc_count, free_count, bytes_used);
}
