/*
 * bubblyVM -- Kernel Linker Script
 * File: host/kernel/kernel.ld
 *
 * Tells the linker how to arrange the kernel binary sections.
 * The kernel is loaded at 1MB physical (0x100000) -- a safe address
 * above the first megabyte which is reserved for BIOS/firmware use.
 *
 * Section layout:
 *   .multiboot2  -- Multiboot2 header (must be in first 32KB)
 *   .text        -- Executable code
 *   .rodata      -- Read-only data (strings, constants)
 *   .data        -- Initialized global/static variables
 *   .bss         -- Zero-initialized globals (cleared by entry.asm)
 */

OUTPUT_FORMAT(elf32-i386)       /* 32-bit ELF for initial Multiboot2 entry */
OUTPUT_ARCH(i386:x86-64)        /* But targeting x86-64 */
ENTRY(_start)

SECTIONS
{
    /* Load kernel at 1MB physical address */
    . = 0x100000;

    /* Multiboot2 header must come first */
    .multiboot2 :
    {
        KEEP(*(.multiboot2))
    }

    /* Executable code */
    .text ALIGN(4096) :
    {
        *(.text)
        *(.text.*)
    }

    /* Read-only data: strings, constants, GDT/IDT descriptors */
    .rodata ALIGN(4096) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* Initialized data */
    .data ALIGN(4096) :
    {
        *(.data)
        *(.data.*)
    }

    /* BSS: zero-initialized data
     * entry.asm clears this section before calling kernel_main */
    .bss ALIGN(4096) :
    {
        bss_start = .;          /* Symbol used by entry.asm to find BSS start */
        *(.bss)
        *(.bss.*)
        *(COMMON)
        bss_end = .;            /* Symbol used by entry.asm to find BSS end */
    }

    /* Discard debug/comment sections from final binary */
    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}
