//
// Created by sigsegv on 8/28/26.
//

#include <armstage.h>
#include <stage1.h>

extern "C" [[noreturn]] void stage_main(ArmStageContext *ctx) {
	// MAIR_EL1:
	// Attr 0 (0x04): Device-nGnRE
	// Attr 1 (0xFF): Normal Inner/Outer Write-Back Non-Transient Read/Write Allocate
	u64 mair = (0x04ULL << 0) | (0xFFULL << 8);
	asm volatile("msr MAIR_EL1, %0" :: "r"(mair));

	// TCR_EL1:
	u64 mmfr0;
	asm volatile("mrs %0, ID_AA64MMFR0_EL1" : "=r"(mmfr0));
	u64 parange = mmfr0 & 0xf;
	if (parange > 6) {
		parange = 0;
	}

	u64 tcr = (parange << 32)
	        | (2ULL << 30) // TG1 = 4KB (0b10)
	        | (3ULL << 28) // SH1 = Inner Shareable (0b11)
	        | (1ULL << 26) // ORGN1 = Normal Outer WB WA (0b01)
	        | (1ULL << 24) // IRGN1 = Normal Inner WB WA (0b01)
	        | (0ULL << 23) // EPD1 = 0 (walk TTBR1)
	        | (16ULL << 16)// T1SZ = 16 (48-bit VA)
	        | (0ULL << 14) // TG0 = 4KB (0b00)
	        | (3ULL << 12) // SH0 = Inner Shareable (0b11)
	        | (1ULL << 10) // ORGN0 = Normal Outer WB WA (0b01)
	        | (1ULL << 8)  // IRGN0 = Normal Inner WB WA (0b01)
	        | (0ULL << 7)  // EPD0 = 0 (walk TTBR0)
	        | (16ULL << 0); // T0SZ = 16 (48-bit VA)
	asm volatile("msr TCR_EL1, %0" :: "r"(tcr));

	u64 ttbr = ctx->ttbr;
	asm volatile("msr TTBR0_EL1, %0" :: "r"(ttbr));
	asm volatile("msr TTBR1_EL1, %0" :: "r"(ttbr));

	asm volatile(
		"dsb ish\n"
		"tlbi vmalle1\n"
		"dsb ish\n"
		"isb\n"
	);

	u64 sctlr;
	asm volatile("mrs %0, SCTLR_EL1" : "=r"(sctlr));
	sctlr |= (1ULL << 0);  // M: enable MMU
	sctlr |= (1ULL << 2);  // C: enable data cache
	sctlr |= (1ULL << 12); // I: enable instruction cache
	sctlr |= (1ULL << 3);  // SA: SP Alignment check
	sctlr &= ~(1ULL << 1); // A: Alignment check disable
	asm volatile(
		"msr SCTLR_EL1, %0\n"
		"isb\n"
		:: "r"(sctlr)
	);

	u64 mpidr;
	asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
	u64 cpu_id = mpidr & 0xFF;
	if (cpu_id >= ctx->cpu_count) {
		cpu_id = 0;
	}

	u64 kernel_sp = ctx->kernel_stacks[cpu_id];
	u64 entrypoint = ctx->kernel_entrypoint;
	u64 stage1DataPtr = ctx->stage1data;

	asm volatile(
		"mov sp, %0\n"
		"mov x0, %1\n"
		"br %2\n"
		:
		: "r"(kernel_sp), "r"(stage1DataPtr), "r"(entrypoint)
		: "x0"
	);

	for (;;) {
		asm volatile("wfe");
	}
}