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
using u8 = __UINT8_TYPE__;
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

	// ---- Flattened Device Tree (FDT) parsing -------------------------------
	// arm64 has no BIOS memory map; the loader passes a DTB pointer in x0 and
	// the /memory node(s) inside describe the physical RAM banks. Every FDT
	// integer is stored big-endian, so each word is byte-swapped on read.
	constexpr u32 FDT_MAGIC      = 0xd00dfeed;
	constexpr u32 FDT_BEGIN_NODE = 0x1;
	constexpr u32 FDT_END_NODE   = 0x2;
	constexpr u32 FDT_PROP       = 0x3;
	constexpr u32 FDT_NOP        = 0x4;
	constexpr u32 FDT_END        = 0x9;

	u32 be32(const u8 *p) {
		return (u32(p[0]) << 24) | (u32(p[1]) << 16) |
		       (u32(p[2]) << 8)  |  u32(p[3]);
	}

	// Read `cells` consecutive big-endian 32-bit words as one value. The FDT
	// uses 1 or 2 cells for addresses/sizes, so the result fits in a u64.
	u64 read_cells(const u8 *p, u32 cells) {
		u64 v = 0;
		for (u32 i = 0; i < cells; ++i, p += 4) {
			v = (v << 32) | be32(p);
		}
		return v;
	}

	bool streq(const char *a, const char *b) {
		while (*a != '\0' && *a == *b) {
			++a;
			++b;
		}
		return *a == *b;
	}

	// Tokens and property values are padded to a 4-byte boundary.
	const u8 *align4(const u8 *p) {
		return reinterpret_cast<const u8 *>((uptr(p) + 3) & ~uptr(3));
	}

	void report_memory(u64 dtb) {
		const u8 *blob = reinterpret_cast<const u8 *>(dtb);
		if (dtb == 0 || be32(blob) != FDT_MAGIC) {
			puts("No valid device tree - cannot enumerate RAM\n");
			return;
		}

		const u32 off_struct  = be32(blob + 8);
		const u32 off_strings = be32(blob + 12);
		const char *strings = reinterpret_cast<const char *>(blob + off_strings);
		const u8 *p = blob + off_struct;

		// Defaults from the FDT spec; the root node usually overrides them
		// (QEMU virt sets both to 2). They govern how a child's reg property
		// is decoded, and /memory is a child of the root.
		u32 addr_cells = 2;
		u32 size_cells = 1;

		// Per-node state. We assume /memory nodes are leaves (true on every
		// loader we target), so a single level of tracking suffices.
		int depth = 0;
		bool is_memory = false;
		const u8 *reg = nullptr;
		u32 reg_len = 0;

		puts("Physical RAM banks (from device tree):\n");

		for (;;) {
			const u32 tok = be32(p);
			p += 4;

			if (tok == FDT_END) {
				break;
			} else if (tok == FDT_NOP) {
				continue;
			} else if (tok == FDT_BEGIN_NODE) {
				++depth;
				while (*p != '\0') {	// skip the node name
					++p;
				}
				p = align4(p + 1);
				is_memory = false;
				reg = nullptr;
				reg_len = 0;
			} else if (tok == FDT_END_NODE) {
				if (is_memory && reg != nullptr) {
					const u32 entry = (addr_cells + size_cells) * 4;
					for (u32 o = 0; o + entry <= reg_len; o += entry) {
						const u64 base = read_cells(reg + o, addr_cells);
						const u64 size =
							read_cells(reg + o + addr_cells * 4, size_cells);
						puts("  base ");
						put_hex(base);
						puts("  size ");
						put_hex(size);
						puts("\n");
					}
				}
				--depth;
				is_memory = false;
				reg = nullptr;
			} else if (tok == FDT_PROP) {
				const u32 len     = be32(p);
				const u32 nameoff = be32(p + 4);
				const u8 *val = p + 8;
				p = align4(p + 8 + len);

				const char *name = strings + nameoff;
				if (depth == 1 && streq(name, "#address-cells")) {
					addr_cells = be32(val);
				} else if (depth == 1 && streq(name, "#size-cells")) {
					size_cells = be32(val);
				} else if (streq(name, "device_type") &&
				           streq(reinterpret_cast<const char *>(val),
				                 "memory")) {
					is_memory = true;
				} else if (streq(name, "reg")) {
					reg = val;
					reg_len = len;
				}
			} else {
				puts("Unknown FDT token - aborting parse\n");
				break;
			}
		}
	}
}

// dtb = the device tree pointer the loader left in x0; head.S does not touch
// x0 before the call, so AAPCS delivers it here as the first argument.
extern "C" [[noreturn]] void shim_main(u64 dtb) {
	puts("Hello, world from the jeokernel arm64 boot shim!\n");

	report_paging();
	report_memory(dtb);

	for (;;) {
		asm volatile("wfe");
	}
}
