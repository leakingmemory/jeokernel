.section .text
.code64

.macro pushaq
   push %rax
   push %rbx
   push %rcx
   push %rdx
   push %rbp
   push %rdi
   push %rsi
   push %r8
   push %r9
   push %r10
   push %r11
   push %r12
   push %r13
   push %r14
   push %r15
   xor %rax, %rax
   xor %rbx, %rbx
   mov %gs, %ax
   shl $16, %rax
   mov %fs, %bx
   add %rbx, %rax
   shl $16, %rax
   mov %es, %rbx
   add %rbx, %rax
   shl $16, %rax
   mov %ds, %rbx
   add %rbx, %rax
   push %rax
.endm

.macro popaq
   pop %rax
   mov %rax, %rbx
   and $0xFFFF, %rbx
   mov %bx, %ds
   shr $16, %rax
   mov %rax, %rbx
   and $0xFFFF, %rbx
   mov %bx, %es
   shr $16, %rax
   mov %rax, %rbx
   and $0xFFFF, %rbx
   mov %bx, %fs
   shr $16, %rax
   mov %rax, %rbx
   and $0xFFFF, %rbx
   mov %bx, %gs
   pop %r15
   pop %r14
   pop %r13
   pop %r12
   pop %r11
   pop %r10
   pop %r9
   pop %r8
   pop %rsi
   pop %rdi
   pop %rbp
   pop %rdx
   pop %rcx
   pop %rbx
   pop %rax
.endm

.macro intpush
    // Error code stack alignment
    test $7,%rsp
    //jnz error_code
    .byte 0x75
    .byte 0x04
    //sub $8,%rsp
    .byte 0x48
    .byte 0x83
    .byte 0xEC
    .byte 0x08
//error_code:
    and $0xFFFFFFFFFFFFFFF8,%rsp

    // Push state
    pushaq
    sub $512, %rsp
    fxsave (%rsp)
.endm

.macro intpop
   fxrstor (%rsp)
   add $512, %rsp
   popaq
   add $8, %rsp
.endm

