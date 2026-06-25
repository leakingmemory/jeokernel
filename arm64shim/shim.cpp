/*
 * First-stage arm64 boot shim - C++ entry.
 *
 * Milestone 1: print a greeting over the PL011 UART. On the QEMU "virt"
 * machine the first PL011 lives at 0x09000000 and -nographic routes it to
 * stdio. The same driver works on the Pi4 (PL011 at a different base) once we
 * stop hard-coding the address and read it from the DTB.
 */

// Freestanding build (-nostdinc): use the compiler's built-in fixed-width
// types instead of <cstdint>, which isn't available without libc++ headers.
using u32 = __UINT32_TYPE__;
using u64 = __UINT64_TYPE__;
using uptr = __UINTPTR_TYPE__;

namespace {
	// QEMU virt PL011 UART. (Pi4: 0xfe201000 - to come from the DTB later.)
	constexpr uptr PL011_BASE = 0x09000000;

	volatile u32 *const UARTDR =
		reinterpret_cast<volatile u32 *>(PL011_BASE + 0x00);
	volatile u32 *const UARTFR =
		reinterpret_cast<volatile u32 *>(PL011_BASE + 0x18);

	constexpr u32 UARTFR_TXFF = 1u << 5;	// transmit FIFO full

	void putc(char c) {
		while (*UARTFR & UARTFR_TXFF) {
			// spin until there is room in the TX FIFO
		}
		*UARTDR = static_cast<u32>(c);
	}

	void puts(const char *s) {
		for (; *s != '\0'; ++s) {
			if (*s == '\n') {
				putc('\r');
			}
			putc(*s);
		}
	}

	void put_hex(u64 v) {
		puts("0x");
		for (int shift = 60; shift >= 0; shift -= 4) {
			putc("0123456789abcdef"[(v >> shift) & 0xf]);
		}
	}

	// AArch64 has no x86-style "long mode" to enable - the core is already in
	// the 64-bit execution state and the MMU is always present. What we can
	// query is *how* paging can be configured: the supported physical address
	// range and translation granules in ID_AA64MMFR0_EL1. This is the first
	// step toward turning the MMU on in a later milestone.
	void report_paging() {
		u64 mmfr0;
		asm volatile("mrs %0, ID_AA64MMFR0_EL1" : "=r"(mmfr0));

		u64 cur_el;
		asm volatile("mrs %0, CurrentEL" : "=r"(cur_el));

		static const char *const pa_range[] = {
			"32-bit (4GB)",   "36-bit (64GB)",  "40-bit (1TB)",
			"42-bit (4TB)",   "44-bit (16TB)",  "48-bit (256TB)",
			"52-bit (4PB)",
		};
		const unsigned parange = mmfr0 & 0xf;

		puts("Running at EL");
		putc("0123"[(cur_el >> 2) & 0x3]);
		puts("\n");

		puts("ID_AA64MMFR0_EL1 = ");
		put_hex(mmfr0);
		puts("\n");

		puts("  PA range: ");
		puts(parange < 7 ? pa_range[parange] : "reserved");
		puts("\n");

		// Encoding asymmetry: TGran4/TGran64 use 0b0000 = supported, whereas
		// TGran16 uses 0b0001 = supported (0b0000 = not supported).
		puts("  4KB  granule: ");
		puts(((mmfr0 >> 28) & 0xf) == 0 ? "yes\n" : "no\n");
		puts("  16KB granule: ");
		puts(((mmfr0 >> 20) & 0xf) != 0 ? "yes\n" : "no\n");
		puts("  64KB granule: ");
		puts(((mmfr0 >> 24) & 0xf) == 0 ? "yes\n" : "no\n");
	}
}

extern "C" [[noreturn]] void shim_main() {
	puts("Hello, world from the jeokernel arm64 boot shim!\n");

	report_paging();

	for (;;) {
		asm volatile("wfe");
	}
}
