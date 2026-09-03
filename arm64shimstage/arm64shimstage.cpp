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

	u64 kernel_sp = ctx->kernel_sp;
	u64 entrypoint = ctx->kernel_entrypoint;

	kernel_sp -= sizeof(Stage1Data);
	kernel_sp &= ~0xFULL;

	Stage1Data *stage1Data = reinterpret_cast<Stage1Data *>(kernel_sp);
	stage1Data->uart = ctx->uart;
	stage1Data->dtb = ctx->dtb;
	stage1Data->phys_mem_base = ctx->phys_mem_base;
	stage1Data->phys_mem_size = ctx->phys_mem_size;
	stage1Data->root_pt = ctx->root_pt;
	stage1Data->cpu_id = ctx->cpu_id;
	stage1Data->cpu_count = ctx->cpu_count;

	asm volatile(
		"mov sp, %0\n"
		"mov x0, %1\n"
		"br %2\n"
		:
		: "r"(kernel_sp), "r"(reinterpret_cast<u64>(stage1Data)), "r"(entrypoint)
		: "x0"
	);

	for (;;) {
		asm volatile("wfe");
	}
}