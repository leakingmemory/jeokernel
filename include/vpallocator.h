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

struct VPAllocatorPage;

struct VPAllocatorLink {
	union {
		phys_t addr;
		VPAllocatorPage *ptr;
	} paddr;
	union {
		uintptr_t addr;
		VPAllocatorPage *ptr;
	} vaddr;

	constexpr VPAllocatorLink() noexcept {
		paddr.addr = 0;
		paddr.ptr = nullptr;
		vaddr.ptr = nullptr;
	}
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

	constexpr bool HasFree() const {
		return data.num > 0;
	}
	constexpr std::optional<uintptr_t> GetStart() const {
		if (data.num > 0) {
			return { ranges[0].start };
		} else {
			return {};
		}
	}
	constexpr std::optional<uintptr_t> GetEnd() const {
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
							ranges[data.num - j] = ranges[data.num - j - 1];
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
	constexpr VPAllocatorPage *GetNext() {
		if (next.vaddr.ptr != nullptr) {
			return next.vaddr.ptr;
		} else if (next.paddr.ptr != nullptr) {
			return next.paddr.ptr;
		} else {
			return nullptr;
		}
	}
	constexpr void SetNextVirtual(VPAllocatorPage *n) {
		next.vaddr.ptr = n;
		next.paddr.addr = 0;
	}
	constexpr void SetNextPhysical(VPAllocatorPage *n) {
		next.vaddr.ptr = nullptr;
		next.paddr.addr = 0;
		next.paddr.ptr = n;
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

template <class T> concept VPAllocatorPageAllocator = requires(T allocator) {
	{ allocator.TryAllocate() } -> std::same_as<std::optional<VPAllocatorPage *>>;
	{ allocator.Free(std::declval<VPAllocatorPage *>()) };
	{ allocator.IsVirtualAddress() } -> std::same_as<bool>;
};

template <VPAllocatorPageAllocator PageAllocator> class VPAllocator {
private:
	PageAllocator *pageAllocator;
	VPAllocatorPage *first{nullptr};
public:
	constexpr VPAllocator(PageAllocator *pageAllocator, VPAllocatorPage *root) : pageAllocator(pageAllocator), first(root) {
	}
	constexpr ~VPAllocator() = default;

	constexpr void MoveOneUp(VPAllocatorPage *page, VPAllocatorPage *next) {
		if (next->data.num >= next->num_records || page->data.num <= 0) {
			return;
		}
		next->ranges[next->data.num++] = page->ranges[--page->data.num];
	}
	constexpr void MoveOneUp(VPAllocatorPage *page) {
		auto *next = page->GetNext();
		if (next != nullptr) {
			MoveOneUp(page, next);
		}
	}
	constexpr VPAllocatorPage *MoreRoom(VPAllocatorPage *page, VPAllocatorPage *next) {
		if (page == nullptr) {
			return next;
		}
		if (next == nullptr || (next->num_records - next->data.num) < 2) {
			{
				std::optional<VPAllocatorPage *> perhapsNewpage = pageAllocator->TryAllocate();
				if (!perhapsNewpage) {
					return next;
				}
				if (next != nullptr) {
					(*perhapsNewpage)->next = page->next;
				}
				next = *perhapsNewpage;
			}
			if (pageAllocator->IsVirtualAddress()) {
				page->SetNextVirtual(next);
			} else {
				page->SetNextPhysical(next);
			}
			auto half_num = page->data.num / 2;
			if (half_num <= 0 && page->data.num > 0) {
				half_num = 1;
			}
			for (decltype(page->data.num) i = 0; i < half_num; i++) {
				MoveOneUp(page, next);
			}
			return next;
		}
		MoveOneUp(page, next);
		MoveOneUp(page, next);
		return next;
	}
	constexpr VPAllocatorResult TryAllocateFromAddress(uintptr_t addr, uintptr_t size) {
		if (pageAllocator == nullptr) {
			return VPAllocatorResult::ERROR;
		}
		auto *allocator = first;
		while (allocator != nullptr) {
			auto *next = allocator->GetNext();
			if (allocator->HasFree()) {
				if (allocator->GetStart() > addr) {
					return VPAllocatorResult::ERROR;
				} else if (allocator->GetEnd() < addr) {
					allocator = next;
					continue;
				} else {
					auto result = allocator->TryAllocateFromAddress(addr, size);
					if (result == VPAllocatorResult::NO_SPACE) {
						next = MoreRoom(allocator, next);
						result = allocator->TryAllocateFromAddress(addr, size);
					}
					if (result == VPAllocatorResult::DONE || result == VPAllocatorResult::NO_SPACE) {
						return result;
					}
				}
			}
			allocator = next;
		}
		return VPAllocatorResult::ERROR;
	}
	constexpr std::optional<uintptr_t> TryAllocate(uintptr_t size) {
		if (pageAllocator == nullptr) {
			return {};
		}
		auto *allocator = first;
		while (allocator != nullptr) {
			if (allocator->HasFree()) {
				auto result = allocator->TryAllocate(size);
				if (result) {
					return result;
				}
			}
			allocator = allocator->GetNext();
		}
		return {};
	}
	constexpr VPAllocatorResult TryFree(uintptr_t addr, uintptr_t size) {
		if (pageAllocator == nullptr) {
			return VPAllocatorResult::ERROR;
		}
		auto *allocator = first;
		decltype(allocator) theEndingPage = nullptr;
		uintptr_t end{};
		bool allEmpty{true};
		while (allocator != nullptr) {
			auto *next = allocator->GetNext();
			if (allocator->HasFree() || !allEmpty) {
				allEmpty = false;
				if (allocator->HasFree()) {
					theEndingPage = allocator;
					end = allocator->GetEnd();
				}
				if (end < addr && next != nullptr) {
					allocator = next;
					continue;
				} else {
					auto result = allocator->TryFree(addr, size);
					if (result == VPAllocatorResult::NO_SPACE) {
						next = MoreRoom(allocator, next);
						result = allocator->TryFree(addr, size);
					}
					if (result == VPAllocatorResult::DONE || result == VPAllocatorResult::NO_SPACE) {
						return result;
					}
				}
			}
			allocator = next;
		}
		if (allEmpty && first != nullptr) {
			return first->TryFree(addr, size);
		}
		if (theEndingPage != nullptr) {
			auto result = theEndingPage->TryFree(addr, size);
			if (result == VPAllocatorResult::NO_SPACE) {
				MoreRoom(theEndingPage, theEndingPage->GetNext());
				return TryFree(addr, size);
			}
			return result;
		}
		return VPAllocatorResult::ERROR;
	}
};

constexpr bool VPAllocatorTest2() {
	VPAllocatorPage pg{};
	size_t size{0x10000000};
	size_t off{0};
	while (size > 0) {
		if (pg.TryAllocate(size)) {
			return false;
		}
		{
			auto free_res = pg.TryFree(off, size);
			if (free_res == VPAllocatorResult::NO_SPACE) {
				return true;
			}
			if (free_res != VPAllocatorResult::DONE) {
				return false;
			}
		}
		{
			auto allocres = pg.TryAllocate(size);
			if (!allocres || *allocres != off) {
				return false;
			}
		}
		if (pg.TryAllocate(size)) {
			return false;
		}
		{
			auto free_res = pg.TryFree(off, size);
			if (free_res == VPAllocatorResult::NO_SPACE) {
				return true;
			}
			if (free_res != VPAllocatorResult::DONE) {
				return false;
			}
		}
		size -= 0x2000;
		{
			auto allocres = pg.TryAllocate(0x2000);
			if (!allocres || *allocres != off) {
				return false;
			}
		}
		{
			off += 0x2000;
			auto allocres = pg.TryAllocate(size);
			if (!allocres || *allocres != off) {
				return false;
			}
			{
				auto free_res = pg.TryFree(off - 0x2000, 0x1000);
				if (free_res == VPAllocatorResult::NO_SPACE) {
					return true;
				}
				if (free_res != VPAllocatorResult::DONE) {
					return false;
				}
			}
			{
				auto free_res = pg.TryFree(off + size - 0x1000, 0x1000);
				if (free_res == VPAllocatorResult::NO_SPACE) {
					return true;
				}
				if (free_res != VPAllocatorResult::DONE) {
					return false;
				}
			}
		}
		size -= 0x2000;
	}
	return true;
}
static_assert(VPAllocatorTest2());

class VPTestPageAllocator {
private:
	VPAllocatorPage pages[64] = {};
	bool pagesInUse[64] = {};
public:
	constexpr VPTestPageAllocator() = default;
	constexpr ~VPTestPageAllocator() noexcept = default;
	constexpr std::optional<VPAllocatorPage *> TryAllocate() {
		for (size_t i = 0; i < 64; i++) {
			if (!pagesInUse[i]) {
				pagesInUse[i] = true;
				return &(pages[i]);
			}
		}
		return {};
	}
	constexpr void Free(VPAllocatorPage *p) {
	}
	constexpr bool IsVirtualAddress() {
		return true;
	}
};

constexpr bool VPAllocatorTest3() {
	VPTestPageAllocator testPageAllocator{};
	VPAllocatorPage root{};
	VPAllocator<VPTestPageAllocator> pg{&testPageAllocator, &root};
	size_t size{0x10000000};
	size_t off{0};
	for (size_t n = 0; n < 64; n++) {
		if (pg.TryAllocate(size)) {
			return false;
		}
		{
			auto free_res = pg.TryFree(off, size);
			if (free_res == VPAllocatorResult::NO_SPACE) {
				return true;
			}
			if (free_res != VPAllocatorResult::DONE) {
				return false;
			}
		}
		{
			auto allocres = pg.TryAllocate(size);
			if (!allocres || *allocres != off) {
				return false;
			}
		}
		if (pg.TryAllocate(size)) {
			return false;
		}
		{
			auto free_res = pg.TryFree(off, size);
			if (free_res == VPAllocatorResult::NO_SPACE) {
				return true;
			}
			if (free_res != VPAllocatorResult::DONE) {
				return false;
			}
		}
		size -= 0x2000;
		{
			auto allocres = pg.TryAllocate(0x2000);
			if (!allocres || *allocres != off) {
				return false;
			}
		}
		{
			off += 0x2000;
			auto allocres = pg.TryAllocate(size);
			if (!allocres || *allocres != off) {
				return false;
			}
			{
				auto free_res = pg.TryFree(off - 0x2000, 0x1000);
				if (free_res == VPAllocatorResult::NO_SPACE) {
					return true;
				}
				if (free_res != VPAllocatorResult::DONE) {
					return false;
				}
			}
			{
				auto free_res = pg.TryFree(off + size - 0x1000, 0x1000);
				if (free_res == VPAllocatorResult::NO_SPACE) {
					return true;
				}
				if (free_res != VPAllocatorResult::DONE) {
					return false;
				}
			}
		}
		size -= 0x2000;
	}
	return true;
}

static_assert(VPAllocatorTest3());

#endif
