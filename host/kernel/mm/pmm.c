/*
 * bubblyVM -- Physical Memory Manager (Full)
 * File: host/kernel/mm/pmm.c
 *
 * Replaces the Tier 0 stub with a real bitmap-based page frame allocator.
 *
 * Design:
 *   - Divide all usable RAM into 4KB page frames
 *   - Maintain a bitmap: 1 bit per page (1=free, 0=used)
 *   - Bitmap itself stored in the first usable RAM region
 *   - alloc_page(): find first free bit, mark used, return physical addr
 *   - free_page():  mark bit free
 *
 * Complexity:
 *   alloc_page: O(n/64) -- scans 64-bit words for a non-zero word
 *   free_page:  O(1)
 *
 * Limitations (Tier 1):
 *   - No NUMA awareness
 *   - No DMA zone separation
 *   - Max RAM: bitmap fits in first 2MB region
 *   These are addressed in Tier 2 when we add a buddy allocator.
 */

#include "../../include/types.h"
#include "../../include/kernel.h"
#include "../../include/mm.h"

/* Multiboot2 types (shared with kernel.c -- move to mb2.h in Tier 2) */
typedef struct { uint32_t total_size; uint32_t reserved; } PACKED mb2_info_t;
typedef struct { uint32_t type; uint32_t size; } PACKED mb2_tag_t;
typedef struct {
    uint32_t type, size, entry_size, entry_version;
} PACKED mb2_mmap_tag_t;
typedef struct {
    uint64_t base_addr, length;
    uint32_t type, reserved;
} PACKED mb2_mmap_entry_t;

/* ============================================================
 * Bitmap storage
 * We place the bitmap at a known high address after the kernel.
 * Max 4GB RAM / 4KB pages = 1M bits = 128KB bitmap.
 * We reserve 256KB to be safe.
 * ============================================================ */
#define BITMAP_MAX_PAGES  (256 * 1024 * 8)   /* 256KB bitmap = 2M pages = 8GB */

static uint64_t *bitmap     = NULL;   /* Pointer to bitmap (set during init) */
static uint64_t  total_pages_count = 0;
static uint64_t  free_pages_count  = 0;
static uint64_t  bitmap_pages = 0;   /* Pages consumed by the bitmap itself */

/* Physical address where usable RAM starts (skip first MB) */
#define USABLE_RAM_START  0x100000ULL  /* 1MB -- below this is BIOS/reserved */

/* ============================================================
 * Bitmap helpers
 * ============================================================ */
static inline void bitmap_set_free(uint64_t page_idx) {
    bitmap[page_idx / 64] |=  (1ULL << (page_idx % 64));
}
static inline void bitmap_set_used(uint64_t page_idx) {
    bitmap[page_idx / 64] &= ~(1ULL << (page_idx % 64));
}
static inline int bitmap_is_free(uint64_t page_idx) {
    return !!(bitmap[page_idx / 64] & (1ULL << (page_idx % 64)));
}

/* ============================================================
 * pmm_init_full -- Parse Multiboot2 map, build bitmap
 * ============================================================ */
