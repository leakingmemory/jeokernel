#ifndef VPALLOCATOR_H
#define VPALLOCATOR_H

#include <cstdint>
#include <limits>
#include <optional>

constexpr size_t pagesize = 4096;

struct VPAllocatorRange {
	uintptr_t start{0};
	uintptr_t end{0};

	constexpr VPAllocatorRange() = default;
};

struct VPAllocatorData {
	uint32_t num{0};

	constexpr VPAllocatorData() = default;
};

struct VPAllocatorLink {
	phys_t paddr{std::numeric_limits<phys_t>::max()};
	uintptr_t vaddr{std::numeric_limits<uintptr_t>::max()};
};

enum class VPAllocatorResult {
	DONE,
	NO_SPACE,
	ERROR
};

struct VPAllocatorPage {
	static constexpr size_t num_records = (pagesize - sizeof(VPAllocatorLink) - sizeof(VPAllocatorData)) / sizeof(VPAllocatorRange);
	VPAllocatorRange ranges[num_records];
	VPAllocatorLink next{};
	VPAllocatorData data{};

	constexpr VPAllocatorPage() = default;

	constexpr std::optional<uintptr_t> GetStart() {
		if (data.num > 0) {
			return { ranges[0].start };
		} else {
			return {};
		}
	}
	constexpr std::optional<uintptr_t> GetEnd() {
		if (data.num > 0) {
			return { ranges[data.num - 1].end };
		} else {
			return {};
		}
	}
	constexpr VPAllocatorResult TryAllocateFromAddress(uintptr_t addr, uintptr_t size) {
		if (data.num <= 0) {
			return VPAllocatorResult::ERROR;
		}
		for (decltype(data.num) i = 0; i < data.num; i++) {
			if (ranges[i].start == addr) {
				if (ranges[i].end == (addr + size)) {
					for (decltype(data.num) j = i + 1; j < data.num; j++) {
						ranges[j - 1] = ranges[j];
					}
					--data.num;
					return VPAllocatorResult::DONE;
				} else if (ranges[i].end > (addr + size)) {
					ranges[i].start = addr + size;
					return VPAllocatorResult::DONE;
				} else {
					return VPAllocatorResult::ERROR;
				}
			} else if (ranges[i].start < addr) {
				if (ranges[i].end == (addr + size)) {
					ranges[i].end = addr;
					return VPAllocatorResult::DONE;
				} else if (ranges[i].end > (addr + size)) {
					if (data.num >= num_records) {
						return VPAllocatorResult::NO_SPACE;
					}
					for (decltype(data.num) j = 0; j < (data.num - i - 1); j++) {
						ranges[data.num - j] = ranges[data.num - j - 1];
					}
					++data.num;
					ranges[i + 1].end = ranges[i].end;
					ranges[i + 1].start = addr + size;
					ranges[i].end = addr;
					return VPAllocatorResult::DONE;
				} else {
					return VPAllocatorResult::ERROR;
				}
			} else {
				return VPAllocatorResult::ERROR;
			}
		}
		return VPAllocatorResult::ERROR;
	}
	constexpr std::optional<uintptr_t> TryAllocate(uintptr_t size) {
		if (data.num <= 0) {
			return {};
		}
		{
			auto overrun = size % pagesize;
			if (overrun > 0) {
				overrun = pagesize - overrun;
			}
			size += overrun;
		}
		for (decltype(data.num) i = 0; i < data.num; i++) {
			auto avail = ranges[i].end - ranges[i].start;
			if (avail == size) {
				auto addr = ranges[i].start;
				for (decltype(data.num) j = i + 1; j < data.num; j++) {
					ranges[j - 1] = ranges[j];
				}
				--data.num;
				return { addr };
			} else if (avail > size) {
				auto addr = ranges[i].start;
				ranges[i].start += size;
				return { addr };
			}
		}
		return {};
	}
	constexpr VPAllocatorResult TryFree(uintptr_t addr, uintptr_t size) {
		{
			auto overrun = size % pagesize;
			if (overrun > 0) {
				overrun = pagesize - overrun;
			}
			size += overrun;
		}
		if (data.num == 0) {
			ranges[0].start = addr;
			ranges[0].end = addr + size;
			++data.num;
			return VPAllocatorResult::DONE;
		}
		if (size <= 0) {
			return VPAllocatorResult::DONE;
		}
		if (ranges[0].start > addr) {
			auto avail_to_free = ranges[0].start - addr;
			if (avail_to_free > size) {
				if (data.num >= num_records) {
					return VPAllocatorResult::NO_SPACE;
				}
				for (decltype(data.num) i = 1; i < data.num; i++) {
					ranges[num_records - i] = ranges[num_records - i - 1];
				}
				ranges[0].start = addr;
				ranges[0].end = addr + size;
				return VPAllocatorResult::DONE;
			} else if (avail_to_free == size) {
				ranges[0].start = addr;
				return VPAllocatorResult::DONE;
			} else {
				return VPAllocatorResult::ERROR;
			}
		}
		if (ranges[data.num - 1].end < addr) {
			if (data.num >= num_records) {
				return VPAllocatorResult::NO_SPACE;
			}
			ranges[data.num].start = addr;
			ranges[data.num].end = addr + size;
			++data.num;
			return VPAllocatorResult::DONE;
		} else if (ranges[data.num - 1].end == addr) {
			ranges[data.num - 1].end = addr + size;
			return VPAllocatorResult::DONE;
		}
		for (decltype(data.num) i = 0; i < (data.num - 1); i++) {
			if (addr == ranges[i].end) {
				if (ranges[i + 1].start == (addr + size)) {
					ranges[i].end = ranges[i + 1].end;
					for (decltype(data.num) j = i + 2; j < data.num; j++) {
						ranges[j - 1] = ranges[j];
					}
					--data.num;
					return VPAllocatorResult::DONE;
				} else if (ranges[i + 1].start > (addr + size)) {
					ranges[i].end = addr + size;
					return VPAllocatorResult::DONE;
				} else {
					return VPAllocatorResult::ERROR;
				}
			} else if (ranges[i].end < addr && ranges[i + 1].start > addr) {
				if ((addr + size) == ranges[i + 1].start) {
					ranges[i + 1].start = addr;
					return VPAllocatorResult::DONE;
				} else if ((addr + size) < ranges[i + 1].start) {
					if (data.num < num_records) {
						for (decltype(data.num) j = 0; j < (data.num - i - 1); j++) {
							ranges[data.num - j + i] = ranges[data.num - j + i - 1];
						}
						++data.num;
						ranges[i + 1].start = addr;
						ranges[i + 1].end = addr + size;
						return VPAllocatorResult::DONE;
					} else {
						return VPAllocatorResult::NO_SPACE;
					}
				} else {
					return VPAllocatorResult::ERROR;
				}
			}
		}
		return VPAllocatorResult::ERROR;
	}
	VPAllocatorPage *GetNext() {
		if (next.vaddr != std::numeric_limits<decltype(next.vaddr)>::max()) {
			return reinterpret_cast<VPAllocatorPage *>(next.vaddr);
		} else if (next.paddr != std::numeric_limits<decltype(next.paddr)>::max()) {
			return reinterpret_cast<VPAllocatorPage *>(static_cast<uintptr_t>(next.paddr));
		} else {
			return nullptr;
		}
	}
};

constexpr bool VPAllocatorTest1() {
	VPAllocatorPage pg{};
	if (pg.TryFree(0, 0x10000000) != VPAllocatorResult::DONE) {
		return false;
	}
	if (pg.TryAllocate(0x20000000)) {
		return false;
	}
	auto alloc_1pg = pg.TryAllocate(0x1000);
	if (!alloc_1pg) {
		return false;
	}
	auto alloc_1pg_2 = pg.TryAllocate(0x1000);
	if (!alloc_1pg_2) {
		return false;
	}
	if (*alloc_1pg == *alloc_1pg_2) {
		return false;
	}
	if (pg.TryFree(*alloc_1pg, 0x1000) != VPAllocatorResult::DONE) {
		return false;
	}
	if (pg.TryFree(*alloc_1pg_2, 0x1000) != VPAllocatorResult::DONE) {
		return false;
	}
	return true;
}
static_assert(VPAllocatorTest1());

#endif
