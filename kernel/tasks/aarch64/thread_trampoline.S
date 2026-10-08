.section .text

.globl thread_trampoline_start
thread_trampoline_start:
    bl thread_trampoline
    udf #0
