/*
 * First-stage arm64 boot shim - C++ entry.
 *
 * Milestone 1: print a greeting over the PL011 UART. On the QEMU "virt"
 * machine the first PL011 lives at 0x09000000 and -nographic routes it to
 * stdio. The same driver works on the Pi4 (PL011 at a different base) once we
 * stop hard-coding the address and read it from the DTB.
 */

#include <physpagemap.h>
#include <new>
#include <cstring>
#include <vpallocator.h>
#include <pagetable.h>
#include <elf.h>
#include <elf_impl.h>
#include <armstage.h>

// Freestanding build (-nostdinc): use the compiler's built-in fixed-width
// types instead of <cstdint>, which isn't available without libc++ headers.
using u8 = __UINT8_TYPE__;
using u32 = __UINT32_TYPE__;
using u64 = __UINT64_TYPE__;
using uptr = __UINTPTR_TYPE__;

uintptr_t pagetable_virt_offset = 0;

uintptr_t get_pagetable_virt_offset() {
	return pagetable_virt_offset;
}

void set_pagetable_virt_offset(uintptr_t offset) {
	pagetable_virt_offset = offset;
}

// Bounds of the shim image, defined by arm64shim.ld. _start is the load
// address (0x40080000) and __end is just past the boot stack, so [_start,__end)
// covers text/rodata/data/bss/stack. With the MMU off these symbol addresses
// are physical addresses.
extern "C" const u8 _start[];
extern "C" const u8 __end[];
extern "C" const u8 wrapped_kernel_start[];
extern "C" const u8 wrapped_kernel_end[];
extern "C" const u8 wrapped_armstage_start[];
extern "C" const u8 wrapped_armstage_end[];

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

		// VA range support.
		u64 mmfr2;
		asm volatile("mrs %0, ID_AA64MMFR2_EL1" : "=r"(mmfr2));
		const unsigned varange = (mmfr2 >> 12) & 0xf;
		puts("  VA range: ");
		if (varange == 0) {
			puts("48-bit (256TB)\n");
		} else if (varange == 1) {
			puts("52-bit (4PB)\n");
		} else {
			puts("reserved\n");
		}
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

	// Walk the device tree's /memory nodes, invoking fn(ctx, base, size) once
	// per (base,size) pair. Shared by the RAM report and the free-page search.
	using bank_fn = void (*)(void *ctx, u64 base, u64 size);

	void for_each_memory_bank(u64 dtb, bank_fn fn, void *ctx) {
		const u8 *blob = reinterpret_cast<const u8 *>(dtb);
		if (dtb == 0 || be32(blob) != FDT_MAGIC) {
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
						fn(ctx, base, size);
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

	void print_bank(void *, u64 base, u64 size) {
		puts("  base ");
		put_hex(base);
		puts("  size ");
		put_hex(size);
		puts("\n");
	}

	void report_memory(u64 dtb) {
		const u8 *blob = reinterpret_cast<const u8 *>(dtb);
		if (dtb == 0 || be32(blob) != FDT_MAGIC) {
			puts("No valid device tree - cannot enumerate RAM\n");
			return;
		}
		puts("Physical RAM banks (from device tree):\n");
		for_each_memory_bank(dtb, print_bank, nullptr);
	}

	// ---- First-stage free page search -------------------------------------
	// Pick the lowest physical page that lies inside a RAM bank and is not
	// occupied by something the loader left in memory. For this first stage
	// that is the shim image itself and the DTB; this page is the seed the
	// PhyspageMap bootstrap will build from.
	constexpr u64 PAGE_SIZE = 0x1000;

	u64 page_align_up(u64 x) {
		return (x + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
	}

	struct FreePageSearch {
		u64 res_start[2];	// reserved regions to step over
		u64 res_end[2];
		unsigned n_res;
		bool found;
		u64 first_memory_page;
		u64 page;
		bool first_memory_page_set;
	};

	void scan_bank(void *ctxv, u64 base, u64 size) {
		FreePageSearch *s = static_cast<FreePageSearch *>(ctxv);
		const u64 bank_end = base + size;
		u64 cand = page_align_up(base);

		if (!s->first_memory_page_set || s->first_memory_page > cand) {
			s->first_memory_page = cand;
			s->first_memory_page_set = true;
		}

		// Bump the candidate past any reserved region it overlaps, repeating
		// until it lands in a gap (or runs off the end of the bank).
		bool moved = true;
		while (moved) {
			moved = false;
			for (unsigned i = 0; i < s->n_res; ++i) {
				if (cand < s->res_end[i] &&
				    cand + PAGE_SIZE > s->res_start[i]) {
					cand = page_align_up(s->res_end[i]);
					moved = true;
				}
			}
		}

		if (cand + PAGE_SIZE <= bank_end) {
			if (!s->found || cand < s->page) {	// keep the lowest across banks
				s->found = true;
				s->page = cand;
			}
		}
	}

	FreePageSearch get_first_free_page(u64 dtb) {
		FreePageSearch s{};
		s.first_memory_page = 0;
		s.first_memory_page_set = false;

		const u64 shim_start = reinterpret_cast<u64>(_start);
		const u64 shim_end   = reinterpret_cast<u64>(__end);
		s.res_start[s.n_res] = shim_start;
		s.res_end[s.n_res]   = shim_end;
		++s.n_res;

		// DTB header byte 4 holds totalsize (big-endian).
		const u8 *blob = reinterpret_cast<const u8 *>(dtb);
		if (dtb != 0 && be32(blob) == FDT_MAGIC) {
			s.res_start[s.n_res] = dtb;
			s.res_end[s.n_res]   = dtb + be32(blob + 4);
			++s.n_res;
		}

		puts("Reserved by shim image: ");
		put_hex(shim_start);
		puts(" .. ");
		put_hex(shim_end);
		puts("\n");
		if (s.n_res > 1) {
			puts("Reserved by DTB:        ");
			put_hex(s.res_start[1]);
			puts(" .. ");
			put_hex(s.res_end[1]);
			puts("\n");
		}

		for_each_memory_bank(dtb, scan_bank, &s);

		return s;
	}

	void report_first_free_page(FreePageSearch &s) {
		if (s.found) {
			puts("First free physical page: ");
			put_hex(s.page);
			puts("\n");
		} else {
			puts("No free physical page found\n");
		}
	}

	void report_first_free_page(u64 dtb) {
		FreePageSearch s = get_first_free_page(dtb);
		report_first_free_page(s);
	}

	FreePageSearch get_and_report_first_free_page(u64 dtb) {
		auto s = get_first_free_page(dtb);
		report_first_free_page(s);
		return s;
	}

	// ---- Populate the PhyspageMap -----------------------------------------
	// Hand the page allocator a complete picture of the window the map covers,
	// [base, max()): every page that is usable RAM is released, everything else
	// is claimed. We do it in three passes - claim the whole window, release
	// the pages that sit wholly inside a RAM bank, then re-claim the pages the
	// loader is itself occupying - so holes between/around banks stay claimed.
	struct ReleaseCtx {
		physpagemap_managed *map;
		u32 base_page;
		u32 max_page;
		u64 released;
	};

	void release_usable_bank(void *ctxv, u64 base, u64 size) {
		auto *c = static_cast<ReleaseCtx *>(ctxv);
		// Only whole pages fully contained in the bank are usable: round the
		// start up and the end down.
		u64 first = page_align_up(base) / PAGE_SIZE;
		u64 end   = (base + size) / PAGE_SIZE;
		if (first < c->base_page) {
			first = c->base_page;
		}
		if (end > c->max_page) {
			end = c->max_page;
		}
		for (u64 p = first; p < end; ++p) {
			c->map->release(static_cast<u32>(p));
			++c->released;
		}
	}

	// Claim [first, end) (page numbers); the map clips to its own window, so
	// this returns how many of those pages actually fell inside it.
	u64 claim_clipped(physpagemap_managed *map, u32 base_page, u32 max_page,
	                  u32 first, u32 end) {
		map->claim(first, end - first);
		if (first < base_page) {
			first = base_page;
		}
		if (end > max_page) {
			end = max_page;
		}
		return end > first ? static_cast<u64>(end - first) : 0;
	}

	void populate_physpagemap(physpagemap_managed *map, u64 dtb,
	                          const FreePageSearch &s) {
		const u32 base_page = static_cast<u32>(s.first_memory_page / PAGE_SIZE);
		const u32 max_page  = map->max();

		puts("Populating physpagemap: pages ");
		put_hex(base_page);
		puts(" .. ");
		put_hex(max_page);
		puts("\n");

		// Start with the whole window unusable...
		map->claim(base_page, max_page - base_page);

		// ...release the pages that lie wholly inside a RAM bank...
		ReleaseCtx ctx{map, base_page, max_page, 0};
		for_each_memory_bank(dtb, release_usable_bank, &ctx);

		// ...then take back the pages the loader occupies. Round the reserved
		// byte ranges outward so any page they touch stays claimed.
		u64 reclaimed = 0;
		for (unsigned i = 0; i < s.n_res; ++i) {
			const u32 first = static_cast<u32>(s.res_start[i] / PAGE_SIZE);
			const u32 end   =
				static_cast<u32>(page_align_up(s.res_end[i]) / PAGE_SIZE);
			if (end > first) {
				reclaimed += claim_clipped(map, base_page, max_page, first, end);
			}
		}
		const u32 seed = static_cast<u32>(s.page / PAGE_SIZE);	// PhyspageMap page
		reclaimed += claim_clipped(map, base_page, max_page, seed, seed + 1);

		const u64 usable = ctx.released - reclaimed;
		puts("Usable pages released:  ");
		put_hex(usable);
		puts("\n");
		puts("Unusable pages claimed: ");
		put_hex((max_page - base_page) - usable);
		puts("\n");
	}

	std::optional<u32> allocate_physpage(physpagemap_managed *map, u32 pages) {
		u32 start{0};
		u32 p{map->base()};
		u32 n{0};
		if (pages <= 0) {
			return {};
		}
		while (p < map->max() && n < pages) {
			if (!map->claimed(p)) {
				if (n == 0) {
					start = p;
				}
				++n;
			} else {
				n = 0;
			}
			++p;
		}
		if (n >= pages) {
			for (uint32_t i = 0; i < pages; i++) {
				map->claim(start + i);
			}
			return {start};
		} else {
			return {};
		}
	}

	template <class VPPageAllocator> std::optional<u32> allocate_jump_physpage(physpagemap_managed *map, u32 pages, VPAllocator<VPPageAllocator> &vp, u64 supervisor_start) {
		u32 start{0};
		u32 p{map->base()};
		u32 n{0};
		if (pages <= 0) {
			return {};
		}
		while (p < map->max() && n < pages) {
			if (!map->claimed(p)) {
				uintptr_t pa{p};
				pa = pa * 0x1000;
				if (vp.IsFree(supervisor_start + pa, 0x1000)) {
					if (n == 0) {
						start = p;
					}
					++n;
				} else {
					n = 0;
				}
			} else {
				n = 0;
			}
			++p;
		}
		if (n >= pages) {
			uintptr_t pa{start};
			pa = pa * 0x1000;
			uintptr_t ps{pages};
			ps = ps * 0x1000;
			for (uint32_t i = 0; i < pages; i++) {
				map->claim(start + i);
			}
			if (vp.TryAllocateFromAddress(supervisor_start + pa, ps) != VPAllocatorResult::DONE) {
				return {};
			}
			return {start};
		} else {
			return {};
		}
	}

	enum MapFlags : u32 {
		MapFlagNone       = 0,
		MapFlagWrite      = 1 << 0,
		MapFlagExecutable = 1 << 1,
		MapFlagUser       = 1 << 2,
		MapFlagDevice     = 1 << 3,
	};

	std::optional<u64> alloc_pt_page(physpagemap_managed *ppmap) {
		auto o_page = allocate_physpage(ppmap, 1);
		if (!o_page) {
			return {};
		}
		u64 phys = static_cast<u64>(*o_page) * PAGE_SIZE;
		memset(reinterpret_cast<void *>(phys), 0, PAGE_SIZE);
		return phys;
	}

	pageentr *get_pageentr(physpagemap_managed *ppmapp, pagetable &root, u64 vaddr) {
		u64 indices[4];
		indices[0] = (vaddr >> 39) & 0x1FF;
		indices[1] = (vaddr >> 30) & 0x1FF;
		indices[2] = (vaddr >> 21) & 0x1FF;
		indices[3] = (vaddr >> 12) & 0x1FF;

		pageentr *current_table = &root[0];

		for (int level = 0; level < 3; ++level) {
			pageentr &entry = current_table[indices[level]];
			if (!entry.valid()) {
				if (ppmapp == nullptr) {
					return nullptr;
				}
				auto o_new_pt = alloc_pt_page(ppmapp);
				if (!o_new_pt) {
					puts("Failed to allocate sub-table\n");
					return nullptr;
				}
				entry.valid() = 1;
				entry.table() = 1;
				entry.ppn() = (*o_new_pt >> 12);
			}
			current_table = &entry.get_subtable()[0];
		}

		pageentr &leaf = current_table[indices[3]];
		return &leaf;
	}
	void map_page(physpagemap_managed *ppmapp, pagetable &root, u64 vaddr, u64 paddr, u32 flags) {
		pageentr *perhaps_leaf = get_pageentr(ppmapp, root, vaddr);
		if (perhaps_leaf == nullptr) {
			puts("Failed to map memory page\n");
			return;
		}
		pageentr &leaf = *perhaps_leaf;
		leaf.valid() = 1;
		leaf.table() = 1;
		leaf.af() = 1;

		u64 ap = 0;
		if (!(flags & MapFlagWrite)) {
			ap |= 2; // AP[2] = 1 for ReadOnly
		}
		if (flags & MapFlagUser) {
			ap |= 1; // AP[1] = 1 for User
		}
		leaf.ap() = ap;

		bool executable = flags & MapFlagExecutable;
		leaf.pxn() = executable ? 0 : 1;
		leaf.uxn() = (executable && (flags & MapFlagUser)) ? 0 : 1;

		leaf.ppn() = (paddr >> 12);

		if (flags & MapFlagDevice) {
			leaf.attr_indx() = 0;
			leaf.sh() = 0;
		} else {
			leaf.attr_indx() = 1;
			leaf.sh() = 3;
		}
	}

	class ArmShimVPPageAllocator {
	public:
		constexpr ArmShimVPPageAllocator() = default;
		constexpr ~ArmShimVPPageAllocator() = default;
		std::optional<VPAllocatorPage *> TryAllocate() const {
			auto *map = get_physpagemap();
			if (!map) {
				return {};
			}
			auto o_page = allocate_physpage(map, 1);
			if (!o_page) {
				return {};
			}
			u64 addr = static_cast<u64>(*o_page) * PAGE_SIZE;
			return new (reinterpret_cast<void *>(addr)) VPAllocatorPage();
		}
		constexpr void Free(VPAllocatorPage *) const {
		}
		constexpr bool IsVirtualAddress() const {
			return false;
		}
	};
}

// dtb = the device tree pointer the loader left in x0; head.S does not touch
// x0 before the call, so AAPCS delivers it here as the first argument.
extern "C" [[noreturn]] void shim_main(u64 dtb) {
	report_paging();
	report_memory(dtb);

	auto pagesearch = get_and_report_first_free_page(dtb);
	if (!pagesearch.found) {
		puts("No free physical page found - aborting\n");
		for (;;) {
			asm volatile("wfe");
		}
	}

	// Seed the PhyspageMap in the first free page, and base its window at the
	// lowest RAM page so the bank addresses land inside [base, max()).
	auto *ppmap = new (reinterpret_cast<void *>(pagesearch.page)) PhyspageMap();
	const u32 base_page =
		static_cast<u32>(pagesearch.first_memory_page / PAGE_SIZE);
	init_simple_physpagemap(reinterpret_cast<uint64_t>(reinterpret_cast<void*>(ppmap)), base_page);
	auto *sppmap = get_physpagemap();

	populate_physpagemap(sppmap, dtb, pagesearch);

	uint32_t kvmap_phys;
	{
		auto o_kvmap_phys = allocate_physpage(sppmap, 1);
		if (!o_kvmap_phys) {
			puts("Failed to allocate kvmap_phys - aborting\n");
			for (;;) {
				asm volatile("wfe");
			}
		}
		kvmap_phys = *o_kvmap_phys;
	}
	VPAllocatorPage *vpalloc_root = new (reinterpret_cast<void *>(static_cast<uptr>(kvmap_phys) * PAGE_SIZE)) VPAllocatorPage();
	ArmShimVPPageAllocator vpPageAllocator{};
	VPAllocator<ArmShimVPPageAllocator> vpalloc(&vpPageAllocator, vpalloc_root);

	u64 supervisor_start;
	{
		u64 mmfr2;
		asm volatile("mrs %0, ID_AA64MMFR2_EL1" : "=r"(mmfr2));
		const unsigned varange = (mmfr2 >> 12) & 0xf;
		unsigned va_bits = 48;
		if (varange == 1) {
			va_bits = 52;
		}

		supervisor_start = ~0ULL << va_bits;
		u64 supervisor_size = 1ULL << va_bits;
		supervisor_size -= 0x1000;

		puts("Releasing supervisor virtual memory: ");
		put_hex(supervisor_start);
		puts(" size ");
		put_hex(supervisor_size);
		puts("\n");

		if (vpalloc.TryFree(supervisor_start, supervisor_size) != VPAllocatorResult::DONE) {
			puts("Failed to release kernel virtual memory\n");
		}
	}

	uint32_t kernel_root_pt_phys;
	{
		auto o_kernel_root_pt_phys = allocate_physpage(sppmap, 1);
		if (!o_kernel_root_pt_phys) {
			puts("Failed to allocate kernel_root_pt_phys - aborting\n");
			for (;;) {
				asm volatile("wfe");
			}
		}
		kernel_root_pt_phys = *o_kernel_root_pt_phys;
	}
	void *kernel_root_pt_virt = reinterpret_cast<void *>(static_cast<uptr>(kernel_root_pt_phys) * PAGE_SIZE);
	memset(kernel_root_pt_virt, 0, PAGE_SIZE);
	puts("Kernel root page table allocated at: ");
	put_hex(static_cast<u64>(kernel_root_pt_phys) * PAGE_SIZE);
	puts("\n");
	pagetable &root_pt = *(reinterpret_cast<pagetable *>(kernel_root_pt_virt));

	const uint8_t *stageloader_src = &wrapped_armstage_start[0];
	const uint8_t *kernel_src = &wrapped_kernel_start[0];
	const uint8_t *kernel_src_end = &wrapped_kernel_end[0];
	puts("Shimloader detected entrypoint: ");
	put_hex(reinterpret_cast<u64>(&shim_main));
	puts("\nStage loader embedded binary: ");
	put_hex(reinterpret_cast<u64>(wrapped_armstage_start));
	puts("\nKernel embedded binary: ");
	put_hex(reinterpret_cast<u64>(kernel_src));
	puts("\n");

	puts("Stage loader size: ");
	auto stageloader_size = reinterpret_cast<uintptr_t>(&wrapped_armstage_end) - reinterpret_cast<uintptr_t>(stageloader_src);
	put_hex(stageloader_size);
	auto stageloader_pages = static_cast<uint32_t>(stageloader_size / 0x1000);
	if ((stageloader_size % 0x1000) != 0) {
		++stageloader_pages;
	}
	puts(" (pages: ");
	put_hex(stageloader_pages);
	puts(")\n");
	puts("Kernel size: ");
	auto kernel_size = reinterpret_cast<uintptr_t>(kernel_src_end) - reinterpret_cast<uintptr_t>(kernel_src);
	put_hex(static_cast<uint32_t>(kernel_size));
	auto kernel_pages = static_cast<uint32_t>(kernel_size / 0x1000);
	if ((kernel_size % 0x1000) != 0) {
		++kernel_pages;
	}
	puts(" (pages: ");
	put_hex(kernel_pages);
	std::optional<u32> phys_kernel_p = allocate_physpage(sppmap, kernel_pages);
	puts(")\n");
	if (!phys_kernel_p){
		puts("FATAL ERROR: Failed to allocate kernel pages\n");
		for (;;) {
			asm volatile("wfe");
		}
	}
	void *phys_kernel = reinterpret_cast<void *>(*phys_kernel_p * PAGE_SIZE);
	memcpy(phys_kernel, kernel_src, kernel_size);
	puts("Kernel phys addr: ");
	put_hex(reinterpret_cast<u64>(phys_kernel));
	puts("\n");

	ELF stageloader{(void *) stageloader_src, (void *) &wrapped_armstage_end};
	if (!stageloader.is_valid()) {
		puts("FATAL ERROR: stageloader binary is not valid\n");
		for (;;) {
			asm volatile("wfe");
		}
	}
	if (!stageloader.is_valid()) {
        puts("FATAL ERROR: stageloader binary is not valid\n");
		for (;;) {
			asm volatile("wfe");
		}
	}
    ELF kernel{(void *) kernel_src, (void *) kernel_src_end};
    if (!kernel.is_valid()) {
        puts("FATAL ERROR: Kernel binary is not valid: ");
    	puts(kernel.get_error());
    	puts("\n");
    	for (;;) {
    		asm volatile("wfe");
    	}
    }
	const auto &stageloader_header_unaligned = stageloader.get_elf64_header();
	typename std::remove_const<typename std::remove_reference<decltype(stageloader_header_unaligned)>::type>::type stageloader_header{};
	memcpy(&stageloader_header, &stageloader_header_unaligned, sizeof(stageloader_header));
    uint64_t stageloader_entrypoint_addr = stageloader_header.e_entry;
    puts("Stageloader entrypoint: ");
    put_hex(static_cast<u64>(stageloader_entrypoint_addr));
    puts("\n");
    uintptr_t stageloader_vmem_start;
    {
        bool loaded_first_offset{false};
        uintptr_t first_offset;
        for (uint16_t i = 0; i < stageloader_header.e_phnum; i++) {
            const auto &ph_unaligned = stageloader_header_unaligned.get_program_entry_unaligned(i);
        	typename std::remove_const<typename std::remove_reference<decltype(ph_unaligned)>::type>::type ph{};
        	memcpy(&ph, &ph_unaligned, sizeof(ph));
            if (ph.p_type != PHT_LOAD || ph.p_memsz <= 0) {
                continue;
            }
            if (!loaded_first_offset || ph.p_offset < first_offset) {
                first_offset = ph.p_offset;
                stageloader_vmem_start = ph.p_vaddr;
                loaded_first_offset = true;
            }
        }
        if (!loaded_first_offset) {
            puts("FATAL ERROR: No loadable sections found in stageloader binary");
        	for (;;) {
        		asm volatile("wfe");
        	}
        }
        if (stageloader_vmem_start < first_offset) {
            puts("FATAL ERROR: Stageloader binary is not properly relocated");
        	for (;;) {
        		asm volatile("wfe");
        	}
        }
        stageloader_vmem_start -= first_offset;
        if (stageloader_vmem_start % 0x1000 != 0) {
            puts("FATAL ERROR: Stageloader binary is not properly page aligned");
        	for (;;) {
        		asm volatile("wfe");
        	}
        }
    }
    puts("Stageloader virtual start addr: ");
    put_hex(stageloader_vmem_start);
	puts("Examining ELF header: ");

	// The embedded image is probably not aligned
	ELF64_header elf64_header{};
	const ELF64_header &elf64_header_unaligned = kernel.get_elf64_header();
    memcpy(&elf64_header, &elf64_header_unaligned, sizeof(elf64_header));

	puts(".\n");
    uint64_t entrypoint_addr = elf64_header.e_entry;
    puts("Kernel entrypoint: ");
    put_hex(supervisor_start + entrypoint_addr);
    puts("\n");
    uintptr_t kernel_vmem_start;
    {
        bool loaded_first_offset{false};
        uintptr_t first_offset;
        for (uint16_t i = 0; i < elf64_header.e_phnum; i++) {
        	ELF64_program_entry ph{};
            const auto &ph_unaligned = elf64_header_unaligned.get_program_entry_unaligned(i);
        	memcpy(&ph, &ph_unaligned, sizeof(ph));
            if (ph.p_type != PHT_LOAD || ph.p_memsz <= 0) {
                continue;
            }
            if (!loaded_first_offset || ph.p_offset < first_offset) {
                first_offset = ph.p_offset;
                kernel_vmem_start = ph.p_vaddr;
                loaded_first_offset = true;
            }
        }
        if (!loaded_first_offset) {
            puts("FATAL ERROR: No loadable sections found in kernel binary\n");
        	for (;;) {
        		asm volatile("wfe");
        	}
        }
        if (kernel_vmem_start < first_offset) {
            puts("FATAL ERROR: Kernel binary is not properly relocated\n");
        	for (;;) {
        		asm volatile("wfe");
        	}
        }
        kernel_vmem_start -= first_offset;
        if (kernel_vmem_start % 0x1000 != 0) {
            puts("FATAL ERROR: Kernel binary is not properly page aligned\n");
        	for (;;) {
        		asm volatile("wfe");
        	}
        }
    }
    puts("Kernel virtual start addr: ");
    put_hex(supervisor_start + kernel_vmem_start);
	puts("\n");

	{
		uint32_t ph_page_start{static_cast<uint32_t>(reinterpret_cast<phys_t>(phys_kernel) / 0x1000)};
		for (uint32_t i = 0; i < kernel_pages; i++) {
			uint64_t vaddr{kernel_vmem_start + (static_cast<uint64_t>(i) * 0x1000)};
			uint64_t ph_page_addr{ph_page_start + i};
			ph_page_addr *= 0x1000;

			map_page(sppmap, root_pt, vaddr, ph_page_addr, 0);
		}
	}

    for (uint16_t i = 0; i < elf64_header.e_shnum; i++) {
    	ELF64_section_entry section{};
        const auto &section_unaligned = elf64_header_unaligned.get_section_entry_unaligned(i);
    	memcpy(&section, &section_unaligned, sizeof(section));
        if (section.sh_addr != 0 && section.sh_size != 0) {
            u64 vaddr = section.sh_addr;
            u64 end_vaddr = vaddr + section.sh_size;
            vaddr = vaddr & ~0xFFFULL;
            for (; vaddr < end_vaddr; vaddr += 0x1000) {
                pageentr *pe = get_pageentr(nullptr, root_pt, vaddr);
                if (pe != nullptr) {
                    if (section.sh_flags & SHF_WRITE) {
                        pe->ap() = 0;
                    }
                    if (section.sh_flags & SHF_EXECINSTR) {
                        pe->pxn() = 0;
                    }
                }
            }
        }
    }

    {
        const auto *rela_dyn_unaligned = elf64_header_unaligned.get_rela_dyn_section_unaligned();
        if (rela_dyn_unaligned != nullptr) {
			ELF64_section_entry rela_dyn{};
			memcpy(&rela_dyn, rela_dyn_unaligned, sizeof(rela_dyn));
            ELF64_rela_dyn *rela_dyns_unaligned = (ELF64_rela_dyn *) (void *) (((uint8_t *) &(elf64_header_unaligned.start)) + rela_dyn.sh_offset);
            /* X - May not handle very big images (in the astronomical range >4G->1TB) due to overflow */
            static_assert((3 << 3) == sizeof(*rela_dyns_unaligned));
            auto rela_dyn_count = (uint32_t) (rela_dyn.sh_size >> 3);
            rela_dyn_count = rela_dyn_count / 3;
            for (uint32_t i = 0; i < rela_dyn_count; i++) {
            	typename std::remove_const<typename std::remove_reference<decltype(rela_dyns_unaligned[i])>::type>::type rela_dyn{};
            	memcpy(&rela_dyn, &(rela_dyns_unaligned[i]), sizeof(rela_dyn));
                switch (rela_dyn.rela_type) {
                    case R_AARCH64_RELATIVE: {
                        if (rela_dyn.sym_index != 0) {
                        	puts("rela_dyn sym index invalid\n");
                        	for (;;) {
                        		asm volatile("wfe");
                        	}
                        }
                        pageentr *pe = get_pageentr(nullptr, root_pt, rela_dyn.offset & ~((uint64_t) 0xFFF));
                        if (pe == nullptr) {
                        	puts("rela_dyn out of valid range\n");
                        	for (;;) {
                        		asm volatile("wfe");
                        	}
                        }
                        uint64_t phys = static_cast<uint64_t>(pe->ppn()) << 12;
                        phys += rela_dyn.offset & 0xFFF;
                        *((uint64_t *) phys) += rela_dyn.addendum;
                    }
                    break;
                }
            }
        }
    }
	puts("Kernel image ready\n");

	puts("Allocating shimstage area...\n");
	void *phys_stageloader;
	uint32_t stageloader_start_page;
	{
		std::optional<u32> phys_stageloader_addr = allocate_jump_physpage(sppmap, stageloader_pages, vpalloc, supervisor_start);
		if (!phys_stageloader_addr){
			puts("FATAL ERROR: Failed to allocate stageloader pages\n");
			for (;;) {
				asm volatile("wfe");
			}
		}
		stageloader_start_page = *phys_stageloader_addr;
		phys_stageloader = reinterpret_cast<void *>(static_cast<u64>(stageloader_start_page) * PAGE_SIZE);
	}
	memcpy(phys_stageloader, stageloader_src, stageloader_size);
	puts("Shimstage installed at: ");
	put_hex(reinterpret_cast<u64>(phys_stageloader));
	puts("\n");

	constexpr uint32_t stageloader_stack_pages = 2;
	uint32_t stageloader_stack_start_page;
	{
		std::optional<u32> phys_stack_addr = allocate_jump_physpage(sppmap, stageloader_stack_pages, vpalloc, supervisor_start);
		if (!phys_stack_addr){
			puts("FATAL ERROR: Failed to allocate stageloader stack pages\n");
			for (;;) {
				asm volatile("wfe");
			}
		}
		stageloader_stack_start_page = *phys_stack_addr;
	}
	u64 stageloader_sp = static_cast<u64>(stageloader_stack_start_page + stageloader_stack_pages) * PAGE_SIZE;
	puts("Shimstage stack top at: ");
	put_hex(stageloader_sp);
	puts("\n");

	// Map stageloader pages (identity mapped)
	for (uint32_t i = 0; i < stageloader_pages; ++i) {
		u64 p = static_cast<u64>(stageloader_start_page + i) * PAGE_SIZE;
		map_page(sppmap, root_pt, p, p, MapFlagWrite | MapFlagExecutable);
	}

	// Map stageloader stack pages (identity mapped)
	for (uint32_t i = 0; i < stageloader_stack_pages; ++i) {
		u64 p = static_cast<u64>(stageloader_stack_start_page + i) * PAGE_SIZE;
		map_page(sppmap, root_pt, p, p, MapFlagWrite);
	}

	int stack_pages = 7;
	++stack_pages;
	u64 stack_size{static_cast<u64>(stack_pages)};
	stack_size *= PAGE_SIZE;
	auto o_stack_vaddr = vpalloc.TryAllocate(stack_size);
	if (!o_stack_vaddr) {
		puts("FATAL ERROR: Unable to allocate CPU-boot stack virtual address\n");
		for (;;) {
			asm volatile("wfe");
		}
	}
	u64 stack_vaddr = *o_stack_vaddr;
	auto o_stack_phys = allocate_physpage(sppmap, stack_pages - 1);
	if (!o_stack_phys) {
		puts("FATAL ERROR: Unable to allocate CPU-boot stack physical pages\n");
		for (;;) {
			asm volatile("wfe");
		}
	}
	u32 stack_pageaddr = *o_stack_phys;
	for (int i = 1; i < stack_pages; i++) {
		u64 vaddr = stack_vaddr + (static_cast<u64>(i) * PAGE_SIZE);
		u64 paddr = static_cast<u64>(stack_pageaddr + (i - 1)) * PAGE_SIZE;
		map_page(sppmap, root_pt, vaddr, paddr, MapFlagWrite);
	}
	u64 stack_addr = stack_vaddr + stack_size;
	stack_addr &= ~0xFULL;

	// Map PL011 UART so console output continues working
	map_page(sppmap, root_pt, PL011_BASE, PL011_BASE, MapFlagWrite | MapFlagDevice);

	// Map DTB if present
	if (pagesearch.n_res > 1) {
		u64 dtb_start_page = pagesearch.res_start[1] & ~0xFFFULL;
		u64 dtb_end_page = (pagesearch.res_end[1] + 0xFFFULL) & ~0xFFFULL;
		for (u64 p = dtb_start_page; p < dtb_end_page; p += PAGE_SIZE) {
			map_page(sppmap, root_pt, p, p, MapFlagNone);
		}
	}

	stageloader_sp -= sizeof(ArmStageContext);
	stageloader_sp &= ~0xFULL;

	ArmStageContext *ctx = reinterpret_cast<ArmStageContext *>(stageloader_sp);
	ctx->ttbr = static_cast<u64>(kernel_root_pt_phys) * PAGE_SIZE;
	ctx->kernel_entrypoint = supervisor_start + entrypoint_addr;
	ctx->kernel_sp = stack_addr;
	ctx->dtb = dtb;

	u64 stageloader_entry = stageloader_entrypoint_addr + reinterpret_cast<u64>(phys_stageloader);

	puts("Jumping to stageloader at: ");
	put_hex(stageloader_entry);
	puts("\n");

	asm volatile(
		"mov sp, %0\n"
		"mov x0, %1\n"
		"br %2\n"
		:
		: "r"(stageloader_sp), "r"(reinterpret_cast<u64>(ctx)), "r"(stageloader_entry)
		: "x0"
	);

	for (;;) {
		asm volatile("wfe");
	}
}
