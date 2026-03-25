/*
 * bubblyVM -- Global Descriptor Table (GDT)
 * File: host/kernel/arch/x86_64/gdt.c
 *
 * The GDT tells the CPU about memory segments: which ranges of memory
 * exist, what privilege level can access them, and whether they hold
 * code or data.
 *
 * In 64-bit long mode, segmentation is mostly disabled -- the CPU
 * ignores base/limit fields and uses flat 64-bit addressing.
 * However, we MUST still have a valid GDT loaded with correct
 * descriptor types or the CPU will triple-fault.
 *
 * Our GDT layout:
 *   [0] Null descriptor       (required -- CPU rejects index 0)
 *   [1] Kernel code segment   (ring 0, 64-bit, execute/read)
 *   [2] Kernel data segment   (ring 0, read/write)
 *   [3] User code segment     (ring 3, 64-bit, execute/read)
 *   [4] User data segment     (ring 3, read/write)
 *   [5] TSS descriptor low    (Task State Segment -- for syscall stack)
 *   [6] TSS descriptor high   (64-bit TSS needs two 8-byte slots)
 *
 * Segment selector values (index * 8):
 *   Kernel CS = 0x08
 *   Kernel DS = 0x10
 *   User   CS = 0x18 | 3  (RPL=3)
 *   User   DS = 0x20 | 3
 *   TSS       = 0x28
 */

#include "../../../include/types.h"
#include "../../../include/kernel.h"

/* ============================================================
 * GDT entry (descriptor) format
 * Each entry is 8 bytes packed
 * ============================================================ */
typedef struct {
    uint16_t limit_low;     /* Bits 0-15 of segment limit */
    uint16_t base_low;      /* Bits 0-15 of base address */
    uint8_t  base_mid;      /* Bits 16-23 of base address */
    uint8_t  access;        /* Access byte: present, DPL, type */
    uint8_t  granularity;   /* Flags (4 bits) + limit high (4 bits) */
    uint8_t  base_high;     /* Bits 24-31 of base address */
} PACKED gdt_entry_t;

/* 64-bit TSS descriptor is 16 bytes (two 8-byte slots) */
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;    /* Bits 32-63 of base (64-bit extension) */
    uint32_t reserved;
} PACKED gdt_tss_entry_t;

/* GDTR: the register that points to our GDT */
typedef struct {
    uint16_t limit;         /* Size of GDT in bytes minus 1 */
    uint64_t base;          /* Linear address of GDT */
} PACKED gdtr_t;

/* ============================================================
 * Task State Segment (TSS)
 * Minimal TSS -- only RSP0 (kernel stack pointer for ring-0 entry)
 * is needed at Tier 0. Expanded later for full syscall support.
 * ============================================================ */
typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;          /* Stack pointer for ring-0 (kernel) entry */
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];        /* Interrupt Stack Table entries */
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;    /* I/O permission bitmap offset */
} PACKED tss_t;

/* ============================================================
 * Static storage
 * ============================================================ */

#define GDT_ENTRIES 7

static gdt_entry_t  gdt[GDT_ENTRIES];
static gdt_tss_entry_t *tss_slot = (gdt_tss_entry_t *)&gdt[5];
static gdtr_t       gdtr;
static tss_t        kernel_tss;

/* Kernel stack for interrupt handling (16KB) */
static uint8_t kernel_stack[16 * 1024] ALIGNED(16);

/* ============================================================
 * Helper: build a standard GDT entry
 * ============================================================ */

static gdt_entry_t make_entry(uint32_t base, uint32_t limit,
                               uint8_t access, uint8_t flags) {
    gdt_entry_t e;
    e.limit_low  = (uint16_t)(limit & 0xFFFF);
    e.base_low   = (uint16_t)(base  & 0xFFFF);
    e.base_mid   = (uint8_t)((base  >> 16) & 0xFF);
    e.access     = access;
    e.granularity = ((flags & 0x0F) << 4) | ((limit >> 16) & 0x0F);
    e.base_high  = (uint8_t)((base  >> 24) & 0xFF);
    return e;
}

/* ============================================================
 * Access byte constants
 *
 * Bit 7:   Present (must be 1 for valid descriptor)
 * Bits 6-5: DPL (0=kernel, 3=user)
 * Bit 4:   Descriptor type (1=code/data, 0=system)
 * Bit 3:   Executable (1=code, 0=data)
 * Bit 2:   Direction/Conforming
 * Bit 1:   Readable/Writable
 * Bit 0:   Accessed (CPU sets this; we start at 0)
 * ============================================================ */

