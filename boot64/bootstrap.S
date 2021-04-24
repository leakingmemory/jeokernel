.section .text
.code64

stackBottom:

.skip 4096

stackTop:

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
    mov $stackTop, %rsp
    pushq %rbx
    mov %rbx, %rdi
    call start_m64
    popq %rbx
    hlt

.globl secondboot
secondboot:
    call init_m64
    hlt
