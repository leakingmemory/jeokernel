.section .text
.code64

stackBottom:

.skip 4096

stackTop:

.globl bootstrap

bootstrap:
    mov $stackTop, %esp
    call start_m64

