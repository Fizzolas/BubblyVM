/*
 * bubblyVM -- Memory Manager Public Interface
 * File: host/kernel/include/mm.h
 *
 * Two layers:
 *   1. PMM (Physical Memory Manager) -- page frame allocator
 *      Tracks which 4KB physical pages are free/used.
 *      alloc_page() / free_page()
 *
 *   2. KMM (Kernel Memory Manager) -- heap allocator
 *      Built on top of PMM. Provides kmalloc/kfree for
 *      arbitrary-sized kernel allocations.
 */

#pragma once
#include "types.h"

#define PAGE_SIZE   4096ULL
#define PAGE_SHIFT  12
#define PAGE_MASK   (~(PAGE_SIZE - 1))

/* Align address UP to next page boundary */
#define PAGE_ALIGN_UP(x)   (((uint64_t)(x) + PAGE_SIZE - 1) & PAGE_MASK)
/* Align address DOWN to page boundary */
#define PAGE_ALIGN_DOWN(x) ((uint64_t)(x) & PAGE_MASK)

/* ============================================================
 * Physical Memory Manager (full version -- replaces Tier 0 stub)
 * ============================================================ */
void  pmm_init_full(uint32_t mb2_info_addr);
uint64_t pmm_alloc_page(void);          /* Returns physical address or 0 */
void     pmm_free_page(uint64_t paddr); /* Return page to free pool */
uint64_t pmm_free_pages(void);          /* Count of free pages */
uint64_t pmm_total_pages(void);         /* Total usable pages */

/* ============================================================
 * Kernel Heap (kmalloc / kfree)
 * ============================================================ */
void  kmalloc_init(void);
void *kmalloc(size_t size);
void *kzalloc(size_t size);   /* kmalloc + zero */
void  kfree(void *ptr);
void  kmalloc_stats(void);    /* Print heap stats */
