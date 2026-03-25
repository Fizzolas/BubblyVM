/*
 * bubblyVM -- Early Kernel Main
 * File: host/kernel/kernel.c
 *
 * This is the first C code that runs after the bootloader and assembly
 * entry stubs hand off control. At Tier 0, the goals are:
 *
 *   1. Initialize the serial port (COM1) for debug output
 *   2. Initialize the VGA text console as fallback display
 *   3. Print the bubblyVM version banner
 *   4. Print the Multiboot2 memory map
 *   5. Halt cleanly
 *
 * This code intentionally does NOT depend on any standard library.
 * Everything here is freestanding (no libc, no OS calls).
 *
 * Compiler: x86_64-elf-gcc -ffreestanding -nostdlib
 */

#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * Serial port (COM1) output
 * Used for debug logging -- always available before display
 * ============================================================ */

#define COM1_PORT 0x3F8

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_init(void) {
    outb(COM1_PORT + 1, 0x00); /* Disable interrupts */
    outb(COM1_PORT + 3, 0x80); /* Enable DLAB (set baud rate divisor) */
    outb(COM1_PORT + 0, 0x03); /* Baud rate divisor low byte: 38400 baud */
    outb(COM1_PORT + 1, 0x00); /* Baud rate divisor high byte */
    outb(COM1_PORT + 3, 0x03); /* 8 bits, no parity, one stop bit */
    outb(COM1_PORT + 2, 0xC7); /* Enable FIFO, clear, 14-byte threshold */
    outb(COM1_PORT + 4, 0x0B); /* IRQs enabled, RTS/DSR set */
}

static int serial_tx_ready(void) {
    return inb(COM1_PORT + 5) & 0x20;
}

void serial_putchar(char c) {
    while (!serial_tx_ready());
    outb(COM1_PORT, (uint8_t)c);
    /* Also mirror to QEMU debug port 0xE9 */
    outb(0xE9, (uint8_t)c);
}

void serial_print(const char *str) {
    for (; *str; str++) {
        if (*str == '\n')
            serial_putchar('\r'); /* CR+LF for serial terminals */
        serial_putchar(*str);
    }
}

/* ============================================================
 * VGA Text Console (80x25)
 * Fallback display output when no GPU driver exists yet
 * ============================================================ */

#define VGA_BUFFER   ((volatile uint16_t *)0xB8000)
#define VGA_WIDTH    80
#define VGA_HEIGHT   25
#define VGA_COLOR    0x0F  /* White on black */

static int vga_col = 0;
static int vga_row = 0;

static void vga_scroll(void) {
    for (int r = 1; r < VGA_HEIGHT; r++)
        for (int c = 0; c < VGA_WIDTH; c++)
            VGA_BUFFER[(r-1)*VGA_WIDTH + c] = VGA_BUFFER[r*VGA_WIDTH + c];
    for (int c = 0; c < VGA_WIDTH; c++)
        VGA_BUFFER[(VGA_HEIGHT-1)*VGA_WIDTH + c] = (VGA_COLOR << 8) | ' ';
    vga_row = VGA_HEIGHT - 1;
}

void vga_putchar(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
        if (vga_row >= VGA_HEIGHT) vga_scroll();
        return;
    }
    if (c == '\r') {
        vga_col = 0;
        return;
    }
    VGA_BUFFER[vga_row * VGA_WIDTH + vga_col] = ((uint16_t)VGA_COLOR << 8) | (uint8_t)c;
    vga_col++;
    if (vga_col >= VGA_WIDTH) {
        vga_col = 0;
        vga_row++;
        if (vga_row >= VGA_HEIGHT) vga_scroll();
    }
}

void vga_print(const char *str) {
    for (; *str; str++)
        vga_putchar(*str);
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_BUFFER[i] = (VGA_COLOR << 8) | ' ';
    vga_col = 0;
    vga_row = 0;
}

/* ============================================================
 * Minimal printf-like output (no malloc, no libc)
 * Supports: %s, %d, %u, %x, %c, %%
 * ============================================================ */

static void print_uint(uint64_t n, int base, int pad, char padchar,
                        void (*putfn)(char)) {
    char buf[64];
    const char *digits = "0123456789abcdef";
    int i = 0;
    if (n == 0) { buf[i++] = '0'; }
    else { while (n) { buf[i++] = digits[n % base]; n /= base; } }
    /* Padding */
    while (i < pad) buf[i++] = padchar;
    /* Reverse */
    for (int j = i-1; j >= 0; j--) putfn(buf[j]);
}

