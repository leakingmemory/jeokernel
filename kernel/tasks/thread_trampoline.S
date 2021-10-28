.section .text
.code64

.globl thread_trampoline_start
thread_trampoline_start:
    call thread_trampoline
    ud2
