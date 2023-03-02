#include <loaderconfig.h>

#define GDTR_PTR_ADDR (GDT_ADDR + ((GDT_SIZE + (LDT_DESC_SIZE * 2)) * 8))

.code16

.globl ap_trampoline
.globl ap_trampoline_end

.align 256
ap_trampoline:
    cli
    cld
    #ljmp $0x0800,$0x0040
    .byte 0xEA
    .word 0x0040
    .word 0x0800
.align 16
ap_trampoline_gdt32_8010:
    .long 0, 0
    .long 0x0000FFFF, 0x00CF9A00    // flat code
    .long 0x0000FFFF, 0x008F9200    // flat data
    .long 0x00000068, 0x00CF8900    // tss
ap_trampoline_gdt32ptr_8030:
    .word ap_trampoline_gdt32ptr_8030 - ap_trampoline_gdt32_8010 - 1
    .long 0x8010
    .long 0, 0
.align 64
ap_trampoline_8040:
    xorw    %ax, %ax
    movw    %ax, %ds
    lgdtl   0x8030
    movl    %cr0, %eax
    orl     $1, %eax
    movl    %eax, %cr0
    #ljmpw    $8, $0x8060
    .byte 0xEA
    .word 0x8060
    .word 0x0008
.code32
.align 32
ap_trampoline_8060:
    mov $0x10,%eax
    mov %ax,%ds
    mov %ax,%es
    mov %ax,%fs
    mov %ax,%gs
    mov %ax,%ss
    lock incw (0x8FF0)
    lgdt GDTR_PTR_ADDR

    fninit
    fldcw (0x8158)

    mov %cr0, %eax
    and $0xFFFFFFFB, %eax // coprocessor emu off
    or $2, %eax // coprocessor monitor on
    mov %eax, %cr0
    mov %cr4, %eax
    or $0x700, %eax // FXSR xmmsexcept
    mov %eax, %cr4

    fxsave (0x8200)
    mov (0x821C), %eax
    or %eax, %eax
    jnz ap_has_mxcsr_mask
    mov $0xFFBF, %eax
ap_has_mxcsr_mask:
    and %eax, (0x815C)
    ldmxcsr (0x815C)

    // Page tables
    mov $0x05000, %eax
    mov %eax, %cr3

    mov %cr4, %eax
    or $0x00020,%eax
    mov %eax, %cr4

    // enable long mode: 0x100 (1 << 8) and 0x800 (1 << 11)
    mov $0xC0000080, %ecx
    rdmsr
    or $0x900, %eax
    wrmsr

    mov %cr0, %eax
    or $0x00010000, %eax // Write protect
    or $0x80000000, %eax // Paging
    mov %eax, %cr0
    //jmp ap_started

    // JMP FAR 0x08:0x8100
    .byte 0xEA
    .long 0x8100
    .word 0x0008
.code64
.align 256
ap_trampoline_8100:
    movq 0x8150, %rdi
    movq 0x8140, %rax
    #jmpq (%rax) / jmpq *ap_trampoline_8140
    .byte 0xFF
    .byte 0xE0
    ud2

.align 64
ap_trampoline_8140:
    .quad 0xaddda000000addda # ap_started

    ud2

.align 16
ap_trampoline_8150:
    .quad 0x557744cccc997733 # stackptr
    .long 0x0000037F # 0x8158 (used low word)
    .long 0x00001F80 # 0x815C

ap_trampoline_end:
    hlt
    jmp ap_trampoline_end

.align 256
tmp_8200:
    .long 0
