; =============================================================================
; bubblyVM -- 32-bit Protected Mode to 64-bit Long Mode Transition
; File: host/kernel/arch/x86_64/boot/boot32.asm
;
; This code runs after entry.asm verifies multiboot and sets up the stack.
; It transitions the CPU from 32-bit protected mode into 64-bit long mode
; so the main C kernel can run as proper 64-bit code.
;
; Steps:
;   1. Check CPU supports long mode (CPUID)
;   2. Set up identity-mapped page tables (first 4GB)
;   3. Enable PAE (Physical Address Extension)
;   4. Load page tables into CR3
;   5. Enable long mode bit in EFER MSR
;   6. Enable paging (activates long mode)
;   7. Load 64-bit GDT
;   8. Far jump into 64-bit code segment
; =============================================================================

bits 32

section .bss
align 4096
; Page tables: PML4, PDPT, PD (identity map first 1GB for now)
pml4_table: resb 4096
pdpt_table: resb 4096
pd_table:   resb 4096

section .rodata
align 16
gdt64:
    dq 0                        ; Null descriptor
.code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53)  ; 64-bit code segment
gdt64_pointer:
    dw $ - gdt64 - 1            ; Limit
    dd gdt64                    ; Base (32-bit pointer, fine for early boot)

section .text
global enter_long_mode
extern kernel_main_64

enter_long_mode:
    ; -------------------------------------------------------------------------
    ; Step 1: Verify CPUID and long mode support
    ; -------------------------------------------------------------------------
    ; Check if CPUID supports extended functions
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    ; Check long mode bit (bit 29 of EDX from leaf 0x80000001)
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode

    ; -------------------------------------------------------------------------
    ; Step 2: Build identity-mapped page tables
    ; PML4[0] -> PDPT, PDPT[0] -> PD, PD maps 0-1GB with 2MB pages
    ; -------------------------------------------------------------------------
    ; PML4[0] = &pdpt_table | PRESENT | WRITABLE
    mov eax, pdpt_table
    or eax, 0x3
    mov [pml4_table], eax

    ; PDPT[0] = &pd_table | PRESENT | WRITABLE
    mov eax, pd_table
    or eax, 0x3
    mov [pdpt_table], eax

    ; PD: 512 entries, each maps a 2MB region (PRESENT | WRITABLE | HUGE)
    mov ecx, 0
.map_pd:
    mov eax, 0x200000           ; 2MB
    mul ecx
    or eax, 0x83                ; PRESENT | WRITABLE | HUGE PAGE
    mov [pd_table + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_pd

    ; -------------------------------------------------------------------------
    ; Step 3: Enable PAE (CR4.PAE = bit 5)
    ; -------------------------------------------------------------------------
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; -------------------------------------------------------------------------
    ; Step 4: Load PML4 into CR3
    ; -------------------------------------------------------------------------
    mov eax, pml4_table
    mov cr3, eax

    ; -------------------------------------------------------------------------
    ; Step 5: Enable long mode in EFER MSR (bit 8)
    ; -------------------------------------------------------------------------
    mov ecx, 0xC0000080         ; EFER MSR index
    rdmsr
    or eax, 1 << 8              ; Set LME (Long Mode Enable)
    wrmsr

    ; -------------------------------------------------------------------------
    ; Step 6: Enable paging (CR0.PG = bit 31) -- this activates long mode
    ; Also ensure protected mode bit is set (CR0.PE = bit 0)
    ; -------------------------------------------------------------------------
    mov eax, cr0
    or eax, (1 << 31) | (1 << 0)
    mov cr0, eax

    ; -------------------------------------------------------------------------
    ; Step 7: Load 64-bit GDT
    ; -------------------------------------------------------------------------
    lgdt [gdt64_pointer]

    ; -------------------------------------------------------------------------
    ; Step 8: Far jump into 64-bit code segment
    ; This flushes the pipeline and activates 64-bit mode fully
    ; -------------------------------------------------------------------------
    jmp gdt64.code:long_mode_entry

.no_long_mode:
    ; CPU does not support 64-bit mode -- write to debug port and halt
    mov al, 0xFE
    out 0xE9, al
    cli
    hlt

bits 64
long_mode_entry:
    ; We are now in 64-bit long mode
    ; Zero segment registers (not used in 64-bit mode except FS/GS)
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Call the main 64-bit C kernel
    call kernel_main_64

    ; Should never return
.halt:
    cli
    hlt
    jmp .halt
