; =============================================================================
; bubblyVM -- x86_64 Kernel Entry Point
; File: host/kernel/arch/x86_64/boot/entry.asm
;
; This is the very first code the CPU executes after the bootloader hands off.
; Responsibilities:
;   1. Verify we were loaded by a Multiboot2-compliant bootloader (GRUB)
;   2. Set up a minimal stack
;   3. Clear the BSS segment
;   4. Call the C kernel entry: kernel_main()
;
; Architecture: x86_64 (the bootloader puts us in 32-bit protected mode first;
;               we will enter long mode in a later Tier-0 step via boot32.asm)
; Calling convention: System V AMD64 ABI
; =============================================================================

bits 32

; -----------------------------------------------------------------------------
; Multiboot2 Header
; GRUB reads this to know we are a valid kernel image.
; Must appear in the first 32KB of the kernel binary.
; -----------------------------------------------------------------------------
MULTIBOOT2_MAGIC    equ 0xE85250D6
MULTIBOOT2_ARCH    equ 0           ; i386 protected mode
MULTIBOOT2_LENGTH  equ (mb2_header_end - mb2_header_start)
MULTIBOOT2_CHECKSUM equ -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH + MULTIBOOT2_LENGTH)

section .multiboot2
align 8
mb2_header_start:
    dd MULTIBOOT2_MAGIC
    dd MULTIBOOT2_ARCH
    dd MULTIBOOT2_LENGTH
    dd MULTIBOOT2_CHECKSUM
    ; --- End tag (required) ---
    dw 0    ; type = end
    dw 0    ; flags
    dd 8    ; size
mb2_header_end:

; -----------------------------------------------------------------------------
; BSS: kernel stack (16KB)
; -----------------------------------------------------------------------------
section .bss
align 16
stack_bottom:
    resb 16384      ; 16 KiB stack
stack_top:

; -----------------------------------------------------------------------------
; Entry point: _start
; GRUB jumps here in 32-bit protected mode.
; EAX = 0x36d76289 (Multiboot2 magic)
; EBX = physical address of Multiboot2 information structure
; -----------------------------------------------------------------------------
section .text
global _start
extern kernel_main
extern bss_start
extern bss_end

_start:
    ; Set up our stack immediately -- we have no stack yet
    mov esp, stack_top

    ; Verify Multiboot2 magic in EAX
    ; If wrong, we were not loaded by a compliant bootloader -- halt
    cmp eax, 0x36d76289
    jne .no_multiboot

    ; Save Multiboot2 info pointer (EBX) -- we will pass it to kernel_main
    push ebx

    ; Clear BSS section (zero-initialize global/static variables)
    ; BSS must be zero before C code runs
    mov edi, bss_start
    mov ecx, bss_end
    sub ecx, edi
    xor eax, eax
    rep stosd

    ; Restore Multiboot2 pointer as first argument
    pop ebx

    ; -- Long mode setup will be added here in next commit --
    ; For now: call 32-bit kernel_main for early serial/VGA console test
    push ebx        ; arg1: multiboot info pointer
    call kernel_main

    ; If kernel_main returns (it should not), halt
.halt:
    cli
    hlt
    jmp .halt

.no_multiboot:
    ; Bootloader magic was wrong -- we cannot safely proceed
    ; Write error code to port 0xE9 (QEMU debug port) and halt
    mov al, 0xFF
    out 0xE9, al
    cli
    hlt
    jmp .no_multiboot
