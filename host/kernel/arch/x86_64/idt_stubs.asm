; =============================================================================
; bubblyVM -- IDT Interrupt Stubs
; File: host/kernel/arch/x86_64/idt_stubs.asm
;
; One stub per interrupt vector. Each stub:
;   1. Pushes a fake error code (0) for exceptions that don't push one
;   2. Pushes the vector number
;   3. Saves all general-purpose registers (pusha equivalent for 64-bit)
;   4. Calls idt_dispatch(interrupt_frame_t *frame)
;   5. Restores all registers
;   6. Returns via IRETQ
;
; Exceptions WITH error codes (pushed automatically by CPU):
;   8 (DF), 10 (TS), 11 (NP), 12 (SS), 13 (GP), 14 (PF), 17 (AC), 21 (CP)
;
; All others get a fake error code of 0 pushed by our stub.
; =============================================================================

bits 64

extern idt_dispatch

; -----------------------------------------------------------------------
; Macro: ISR_NOERR vector
; For exceptions that do NOT push an error code
; -----------------------------------------------------------------------
%macro ISR_NOERR 1
global isr%1
isr%1:
    push qword 0        ; Fake error code (keeps frame uniform)
    push qword %1       ; Vector number
    jmp isr_common
%endmacro

; -----------------------------------------------------------------------
; Macro: ISR_ERR vector
; For exceptions that DO push an error code (CPU already pushed it)
; -----------------------------------------------------------------------
%macro ISR_ERR 1
global isr%1
isr%1:
    push qword %1       ; Vector number (error code already on stack)
    jmp isr_common
%endmacro

; -----------------------------------------------------------------------
; Define all 48 stubs
; -----------------------------------------------------------------------

; CPU exceptions 0-31
ISR_NOERR 0   ; #DE Divide-by-zero
ISR_NOERR 1   ; #DB Debug
ISR_NOERR 2   ; NMI
ISR_NOERR 3   ; #BP Breakpoint
ISR_NOERR 4   ; #OF Overflow
ISR_NOERR 5   ; #BR Bound range
ISR_NOERR 6   ; #UD Invalid opcode
ISR_NOERR 7   ; #NM Device not available
ISR_ERR   8   ; #DF Double fault        (CPU pushes error code)
ISR_NOERR 9   ; Coprocessor segment overrun
ISR_ERR   10  ; #TS Invalid TSS         (CPU pushes error code)
ISR_ERR   11  ; #NP Segment not present (CPU pushes error code)
ISR_ERR   12  ; #SS Stack fault         (CPU pushes error code)
ISR_ERR   13  ; #GP General protection  (CPU pushes error code)
ISR_ERR   14  ; #PF Page fault          (CPU pushes error code)
ISR_NOERR 15  ; Reserved
ISR_NOERR 16  ; #MF x87 FPU error
ISR_ERR   17  ; #AC Alignment check     (CPU pushes error code)
ISR_NOERR 18  ; #MC Machine check
ISR_NOERR 19  ; #XF SIMD FPU
ISR_NOERR 20  ; #VE Virtualization
ISR_ERR   21  ; #CP Control protection  (CPU pushes error code)
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; Hardware IRQs 0-15 (mapped to vectors 32-47 by PIC)
ISR_NOERR 32  ; IRQ0  -- Timer (PIT)
ISR_NOERR 33  ; IRQ1  -- Keyboard
ISR_NOERR 34  ; IRQ2  -- Cascade (PIC2)
ISR_NOERR 35  ; IRQ3  -- COM2
ISR_NOERR 36  ; IRQ4  -- COM1
ISR_NOERR 37  ; IRQ5  -- LPT2
ISR_NOERR 38  ; IRQ6  -- Floppy
ISR_NOERR 39  ; IRQ7  -- LPT1 / spurious
ISR_NOERR 40  ; IRQ8  -- RTC
ISR_NOERR 41  ; IRQ9  -- ACPI
ISR_NOERR 42  ; IRQ10 -- free
ISR_NOERR 43  ; IRQ11 -- free
ISR_NOERR 44  ; IRQ12 -- PS/2 mouse
ISR_NOERR 45  ; IRQ13 -- FPU
ISR_NOERR 46  ; IRQ14 -- ATA primary
ISR_NOERR 47  ; IRQ15 -- ATA secondary

; -----------------------------------------------------------------------
; isr_common -- shared handler body
; Stack layout on entry:
;   [rsp+0]  = vector number
;   [rsp+8]  = error code (real or fake 0)
;   [rsp+16] = RIP     \  pushed
;   [rsp+24] = CS       > by CPU
;   [rsp+32] = RFLAGS  /
;   [rsp+40] = RSP      (if privilege change)
;   [rsp+48] = SS
; -----------------------------------------------------------------------
isr_common:
    ; Save all general-purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Pass pointer to the full stack frame as first argument (RDI)
    ; The frame matches interrupt_frame_t in idt.c
    mov rdi, rsp

    ; Align stack to 16 bytes for System V ABI (required before call)
    and rsp, ~0xFULL
    sub rsp, 8

    ; Call the C dispatcher
    call idt_dispatch

    ; Restore stack alignment
    add rsp, 8
    mov rsp, rdi   ; Restore original RSP (undo alignment)

    ; Restore all registers in reverse order
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Remove vector number and error code from stack
    add rsp, 16

    ; Return from interrupt
    iretq
