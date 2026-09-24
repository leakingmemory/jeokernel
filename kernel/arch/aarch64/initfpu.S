.arch armv8-a+fp

.global fpu_init
fpu_init:
    // 1. Configure CPACR_EL1 (Architectural Feature Access Control Register)
    // Set bits [21:20] (FPEN) to 0b11 to enable FPU/SIMD instructions at EL1 and EL0.
    mrs x0, cpacr_el1
    orr x0, x0, #(3 << 20)
    msr cpacr_el1, x0

    // 2. Clear FPCR (Floating-point Control Register)
    // Resets rounding mode to nearest, disables traps, and clears exception flags.
    mov x0, #0
    msr fpcr, x0

    // 3. Clear FPSR (Floating-point Status Register)
    // Clears cumulative exception flags (e.g., overflow, divide-by-zero).
    msr fpsr, x0

    // 4. Synchronize context
    // Ensures register writes complete before executing subsequent instructions.
    isb

    ret