.globl interrupt_imm00_handler
.globl interrupt_imm01_handler
.globl interrupt_imm02_handler
.globl interrupt_imm03_handler
.globl interrupt_imm04_handler
.globl interrupt_imm05_handler
.globl interrupt_imm06_handler
.globl interrupt_imm07_handler
.globl interrupt_imm08_handler
.globl interrupt_imm09_handler
.globl interrupt_imm0A_handler
.globl interrupt_imm0B_handler
.globl interrupt_imm0C_handler
.globl interrupt_imm0D_handler
.globl interrupt_imm0E_handler
.globl interrupt_imm0F_handler
.globl interrupt_imm10_handler
.globl interrupt_imm11_handler
.globl interrupt_imm12_handler
.globl interrupt_imm13_handler
.globl interrupt_imm14_handler
.globl interrupt_imm15_handler
.globl interrupt_imm16_handler
.globl interrupt_imm17_handler
.globl interrupt_imm18_handler
.globl interrupt_imm19_handler
.globl interrupt_imm1A_handler
.globl interrupt_imm1B_handler
.globl interrupt_imm1C_handler
.globl interrupt_imm1D_handler
.globl interrupt_imm1E_handler
.globl interrupt_imm1F_handler
.globl interrupt_imm20_handler
.globl interrupt_imm21_handler
.globl interrupt_imm22_handler
.globl interrupt_imm23_handler
.globl interrupt_imm24_handler
.globl interrupt_imm25_handler
.globl interrupt_imm26_handler
.globl interrupt_imm27_handler
.globl interrupt_imm28_handler
.globl interrupt_imm29_handler
.globl interrupt_imm2A_handler
.globl interrupt_imm2B_handler
.globl interrupt_imm2C_handler
.globl interrupt_imm2D_handler
.globl interrupt_imm2E_handler
.globl interrupt_imm2F_handler
.globl interrupt_imm30_handler
.globl interrupt_imm31_handler
.globl interrupt_imm32_handler
.globl interrupt_imm33_handler
.globl interrupt_imm34_handler
.globl interrupt_imm35_handler
.globl interrupt_imm36_handler
.globl interrupt_imm37_handler
.globl interrupt_imm38_handler
.globl interrupt_imm39_handler
.globl interrupt_imm3A_handler
.globl interrupt_imm3B_handler
.globl interrupt_imm3C_handler
.globl interrupt_imm3D_handler
.globl interrupt_imm3E_handler
.globl interrupt_imm3F_handler
.globl interrupt_imm40_handler
.globl interrupt_imm41_handler
.globl interrupt_imm42_handler
.globl interrupt_imm43_handler
.globl interrupt_imm44_handler
.globl interrupt_imm45_handler
.globl interrupt_imm46_handler
.globl interrupt_imm47_handler
.globl interrupt_imm48_handler
.globl interrupt_imm49_handler
.globl interrupt_imm4A_handler
.globl interrupt_imm4B_handler
.globl interrupt_imm4C_handler
.globl interrupt_imm4D_handler
.globl interrupt_imm4E_handler
.globl interrupt_imm4F_handler
.globl interrupt_imm50_handler
.globl interrupt_imm51_handler
.globl interrupt_imm52_handler
.globl interrupt_imm53_handler
.globl interrupt_imm54_handler
.globl interrupt_imm55_handler
.globl interrupt_imm56_handler
.globl interrupt_imm57_handler
.globl interrupt_imm58_handler
.globl interrupt_imm59_handler
.globl interrupt_imm5A_handler
.globl interrupt_imm5B_handler
.globl interrupt_imm5C_handler
.globl interrupt_imm5D_handler
.globl interrupt_imm5E_handler
.globl interrupt_imm5F_handler
.globl interrupt_imm60_handler
.globl interrupt_imm61_handler
.globl interrupt_imm62_handler
.globl interrupt_imm63_handler
.globl interrupt_imm64_handler
.globl interrupt_imm65_handler
.globl interrupt_imm66_handler
.globl interrupt_imm67_handler
.globl interrupt_imm68_handler
.globl interrupt_imm69_handler
.globl interrupt_imm6A_handler
.globl interrupt_imm6B_handler
.globl interrupt_imm6C_handler
.globl interrupt_imm6D_handler
.globl interrupt_imm6E_handler
.globl interrupt_imm6F_handler
.globl interrupt_imm70_handler
.globl interrupt_imm71_handler
.globl interrupt_imm72_handler
.globl interrupt_imm73_handler
.globl interrupt_imm74_handler
.globl interrupt_imm75_handler
.globl interrupt_imm76_handler
.globl interrupt_imm77_handler
.globl interrupt_imm78_handler
.globl interrupt_imm79_handler
.globl interrupt_imm7A_handler
.globl interrupt_imm7B_handler
.globl interrupt_imm7C_handler
.globl interrupt_imm7D_handler
.globl interrupt_imm7E_handler
.globl interrupt_imm7F_handler
.globl interrupt_imm80_handler
.globl interrupt_imm81_handler
.globl interrupt_imm82_handler
.globl interrupt_imm83_handler
.globl interrupt_imm84_handler
.globl interrupt_imm85_handler
.globl interrupt_imm86_handler
.globl interrupt_imm87_handler
.globl interrupt_imm88_handler
.globl interrupt_imm89_handler
.globl interrupt_imm8A_handler
.globl interrupt_imm8B_handler
.globl interrupt_imm8C_handler
.globl interrupt_imm8D_handler
.globl interrupt_imm8E_handler
.globl interrupt_imm8F_handler
.globl interrupt_imm90_handler
.globl interrupt_imm91_handler
.globl interrupt_imm92_handler
.globl interrupt_imm93_handler
.globl interrupt_imm94_handler
.globl interrupt_imm95_handler
.globl interrupt_imm96_handler
.globl interrupt_imm97_handler
.globl interrupt_imm98_handler
.globl interrupt_imm99_handler
.globl interrupt_imm9A_handler
.globl interrupt_imm9B_handler
.globl interrupt_imm9C_handler
.globl interrupt_imm9D_handler
.globl interrupt_imm9E_handler
.globl interrupt_imm9F_handler
.globl interrupt_immA0_handler
.globl interrupt_immA1_handler
.globl interrupt_immA2_handler
.globl interrupt_immA3_handler
.globl interrupt_immA4_handler
.globl interrupt_immA5_handler
.globl interrupt_immA6_handler
.globl interrupt_immA7_handler
.globl interrupt_immA8_handler
.globl interrupt_immA9_handler
.globl interrupt_immAA_handler
.globl interrupt_immAB_handler
.globl interrupt_immAC_handler
.globl interrupt_immAD_handler
.globl interrupt_immAE_handler
.globl interrupt_immAF_handler
.globl interrupt_immB0_handler
.globl interrupt_immB1_handler
.globl interrupt_immB2_handler
.globl interrupt_immB3_handler
.globl interrupt_immB4_handler
.globl interrupt_immB5_handler
.globl interrupt_immB6_handler
.globl interrupt_immB7_handler
.globl interrupt_immB8_handler
.globl interrupt_immB9_handler
.globl interrupt_immBA_handler
.globl interrupt_immBB_handler
.globl interrupt_immBC_handler
.globl interrupt_immBD_handler
.globl interrupt_immBE_handler
.globl interrupt_immBF_handler
.globl interrupt_immC0_handler
.globl interrupt_immC1_handler
.globl interrupt_immC2_handler
.globl interrupt_immC3_handler
.globl interrupt_immC4_handler
.globl interrupt_immC5_handler
.globl interrupt_immC6_handler
.globl interrupt_immC7_handler
.globl interrupt_immC8_handler
.globl interrupt_immC9_handler
.globl interrupt_immCA_handler
.globl interrupt_immCB_handler
.globl interrupt_immCC_handler
.globl interrupt_immCD_handler
.globl interrupt_immCE_handler
.globl interrupt_immCF_handler
.globl interrupt_immD0_handler
.globl interrupt_immD1_handler
.globl interrupt_immD2_handler
.globl interrupt_immD3_handler
.globl interrupt_immD4_handler
.globl interrupt_immD5_handler
.globl interrupt_immD6_handler
.globl interrupt_immD7_handler
.globl interrupt_immD8_handler
.globl interrupt_immD9_handler
.globl interrupt_immDA_handler
.globl interrupt_immDB_handler
.globl interrupt_immDC_handler
.globl interrupt_immDD_handler
.globl interrupt_immDE_handler
.globl interrupt_immDF_handler
.globl interrupt_immE0_handler
.globl interrupt_immE1_handler
.globl interrupt_immE2_handler
.globl interrupt_immE3_handler
.globl interrupt_immE4_handler
.globl interrupt_immE5_handler
.globl interrupt_immE6_handler
.globl interrupt_immE7_handler
.globl interrupt_immE8_handler
.globl interrupt_immE9_handler
.globl interrupt_immEA_handler
.globl interrupt_immEB_handler
.globl interrupt_immEC_handler
.globl interrupt_immED_handler
.globl interrupt_immEE_handler
.globl interrupt_immEF_handler
.globl interrupt_immF0_handler
.globl interrupt_immF1_handler
.globl interrupt_immF2_handler
.globl interrupt_immF3_handler
.globl interrupt_immF4_handler
.globl interrupt_immF5_handler
.globl interrupt_immF6_handler
.globl interrupt_immF7_handler
.globl interrupt_immF8_handler
.globl interrupt_immF9_handler
.globl interrupt_immFA_handler
.globl interrupt_immFB_handler
.globl interrupt_immFC_handler
.globl interrupt_immFD_handler
.globl interrupt_immFE_handler
.globl interrupt_immFF_handler

