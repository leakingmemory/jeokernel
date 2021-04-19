.section .text
.code64

stackBottom:

.skip 4096

stackTop:

.globl bootstrap

bootstrap:
    mov $0x10,%ax
    mov %ax,%ds
    mov %ax,%es
    mov %ax,%fs
    mov %ax,%gs
    mov %ax,%ss
    mov $stackTop, %rsp
    call start_m64

