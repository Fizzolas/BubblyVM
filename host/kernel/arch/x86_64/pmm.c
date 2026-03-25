/*
 * bubblyVM -- Physical Memory Manager (Tier 0 Stub)
 * File: host/kernel/arch/x86_64/pmm.c
 *
 * At Tier 0, the physical memory manager does one job:
 * parse the Multiboot2 memory map and record total/free RAM.
 *
 * A full buddy allocator and page frame allocator are built in Tier 1.
 * This stub gives us correct memory information from day one so
 * kprintf() can report real numbers rather than hardcoded guesses.
 *
 * The memory map from GRUB lists every physical memory region and
 * its type. We sum up all type=1 (usable RAM) regions.
 */

#include "../../../include/types.h"
#include "../../../include/kernel.h"

/* Multiboot2 structures (same as kernel.c -- will be moved to a header) */
typedef struct { uint32_t total_size; uint32_t reserved; } PACKED mb2_info_t;
typedef struct { uint32_t type; uint32_t size; } PACKED mb2_tag_t;
typedef struct {
    uint32_t type; uint32_t size;
    uint32_t entry_size; uint32_t entry_version;
} PACKED mb2_mmap_tag_t;
typedef struct {
    uint64_t base_addr; uint64_t length;
    uint32_t type; uint32_t reserved;
} PACKED mb2_mmap_entry_t;

static uint64_t total_mem_bytes = 0;
static uint64_t free_mem_bytes  = 0;

/*
 * pmm_init -- Walk the Multiboot2 memory map and record memory totals.
 * This is the FIRST call in kernel_main_64() after GDT/IDT are ready.
 */
void pmm_init(uint32_t mb2_info_addr) {
    if (!mb2_info_addr) {
        kprintf("[PMM] WARNING: No Multiboot2 info -- cannot detect RAM\n");
        return;
    }

    mb2_info_t *info = (mb2_info_t *)(uintptr_t)mb2_info_addr;
    mb2_tag_t  *tag  = (mb2_tag_t  *)(uintptr_t)(mb2_info_addr + 8);

    while (tag->type != 0) {
        if (tag->type == 6) {  /* Memory map tag */
            mb2_mmap_tag_t  *mt    = (mb2_mmap_tag_t *)tag;
            uint32_t         count = (tag->size - sizeof(mb2_mmap_tag_t))
                                      / mt->entry_size;
            mb2_mmap_entry_t *e    = (mb2_mmap_entry_t *)(
                (uintptr_t)tag + sizeof(mb2_mmap_tag_t));

            for (uint32_t i = 0; i < count; i++, e++) {
                if (e->type == 1) {  /* Usable RAM */
                    free_mem_bytes  += e->length;
                }
                total_mem_bytes += e->length;
            }
        }
        uint32_t next = (uint32_t)((uintptr_t)tag + tag->size);
        next = (next + 7) & ~7U;
        tag  = (mb2_tag_t *)(uintptr_t)next;
    }

    kprintf("[PMM] Total RAM:   %u MB\n", (uint32_t)(total_mem_bytes / MB));
    kprintf("[PMM] Usable RAM:  %u MB\n", (uint32_t)(free_mem_bytes  / MB));
    kprintf("[PMM] NOTE: Full page allocator comes in Tier 1\n");
}

uint64_t pmm_total_bytes(void) { return total_mem_bytes; }
uint64_t pmm_free_bytes(void)  { return free_mem_bytes;  }