interrupt_imm00_handler:
    intpush
    mov $0x00,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm01_handler:
    intpush
    mov $0x01,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm02_handler:
    intpush
    mov $0x02,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm03_handler:
    intpush
    mov $0x03,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm04_handler:
    intpush
    mov $0x04,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm05_handler:
    intpush
    mov $0x05,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm06_handler:
    intpush
    mov $0x06,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm07_handler:
    intpush
    mov $0x07,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm08_handler: // double fault handler
    intpush
    mov $0x08,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm09_handler:
    intpush
    mov $0x09,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm0A_handler:
    intpush
    mov $0x0A,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm0B_handler:
    intpush
    mov $0x0B,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm0C_handler:
    intpush
    mov $0x0C,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm0D_handler:
    intpush
    mov $0x0D,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm0E_handler:
    intpush
    mov $0x0E,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm0F_handler:
    intpush
    mov $0x0F,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm10_handler:
    intpush
    mov $0x10,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm11_handler:
    intpush
    mov $0x11,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm12_handler:
    intpush
    mov $0x12,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm13_handler:
    intpush
    mov $0x13,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm14_handler:
    intpush
    mov $0x14,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm15_handler:
    intpush
    mov $0x15,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm16_handler:
    intpush
    mov $0x16,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm17_handler:
    intpush
    mov $0x17,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm18_handler:
    intpush
    mov $0x18,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm19_handler:
    intpush
    mov $0x19,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm1A_handler:
    intpush
    mov $0x1A,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm1B_handler:
    intpush
    mov $0x1B,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm1C_handler:
    intpush
    mov $0x1C,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm1D_handler:
    intpush
    mov $0x1D,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm1E_handler:
    intpush
    mov $0x1E,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm1F_handler:
    intpush
    mov $0x1F,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm20_handler:
    intpush
    mov $0x20,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm21_handler:
    intpush
    mov $0x21,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm22_handler:
    intpush
    mov $0x22,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm23_handler:
    intpush
    mov $0x23,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm24_handler:
    intpush
    mov $0x24,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm25_handler:
    intpush
    mov $0x25,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm26_handler:
    intpush
    mov $0x26,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm27_handler:
    intpush
    mov $0x27,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm28_handler: // double fault handler
    intpush
    mov $0x28,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm29_handler:
    intpush
    mov $0x29,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm2A_handler:
    intpush
    mov $0x2A,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm2B_handler:
    intpush
    mov $0x2B,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm2C_handler:
    intpush
    mov $0x2C,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm2D_handler:
    intpush
    mov $0x2D,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm2E_handler:
    intpush
    mov $0x2E,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm2F_handler:
    intpush
    mov $0x2F,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm30_handler:
    intpush
    mov $0x30,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm31_handler:
    intpush
    mov $0x31,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm32_handler:
    intpush
    mov $0x32,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm33_handler:
    intpush
    mov $0x33,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm34_handler:
    intpush
    mov $0x34,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm35_handler:
    intpush
    mov $0x35,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm36_handler:
    intpush
    mov $0x36,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm37_handler:
    intpush
    mov $0x37,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm38_handler: // double fault handler
    intpush
    mov $0x38,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm39_handler:
    intpush
    mov $0x39,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm3A_handler:
    intpush
    mov $0x3A,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm3B_handler:
    intpush
    mov $0x3B,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm3C_handler:
    intpush
    mov $0x3C,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm3D_handler:
    intpush
    mov $0x3D,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm3E_handler:
    intpush
    mov $0x3E,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm3F_handler:
    intpush
    mov $0x3F,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm40_handler:
    intpush
    mov $0x40,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm41_handler:
    intpush
    mov $0x41,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm42_handler:
    intpush
    mov $0x42,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm43_handler:
    intpush
    mov $0x43,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm44_handler:
    intpush
    mov $0x44,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm45_handler:
    intpush
    mov $0x45,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm46_handler:
    intpush
    mov $0x46,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm47_handler:
    intpush
    mov $0x47,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm48_handler: // double fault handler
    intpush
    mov $0x48,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm49_handler:
    intpush
    mov $0x49,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm4A_handler:
    intpush
    mov $0x4A,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm4B_handler:
    intpush
    mov $0x4B,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm4C_handler:
    intpush
    mov $0x4C,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm4D_handler:
    intpush
    mov $0x4D,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm4E_handler:
    intpush
    mov $0x4E,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm4F_handler:
    intpush
    mov $0x4F,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm50_handler:
    intpush
    mov $0x50,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm51_handler:
    intpush
    mov $0x51,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm52_handler:
    intpush
    mov $0x52,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm53_handler:
    intpush
    mov $0x53,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm54_handler:
    intpush
    mov $0x54,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm55_handler:
    intpush
    mov $0x55,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm56_handler:
    intpush
    mov $0x56,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm57_handler:
    intpush
    mov $0x57,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm58_handler: // double fault handler
    intpush
    mov $0x58,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm59_handler:
    intpush
    mov $0x59,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm5A_handler:
    intpush
    mov $0x5A,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm5B_handler:
    intpush
    mov $0x5B,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm5C_handler:
    intpush
    mov $0x5C,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm5D_handler:
    intpush
    mov $0x5D,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm5E_handler:
    intpush
    mov $0x5E,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm5F_handler:
    intpush
    mov $0x5F,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm60_handler:
    intpush
    mov $0x60,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm61_handler:
    intpush
    mov $0x61,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm62_handler:
    intpush
    mov $0x62,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm63_handler:
    intpush
    mov $0x63,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm64_handler:
    intpush
    mov $0x64,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm65_handler:
    intpush
    mov $0x65,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm66_handler:
    intpush
    mov $0x66,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm67_handler:
    intpush
    mov $0x67,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm68_handler: // double fault handler
    intpush
    mov $0x68,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm69_handler:
    intpush
    mov $0x69,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm6A_handler:
    intpush
    mov $0x6A,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm6B_handler:
    intpush
    mov $0x6B,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm6C_handler:
    intpush
    mov $0x6C,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm6D_handler:
    intpush
    mov $0x6D,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm6E_handler:
    intpush
    mov $0x6E,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm6F_handler:
    intpush
    mov $0x6F,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm70_handler:
    intpush
    mov $0x70,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm71_handler:
    intpush
    mov $0x71,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm72_handler:
    intpush
    mov $0x72,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm73_handler:
    intpush
    mov $0x73,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm74_handler:
    intpush
    mov $0x74,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm75_handler:
    intpush
    mov $0x75,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm76_handler:
    intpush
    mov $0x76,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm77_handler:
    intpush
    mov $0x77,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm78_handler: // double fault handler
    intpush
    mov $0x78,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm79_handler:
    intpush
    mov $0x79,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm7A_handler:
    intpush
    mov $0x7A,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm7B_handler:
    intpush
    mov $0x7B,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm7C_handler:
    intpush
    mov $0x7C,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm7D_handler:
    intpush
    mov $0x7D,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm7E_handler:
    intpush
    mov $0x7E,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm7F_handler:
    intpush
    mov $0x7F,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm80_handler:
    intpush
    mov $0x80,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm81_handler:
    intpush
    mov $0x81,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm82_handler:
    intpush
    mov $0x82,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm83_handler:
    intpush
    mov $0x83,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm84_handler:
    intpush
    mov $0x84,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm85_handler:
    intpush
    mov $0x85,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm86_handler:
    intpush
    mov $0x86,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm87_handler:
    intpush
    mov $0x87,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm88_handler: // double fault handler
    intpush
    mov $0x88,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm89_handler:
    intpush
    mov $0x89,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm8A_handler:
    intpush
    mov $0x8A,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm8B_handler:
    intpush
    mov $0x8B,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm8C_handler:
    intpush
    mov $0x8C,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm8D_handler:
    intpush
    mov $0x8D,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm8E_handler:
    intpush
    mov $0x8E,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm8F_handler:
    intpush
    mov $0x8F,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm90_handler:
    intpush
    mov $0x90,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm91_handler:
    intpush
    mov $0x91,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm92_handler:
    intpush
    mov $0x92,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm93_handler:
    intpush
    mov $0x93,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm94_handler:
    intpush
    mov $0x94,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm95_handler:
    intpush
    mov $0x95,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm96_handler:
    intpush
    mov $0x96,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm97_handler:
    intpush
    mov $0x97,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm98_handler: // double fault handler
    intpush
    mov $0x98,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm99_handler:
    intpush
    mov $0x99,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm9A_handler:
    intpush
    mov $0x9A,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm9B_handler:
    intpush
    mov $0x9B,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm9C_handler:
    intpush
    mov $0x9C,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm9D_handler:
    intpush
    mov $0x9D,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm9E_handler:
    intpush
    mov $0x9E,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm9F_handler:
    intpush
    mov $0x9F,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immA0_handler:
    intpush
    mov $0xA0,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immA1_handler:
    intpush
    mov $0xA1,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immA2_handler:
    intpush
    mov $0xA2,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immA3_handler:
    intpush
    mov $0xA3,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immA4_handler:
    intpush
    mov $0xA4,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immA5_handler:
    intpush
    mov $0xA5,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immA6_handler:
    intpush
    mov $0xA6,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immA7_handler:
    intpush
    mov $0xA7,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immA8_handler: // double fault handler
    intpush
    mov $0xA8,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immA9_handler:
    intpush
    mov $0xA9,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immAA_handler:
    intpush
    mov $0xAA,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immAB_handler:
    intpush
    mov $0xAB,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immAC_handler:
    intpush
    mov $0xAC,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immAD_handler:
    intpush
    mov $0xAD,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immAE_handler:
    intpush
    mov $0xAE,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immAF_handler:
    intpush
    mov $0xAF,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immB0_handler:
    intpush
    mov $0xB0,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immB1_handler:
    intpush
    mov $0xB1,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immB2_handler:
    intpush
    mov $0xB2,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immB3_handler:
    intpush
    mov $0xB3,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immB4_handler:
    intpush
    mov $0xB4,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immB5_handler:
    intpush
    mov $0xB5,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immB6_handler:
    intpush
    mov $0xB6,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immB7_handler:
    intpush
    mov $0xB7,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immB8_handler: // double fault handler
    intpush
    mov $0xB8,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immB9_handler:
    intpush
    mov $0xB9,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immBA_handler:
    intpush
    mov $0xBA,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immBB_handler:
    intpush
    mov $0xBB,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immBC_handler:
    intpush
    mov $0xBC,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immBD_handler:
    intpush
    mov $0xBD,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immBE_handler:
    intpush
    mov $0xBE,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immBF_handler:
    intpush
    mov $0xBF,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immC0_handler:
    intpush
    mov $0xC0,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immC1_handler:
    intpush
    mov $0xC1,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immC2_handler:
    intpush
    mov $0xC2,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immC3_handler:
    intpush
    mov $0xC3,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immC4_handler:
    intpush
    mov $0xC4,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immC5_handler:
    intpush
    mov $0xC5,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immC6_handler:
    intpush
    mov $0xC6,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immC7_handler:
    intpush
    mov $0xC7,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immC8_handler: // double fault handler
    intpush
    mov $0xC8,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immC9_handler:
    intpush
    mov $0xC9,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immCA_handler:
    intpush
    mov $0xCA,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immCB_handler:
    intpush
    mov $0xCB,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immCC_handler:
    intpush
    mov $0xCC,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immCD_handler:
    intpush
    mov $0xCD,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immCE_handler:
    intpush
    mov $0xCE,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immCF_handler:
    intpush
    mov $0xCF,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immD0_handler:
    intpush
    mov $0xD0,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immD1_handler:
    intpush
    mov $0xD1,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immD2_handler:
    intpush
    mov $0xD2,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immD3_handler:
    intpush
    mov $0xD3,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immD4_handler:
    intpush
    mov $0xD4,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immD5_handler:
    intpush
    mov $0xD5,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immD6_handler:
    intpush
    mov $0xD6,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immD7_handler:
    intpush
    mov $0xD7,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immD8_handler: // double fault handler
    intpush
    mov $0xD8,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immD9_handler:
    intpush
    mov $0xD9,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immDA_handler:
    intpush
    mov $0xDA,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immDB_handler:
    intpush
    mov $0xDB,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immDC_handler:
    intpush
    mov $0xDC,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immDD_handler:
    intpush
    mov $0xDD,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immDE_handler:
    intpush
    mov $0xDE,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immDF_handler:
    intpush
    mov $0xDF,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immE0_handler:
    intpush
    mov $0xE0,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immE1_handler:
    intpush
    mov $0xE1,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immE2_handler:
    intpush
    mov $0xE2,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immE3_handler:
    intpush
    mov $0xE3,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immE4_handler:
    intpush
    mov $0xE4,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immE5_handler:
    intpush
    mov $0xE5,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immE6_handler:
    intpush
    mov $0xE6,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immE7_handler:
    intpush
    mov $0xE7,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immE8_handler: // double fault handler
    intpush
    mov $0xE8,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immE9_handler:
    intpush
    mov $0xE9,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immEA_handler:
    intpush
    mov $0xEA,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immEB_handler:
    intpush
    mov $0xEB,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immEC_handler:
    intpush
    mov $0xEC,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immED_handler:
    intpush
    mov $0xED,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immEE_handler:
    intpush
    mov $0xEE,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immEF_handler:
    intpush
    mov $0xEF,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immF0_handler:
    intpush
    mov $0xF0,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immF1_handler:
    intpush
    mov $0xF1,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immF2_handler:
    intpush
    mov $0xF2,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immF3_handler:
    intpush
    mov $0xF3,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immF4_handler:
    intpush
    mov $0xF4,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immF5_handler:
    intpush
    mov $0xF5,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immF6_handler:
    intpush
    mov $0xF6,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immF7_handler:
    intpush
    mov $0xF7,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immF8_handler: // double fault handler
    intpush
    mov $0xF8,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immF9_handler:
    intpush
    mov $0xF9,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immFA_handler:
    intpush
    mov $0xFA,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immFB_handler:
    intpush
    mov $0xFB,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immFC_handler:
    intpush
    mov $0xFC,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immFD_handler:
    intpush
    mov $0xFD,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immFE_handler:
    intpush
    mov $0xFE,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_immFF_handler:
    intpush
    mov $0xFF,%rdi // first argument
    jmp interrupt_imm_handler
interrupt_imm_handler:
    mov $0x10, %rax
    mov %ax, %ss
    cld
    mov %rsp,%rsi // second argument
    call interrupt_handler
    intpop
    sti
    iretq

