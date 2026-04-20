.section .text

.globl jumpto64
jumpto64:
    jmp $0x08,$in64bit
    hlt

in64bit:
    jmp %rax
    hlt;