#define GDT_PRESENT     0x80
#define GDT_DPL0        0x00
#define GDT_DPL3        0x60
#define GDT_TYPE_SYS    0x00
#define GDT_TYPE_CODE   0x18    /* Executable + non-conforming */
#define GDT_TYPE_DATA   0x12    /* Writable data */
#define GDT_TSS_TYPE    0x09    /* 64-bit TSS, available */

/* Granularity/flags nibble (upper 4 bits of byte 6)
 * Bit 3 (0x8): Granularity (1 = limit in 4KB pages)
 * Bit 2 (0x4): Size (0 = 16-bit, 1 = 32-bit) -- 0 in 64-bit mode
 * Bit 1 (0x2): Long mode (1 = 64-bit code segment)
 * Bit 0 (0x1): Reserved */
#define GDT_LONG        0x02    /* 64-bit code segment */
#define GDT_32BIT       0x04    /* 32-bit segment (not used in 64-bit) */
#define GDT_GRAN_4K     0x08    /* Limit granularity = 4KB pages */

/* ============================================================
 * gdt_init -- Set up and load the GDT
 * Called once early in kernel_main_64()
 * ============================================================ */

void gdt_init(void) {
    /* [0] Null descriptor -- must be all zeros */
    gdt[0] = make_entry(0, 0, 0, 0);

    /* [1] Kernel code: ring 0, 64-bit, executable */
    gdt[1] = make_entry(0, 0xFFFFF,
                        GDT_PRESENT | GDT_DPL0 | GDT_TYPE_CODE,
                        GDT_LONG | GDT_GRAN_4K);

    /* [2] Kernel data: ring 0, read/write */
    gdt[2] = make_entry(0, 0xFFFFF,
                        GDT_PRESENT | GDT_DPL0 | GDT_TYPE_DATA,
                        GDT_GRAN_4K);

    /* [3] User code: ring 3, 64-bit, executable */
    gdt[3] = make_entry(0, 0xFFFFF,
                        GDT_PRESENT | GDT_DPL3 | GDT_TYPE_CODE,
                        GDT_LONG | GDT_GRAN_4K);

    /* [4] User data: ring 3, read/write */
    gdt[4] = make_entry(0, 0xFFFFF,
                        GDT_PRESENT | GDT_DPL3 | GDT_TYPE_DATA,
                        GDT_GRAN_4K);

    /* [5-6] TSS descriptor (16 bytes total, spans two 8-byte slots) */
    uint64_t tss_base  = (uint64_t)(uintptr_t)&kernel_tss;
    uint32_t tss_limit = sizeof(tss_t) - 1;

    tss_slot->limit_low  = (uint16_t)(tss_limit & 0xFFFF);
    tss_slot->base_low   = (uint16_t)(tss_base  & 0xFFFF);
    tss_slot->base_mid   = (uint8_t)((tss_base  >> 16) & 0xFF);
    tss_slot->access     = GDT_PRESENT | GDT_TSS_TYPE;
    tss_slot->granularity = (tss_limit >> 16) & 0x0F;
    tss_slot->base_high  = (uint8_t)((tss_base  >> 24) & 0xFF);
    tss_slot->base_upper = (uint32_t)(tss_base  >> 32);
    tss_slot->reserved   = 0;

    /* Set up TSS: point RSP0 to top of kernel interrupt stack */
    kernel_tss.rsp0 = (uint64_t)(uintptr_t)(kernel_stack + sizeof(kernel_stack));
    kernel_tss.iomap_base = sizeof(tss_t); /* No I/O permission bitmap */

    /* Load GDTR register */
    gdtr.limit = (uint16_t)(sizeof(gdt) - 1);
    gdtr.base  = (uint64_t)(uintptr_t)gdt;

    /* lgdt instruction loads the new GDT, then we reload segment registers */
    __asm__ volatile (
        "lgdt %0             \n"
        /* Reload CS via far return trick (can't mov to CS directly) */
        "pushq $0x08         \n"   /* Kernel code selector */
        "lea 1f(%%rip), %%rax\n"
        "pushq %%rax         \n"
        "lretq               \n"
        "1:                  \n"
        /* Reload data segment registers */
        "movw $0x10, %%ax    \n"   /* Kernel data selector */
        "movw %%ax, %%ds     \n"
        "movw %%ax, %%es     \n"
        "movw %%ax, %%fs     \n"
        "movw %%ax, %%gs     \n"
        "movw %%ax, %%ss     \n"
        /* Load TSS */
        "movw $0x28, %%ax    \n"   /* TSS selector (index 5 * 8) */
        "ltr %%ax            \n"
        : : "m"(gdtr) : "rax", "memory"
    );
}
