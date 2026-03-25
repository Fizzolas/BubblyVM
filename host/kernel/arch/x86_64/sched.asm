; =============================================================================
; bubblyVM -- Context Switch
; File: host/kernel/arch/x86_64/sched.asm
;
; context_switch(cpu_context_t *old, cpu_context_t *new)
;
; Saves the current CPU state into *old, then restores CPU state from *new.
; After this function returns, we are running in the new process's context.
;
; Only callee-saved registers need to be preserved (System V AMD64 ABI):
;   RBX, RBP, R12, R13, R14, R15
; Caller-saved registers (RAX, RCX, RDX, RSI, RDI, R8-R11) are NOT saved
; because the calling convention says the caller already saved them if needed.
;
; The trick: we save RIP as the return address of this function.
; When the new process's context is restored, it "returns" from
; context_switch() as if it had just called it normally.
;
; Arguments (System V AMD64):
;   RDI = pointer to old cpu_context_t  (save here)
;   RSI = pointer to new cpu_context_t  (load from here)
;
; cpu_context_t layout (must match process.h exactly):
;   offset  0: r15
;   offset  8: r14
;   offset 16: r13
;   offset 24: r12
;   offset 32: rbx
;   offset 40: rbp
;   offset 48: rsp
;   offset 56: rip
; =============================================================================

bits 64

global context_switch

context_switch:
    ; ---- Save old context (RDI = &old->context) ----
    mov [rdi + 0],  r15
    mov [rdi + 8],  r14
    mov [rdi + 16], r13
    mov [rdi + 24], r12
    mov [rdi + 32], rbx
    mov [rdi + 40], rbp
    mov [rdi + 48], rsp     ; Save current stack pointer

    ; Save RIP: use the return address already on the stack.
    ; When this context is restored, execution resumes at the
    ; instruction after the call to context_switch().
    mov rax, [rsp]          ; Peek at return address
    mov [rdi + 56], rax     ; Save as RIP

    ; ---- Load new context (RSI = &new->context) ----
    mov r15, [rsi + 0]
    mov r14, [rsi + 8]
    mov r13, [rsi + 16]
    mov r12, [rsi + 24]
    mov rbx, [rsi + 32]
    mov rbp, [rsi + 40]
    mov rsp, [rsi + 48]     ; Switch to new stack

    ; Jump to new RIP (push it then ret, so branch predictor is happy)
    mov rax, [rsi + 56]
    push rax
    ret