void pmm_init_full(uint32_t mb2_info_addr) {
    if (!mb2_info_addr) {
        kprintf("[PMM] FATAL: No Multiboot2 info\n");
        kpanic("PMM: no memory map");
    }

    /* Step 1: Find the largest usable region to place our bitmap */
    mb2_tag_t *tag = (mb2_tag_t *)(uintptr_t)(mb2_info_addr + 8);
    uint64_t best_base = 0, best_len = 0;

    while (tag->type != 0) {
        if (tag->type == 6) {
            mb2_mmap_tag_t  *mt = (mb2_mmap_tag_t *)tag;
            uint32_t count = (tag->size - sizeof(mb2_mmap_tag_t)) / mt->entry_size;
            mb2_mmap_entry_t *e = (mb2_mmap_entry_t *)((uintptr_t)tag + sizeof(mb2_mmap_tag_t));
            for (uint32_t i = 0; i < count; i++, e++) {
                if (e->type == 1 && e->base_addr >= USABLE_RAM_START
                    && e->length > best_len) {
                    best_base = e->base_addr;
                    best_len  = e->length;
                }
            }
        }
        uint32_t next = (uint32_t)((uintptr_t)tag + tag->size);
        tag = (mb2_tag_t *)(uintptr_t)((next + 7) & ~7U);
    }

    if (!best_base) kpanic("PMM: no usable RAM found above 1MB");

    /* Place bitmap at start of best region, page-aligned */
    bitmap = (uint64_t *)(uintptr_t)PAGE_ALIGN_UP(best_base);
    uint64_t bitmap_size_bytes = (best_len / PAGE_SIZE / 8) + 1;
    bitmap_pages = PAGE_ALIGN_UP(bitmap_size_bytes) / PAGE_SIZE;

    /* Zero the bitmap (mark all pages used initially) */
    uint8_t *bm = (uint8_t *)bitmap;
    for (uint64_t i = 0; i < bitmap_size_bytes; i++) bm[i] = 0;

    /* Step 2: Walk map again, mark usable regions as free */
    tag = (mb2_tag_t *)(uintptr_t)(mb2_info_addr + 8);
    while (tag->type != 0) {
        if (tag->type == 6) {
            mb2_mmap_tag_t  *mt = (mb2_mmap_tag_t *)tag;
            uint32_t count = (tag->size - sizeof(mb2_mmap_tag_t)) / mt->entry_size;
            mb2_mmap_entry_t *e = (mb2_mmap_entry_t *)((uintptr_t)tag + sizeof(mb2_mmap_tag_t));
            for (uint32_t i = 0; i < count; i++, e++) {
                if (e->type != 1) continue;
                if (e->base_addr < USABLE_RAM_START) continue;
                uint64_t start = PAGE_ALIGN_UP(e->base_addr);
                uint64_t end   = PAGE_ALIGN_DOWN(e->base_addr + e->length);
                for (uint64_t p = start; p < end; p += PAGE_SIZE) {
                    uint64_t idx = p / PAGE_SIZE;
                    if (idx < BITMAP_MAX_PAGES) {
                        bitmap_set_free(idx);
                        total_pages_count++;
                        free_pages_count++;
                    }
                }
            }
        }
        uint32_t next = (uint32_t)((uintptr_t)tag + tag->size);
        tag = (mb2_tag_t *)(uintptr_t)((next + 7) & ~7U);
    }

    /* Step 3: Mark the bitmap's own pages as used */
    uint64_t bm_phys = (uint64_t)(uintptr_t)bitmap;
    for (uint64_t i = 0; i < bitmap_pages; i++) {
        uint64_t idx = (bm_phys + i * PAGE_SIZE) / PAGE_SIZE;
        if (bitmap_is_free(idx)) {
            bitmap_set_used(idx);
            free_pages_count--;
        }
    }

    kprintf("[PMM] Bitmap @ 0x%x (%u pages reserved)\n",
            (uint64_t)(uintptr_t)bitmap, (uint64_t)bitmap_pages);
    kprintf("[PMM] Total pages: %u  Free pages: %u  (%u MB free)\n",
            total_pages_count, free_pages_count,
            (free_pages_count * PAGE_SIZE) / MB);
}

/* ============================================================
 * pmm_alloc_page -- Allocate one 4KB physical page
 * Returns physical address, or 0 on OOM
 * ============================================================ */
uint64_t pmm_alloc_page(void) {
    uint64_t words = (total_pages_count + 63) / 64;
    for (uint64_t w = 0; w < words; w++) {
        if (bitmap[w] == 0) continue;   /* All used in this word */
        /* Find first set bit */
        uint64_t bit = __builtin_ctzll(bitmap[w]);
        uint64_t idx = w * 64 + bit;
        bitmap_set_used(idx);
        free_pages_count--;
        return idx * PAGE_SIZE;
    }
    return 0;   /* Out of memory */
}

void pmm_free_page(uint64_t paddr) {
    uint64_t idx = paddr / PAGE_SIZE;
    if (!bitmap_is_free(idx)) {
        bitmap_set_free(idx);
        free_pages_count++;
    }
}

uint64_t pmm_free_pages(void)  { return free_pages_count;  }
uint64_t pmm_total_pages(void) { return total_pages_count; }
