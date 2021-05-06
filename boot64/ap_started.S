.section .text
.code64

.globl ap_started

ap_started:
    movq $0x10,%rax
    mov %ax,%ds
    mov %ax,%es
    mov %ax,%fs
    mov %ax,%gs
    mov %ax,%ss
    movq (bootstrap_stackptr), %rsp
    mov %rsp, %rbp
    call ap_bootstrap
ap_start_overrun:
    hlt
    jmp ap_start_overrun

.globl ap_with_stack
ap_with_stack:
    mov %rsp, %rbp
    call ap_boot
