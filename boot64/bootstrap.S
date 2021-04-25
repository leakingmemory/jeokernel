.section .text
.code64

.globl bootstrap
.globl _start

_start:
bootstrap:
    mov $0x10,%ax
    mov %ax,%ds
    mov %ax,%es
    mov %ax,%fs
    mov %ax,%gs
    mov %ax,%ss
    // Stack ptr is in esi
    xor %rsp, %rsp
    mov %esi, %esp
    pushq %rbx
    mov %rbx, %rdi
    call start_m64
    popq %rbx
    hlt

.globl secondboot
secondboot:
    call init_m64
    hlt