void kprintf(const char *fmt, ...) {
    /* Variadic args without stdarg -- use GCC __builtin_va_list */
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            serial_putchar(*fmt);
            vga_putchar(*fmt);
            continue;
        }
        fmt++;
        int pad = 0;
        char padchar = ' ';
        if (*fmt == '0') { padchar = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') { pad = pad*10 + (*fmt - '0'); fmt++; }

        switch (*fmt) {
            case 's': {
                const char *s = __builtin_va_arg(args, const char *);
                if (!s) s = "(null)";
                for (; *s; s++) { serial_putchar(*s); vga_putchar(*s); }
                break;
            }
            case 'd': {
                int64_t n = __builtin_va_arg(args, int64_t);
                if (n < 0) { serial_putchar('-'); vga_putchar('-'); n = -n; }
                print_uint((uint64_t)n, 10, pad, padchar, serial_putchar);
                print_uint((uint64_t)n, 10, pad, padchar, vga_putchar);
                break;
            }
            case 'u': {
                uint64_t n = __builtin_va_arg(args, uint64_t);
                print_uint(n, 10, pad, padchar, serial_putchar);
                print_uint(n, 10, pad, padchar, vga_putchar);
                break;
            }
            case 'x': {
                uint64_t n = __builtin_va_arg(args, uint64_t);
                serial_print("0x"); vga_print("0x");
                print_uint(n, 16, pad, padchar, serial_putchar);
                print_uint(n, 16, pad, padchar, vga_putchar);
                break;
            }
            case 'c': {
                char c = (char)__builtin_va_arg(args, int);
                serial_putchar(c); vga_putchar(c);
                break;
            }
            case '%':
                serial_putchar('%'); vga_putchar('%');
                break;
            default:
                serial_putchar('%'); vga_putchar('%');
                serial_putchar(*fmt); vga_putchar(*fmt);
                break;
        }
    }
    __builtin_va_end(args);
}

/* ============================================================
 * Multiboot2 memory map parser
 * Reads the memory regions from GRUB and prints them
 * ============================================================ */

/* Multiboot2 info header */
typedef struct {
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed)) mb2_info_t;

/* Multiboot2 tag header */
typedef struct {
    uint32_t type;
    uint32_t size;
} __attribute__((packed)) mb2_tag_t;

/* Multiboot2 memory map tag (type 6) */
typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
} __attribute__((packed)) mb2_mmap_tag_t;

typedef struct {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;   /* 1=usable, 2=reserved, 3=ACPI, 4=NVS, 5=bad */
    uint32_t reserved;
} __attribute__((packed)) mb2_mmap_entry_t;

static const char *mmap_type_name(uint32_t type) {
    switch (type) {
        case 1: return "Usable RAM";
        case 2: return "Reserved";
        case 3: return "ACPI Reclaimable";
        case 4: return "ACPI NVS";
        case 5: return "Bad Memory";
        default: return "Unknown";
    }
}

static void parse_multiboot2(uint32_t mb2_addr) {
    mb2_info_t *info = (mb2_info_t *)(uintptr_t)mb2_addr;
    mb2_tag_t *tag = (mb2_tag_t *)(uintptr_t)(mb2_addr + 8);

    kprintf("[MEM] Parsing Multiboot2 info @ 0x%x (size %u bytes)\n",
            (uint64_t)mb2_addr, (uint64_t)info->total_size);

    while (tag->type != 0) { /* type 0 = end tag */
        if (tag->type == 6) { /* memory map */
            mb2_mmap_tag_t *mmap_tag = (mb2_mmap_tag_t *)tag;
            uint32_t num_entries = (tag->size - sizeof(mb2_mmap_tag_t))
                                    / mmap_tag->entry_size;
            mb2_mmap_entry_t *entry = (mb2_mmap_entry_t *)(
                (uintptr_t)tag + sizeof(mb2_mmap_tag_t));

            kprintf("[MEM] Memory map (%u entries):\n", (uint64_t)num_entries);
            for (uint32_t i = 0; i < num_entries; i++, entry++) {
                kprintf("  [%u] Base=0x%x Len=0x%x Type=%s\n",
                        (uint64_t)i,
                        entry->base_addr,
                        entry->length,
                        mmap_type_name(entry->type));
            }
        }
        /* Advance to next tag (8-byte aligned) */
        uint32_t next = (uint32_t)((uintptr_t)tag + tag->size);
        next = (next + 7) & ~7U;
        tag = (mb2_tag_t *)(uintptr_t)next;
    }
}

/* ============================================================
 * Panic handler
 * Called when something goes fatally wrong.
 * Prints message to serial and VGA, then halts permanently.
 * Never call this in a recoverable situation.
 * ============================================================ */

void kpanic(const char *msg) {
    serial_print("\n\n*** KERNEL PANIC ***\n");
    serial_print(msg);
    serial_print("\nSystem halted. No recovery possible from this state.\n");

    vga_clear();
    vga_print("*** KERNEL PANIC ***");
    vga_putchar('\n');
    vga_print(msg);
    vga_putchar('\n');
    vga_print("System halted.");

    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}

/* ============================================================
 * kernel_main -- Entry from assembly stub
 * mb2_info_addr: physical address of Multiboot2 info structure
 * ============================================================ */

void kernel_main(uint32_t mb2_info_addr) {
    serial_init();
    vga_clear();

    /* Banner */
    kprintf("bubblyVM v0.0.1-tier0\n");
    kprintf("======================\n");
    kprintf("[BOOT] Serial console: OK\n");
    kprintf("[BOOT] VGA console:    OK\n");
    kprintf("[BOOT] Multiboot2 info @ 0x%x\n", (uint64_t)mb2_info_addr);

    /* Parse and display memory map */
    if (mb2_info_addr) {
        parse_multiboot2(mb2_info_addr);
    } else {
        kprintf("[WARN] No Multiboot2 info available\n");
    }

    kprintf("[BOOT] Tier 0 boot complete. Halting.\n");
    kprintf("[BOOT] Next step: long mode transition (boot32.asm)\n");

    /* Halt -- Tier 0 is done when we reach this line */
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}
