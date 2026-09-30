.arch armv8-a+fp+simd
.section .text, "ax"

.macro ventry label, vec_id
.balign 128
.global \label
\label:
    sub sp, sp, #0x350
    stp x0, x1, [sp, #(0x30 + 0)]
    mov x0, #\vec_id
    b common_exception_entry
.endm

.balign 2048
.global exception_vector_table
exception_vector_table:
    // Current EL with SP0
    ventry vector_curr_el_sp0_sync, 0
    ventry vector_curr_el_sp0_irq, 1
    ventry vector_curr_el_sp0_fiq, 2
    ventry vector_curr_el_sp0_serror, 3

    // Current EL with SPx
    ventry vector_curr_el_spx_sync, 4
    ventry vector_curr_el_spx_irq, 5
    ventry vector_curr_el_spx_fiq, 6
    ventry vector_curr_el_spx_serror, 7

    // Lower EL using AArch64
    ventry vector_lower_el_aarch64_sync, 8
    ventry vector_lower_el_aarch64_irq, 9
    ventry vector_lower_el_aarch64_fiq, 10
    ventry vector_lower_el_aarch64_serror, 11

    // Lower EL using AArch32
    ventry vector_lower_el_aarch32_sync, 12
    ventry vector_lower_el_aarch32_irq, 13
    ventry vector_lower_el_aarch32_fiq, 14
    ventry vector_lower_el_aarch32_serror, 15

common_exception_entry:
    // x0 and x1 were already saved at [sp, #(0x30 + 0)]
    // x0 currently contains vec_id
    // Save x2..x29 in pairs, and x30
    stp x2, x3, [sp, #(0x30 + 16)]
    stp x4, x5, [sp, #(0x30 + 32)]
    stp x6, x7, [sp, #(0x30 + 48)]
    stp x8, x9, [sp, #(0x30 + 64)]
    stp x10, x11, [sp, #(0x30 + 80)]
    stp x12, x13, [sp, #(0x30 + 96)]
    stp x14, x15, [sp, #(0x30 + 112)]
    stp x16, x17, [sp, #(0x30 + 128)]
    stp x18, x19, [sp, #(0x30 + 144)]
    stp x20, x21, [sp, #(0x30 + 160)]
    stp x22, x23, [sp, #(0x30 + 176)]
    stp x24, x25, [sp, #(0x30 + 192)]
    stp x26, x27, [sp, #(0x30 + 208)]
    stp x28, x29, [sp, #(0x30 + 224)]
    str x30, [sp, #(0x30 + 240)]

    // Save thread pointers
    mrs x1, tpidr_el0
    mrs x2, tpidrro_el0
    stp x1, x2, [sp, #(0x30 + 248)]
    mrs x3, tpidr_el1
    str x3, [sp, #(0x30 + 264)]

    // Save CPU frame (ESR, FAR, ELR, SPSR, SP)
    mrs x1, esr_el1
    mrs x2, far_el1
    stp x1, x2, [sp, #0]
    mrs x3, elr_el1
    mrs x4, spsr_el1
    stp x3, x4, [sp, #16]
    add x5, sp, #0x350
    str x5, [sp, #32]
    str xzr, [sp, #40]

    // Save SIMD / FPU state
    stp q0, q1, [sp, #(0x140 + 0)]
    stp q2, q3, [sp, #(0x140 + 32)]
    stp q4, q5, [sp, #(0x140 + 64)]
    stp q6, q7, [sp, #(0x140 + 96)]
    stp q8, q9, [sp, #(0x140 + 128)]
    stp q10, q11, [sp, #(0x140 + 160)]
    stp q12, q13, [sp, #(0x140 + 192)]
    stp q14, q15, [sp, #(0x140 + 224)]
    stp q16, q17, [sp, #(0x140 + 256)]
    stp q18, q19, [sp, #(0x140 + 288)]
    stp q20, q21, [sp, #(0x140 + 320)]
    stp q22, q23, [sp, #(0x140 + 352)]
    stp q24, q25, [sp, #(0x140 + 384)]
    stp q26, q27, [sp, #(0x140 + 416)]
    stp q28, q29, [sp, #(0x140 + 448)]
    stp q30, q31, [sp, #(0x140 + 480)]

    mrs x1, fpcr
    mrs x2, fpsr
    str w1, [sp, #(0x140 + 512)]
    str w2, [sp, #(0x140 + 516)]
    str xzr, [sp, #(0x140 + 520)]

    // Prepare arguments for interrupt_handler(vector, cpu_frame, stack_frame, fpu_state)
    // x0 is already vector id
    mov x1, sp
    add x2, sp, #0x30
    add x3, sp, #0x140
    bl interrupt_handler

    // Restore SIMD / FPU state
    ldr w1, [sp, #(0x140 + 512)]
    ldr w2, [sp, #(0x140 + 516)]
    msr fpcr, x1
    msr fpsr, x2

    ldp q0, q1, [sp, #(0x140 + 0)]
    ldp q2, q3, [sp, #(0x140 + 32)]
    ldp q4, q5, [sp, #(0x140 + 64)]
    ldp q6, q7, [sp, #(0x140 + 96)]
    ldp q8, q9, [sp, #(0x140 + 128)]
    ldp q10, q11, [sp, #(0x140 + 160)]
    ldp q12, q13, [sp, #(0x140 + 192)]
    ldp q14, q15, [sp, #(0x140 + 224)]
    ldp q16, q17, [sp, #(0x140 + 256)]
    ldp q18, q19, [sp, #(0x140 + 288)]
    ldp q20, q21, [sp, #(0x140 + 320)]
    ldp q22, q23, [sp, #(0x140 + 352)]
    ldp q24, q25, [sp, #(0x140 + 384)]
    ldp q26, q27, [sp, #(0x140 + 416)]
    ldp q28, q29, [sp, #(0x140 + 448)]
    ldp q30, q31, [sp, #(0x140 + 480)]

    // Restore ELR and SPSR
    ldp x3, x4, [sp, #16]
    msr elr_el1, x3
    msr spsr_el1, x4

    // Restore thread pointers
    ldp x1, x2, [sp, #(0x30 + 248)]
    msr tpidr_el0, x1
    msr tpidrro_el0, x2
    ldr x3, [sp, #(0x30 + 264)]
    msr tpidr_el1, x3

    // Restore general registers
    ldr x30, [sp, #(0x30 + 240)]
    ldp x28, x29, [sp, #(0x30 + 224)]
    ldp x26, x27, [sp, #(0x30 + 208)]
    ldp x24, x25, [sp, #(0x30 + 192)]
    ldp x22, x23, [sp, #(0x30 + 176)]
    ldp x20, x21, [sp, #(0x30 + 160)]
    ldp x18, x19, [sp, #(0x30 + 144)]
    ldp x16, x17, [sp, #(0x30 + 128)]
    ldp x14, x15, [sp, #(0x30 + 112)]
    ldp x12, x13, [sp, #(0x30 + 96)]
    ldp x10, x11, [sp, #(0x30 + 80)]
    ldp x8, x9, [sp, #(0x30 + 64)]
    ldp x6, x7, [sp, #(0x30 + 48)]
    ldp x4, x5, [sp, #(0x30 + 32)]
    ldp x2, x3, [sp, #(0x30 + 16)]
    ldp x0, x1, [sp, #(0x30 + 0)]

    add sp, sp, #0x350
    eret

.global install_exception_vectors
install_exception_vectors:
    adrp x0, exception_vector_table
    add x0, x0, :lo12:exception_vector_table
    msr vbar_el1, x0
    isb
    ret
