//
// Created by sigsegv on 3/16/22.
//

#include <physpagemap.h>
#include <new>
#ifndef LOADER
#include <core/vmem.h>
#include <memory>
#include <vector>
#include <pagealloc.h>
#include <klogger.h>
#endif
#include <std/limits.h>

#ifndef LOADER
class extendable_physpagemap_managed;
#endif

class simple_physpagemap_managed : public physpagemap_managed {
#ifndef LOADER
    friend extendable_physpagemap_managed;
#endif
private:
    PhyspageMap *map;
    uint32_t base_addr;
public:
    explicit simple_physpagemap_managed(PhyspageMap *map, uint32_t base_addr) : physpagemap_managed(), map(map), base_addr(base_addr) {}
#ifndef LOADER
    explicit simple_physpagemap_managed(uint64_t addr, uint32_t base_addr) : physpagemap_managed(), map((PhyspageMap *) (void *) addr), base_addr(base_addr) {}
    ~simple_physpagemap_managed() override = default;
#else
    ~simple_physpagemap_managed() = default;
#endif
    void claim(uint32_t pageaddr) override;
    void claim(uint32_t pageaddr, uint32_t num) override;
    void release(uint32_t pageaddr) override;
    bool claimed(uint32_t pageaddr) override;
    uint32_t max() const override;
    void set_max(uint32_t max) override;
};

void simple_physpagemap_managed::claim(uint32_t pageaddr) {
    if (pageaddr >= max()) {
        return;
    }
    if (pageaddr < base_addr) {
        return;
    }
    pageaddr -= base_addr;
    map->claim(pageaddr);
}

void simple_physpagemap_managed::claim(uint32_t pageaddr, uint32_t num) {
    if (pageaddr >= max()) {
        return;
    }
    if (pageaddr >= base_addr) {
        pageaddr -= base_addr;
    } else {
        auto diff = base_addr - pageaddr;
        if (diff >= num) {
            return;
        }
        pageaddr = 0;
        num -= diff;
    }
    uint32_t overflmax = std::numeric_limits<uint32_t>::max() - pageaddr;
    if (num > overflmax) {
        num = overflmax;
    }
    auto rel_max = max() - base_addr;
    if ((pageaddr + num) > rel_max) {
        num = rel_max - pageaddr;
    }
    map->claim(pageaddr, num);
}

void simple_physpagemap_managed::release(uint32_t pageaddr) {
    if (pageaddr >= max()) {
        return;
    }
    if (pageaddr < base_addr) {
        return;
    }
    pageaddr -= base_addr;
    map->release(pageaddr);
}

bool simple_physpagemap_managed::claimed(uint32_t pageaddr) {
    if (pageaddr >= max()) {
        return true;
    }
    if (pageaddr < base_addr) {
        return true;
    }
    pageaddr -= base_addr;
    return map->claimed(pageaddr);
}

uint32_t simple_physpagemap_managed::max() const {
    return map->mappageaddr_to_pageaddr(1) + base_addr;
}

void simple_physpagemap_managed::set_max(uint32_t max) {
}

physpagemap_managed *physp = nullptr;

#ifdef LOADER
uint8_t space_for_simple_physpagemap[sizeof(simple_physpagemap_managed)];

physpagemap_managed *new_simple_physpagemap_for_loader(PhyspageMap *ppmap, uint32_t base_addr) {
    return new ((void *) &(space_for_simple_physpagemap[0])) simple_physpagemap_managed(ppmap, base_addr);
}
#else

void init_simple_physpagemap(uint64_t mapaddr, uint64_t base_addr) {
    physp = new ((void *) &(space_for_simple_physpagemap[0])) simple_physpagemap_managed(mapaddr, base_addr);
}
#endif

physpagemap_managed *get_physpagemap() {
    return physp;
}

#ifndef LOADER
class extendable_physpagemap_managed : public simple_physpagemap_managed {
private:
    std::unique_ptr<vmem> vm;
    std::vector<uint64_t> phys;
    uint32_t size;
public:
    extendable_physpagemap_managed(const simple_physpagemap_managed &original) : simple_physpagemap_managed((uint64_t) original.map, original.base_addr) {
        vm = std::make_unique<vmem>(sizeof(*map));
        phys.push_back((uint64_t) map);
        memmap(*vm, phys, 1);
        map = (PhyspageMap *) vm->pointer();
        size = this->simple_physpagemap_managed::max() - base_addr;
    }
    static void memmap(vmem &vm, std::vector<uint64_t> &pages, int need_pages);
    void relocate();
    uint32_t max() const override;
    void set_max(uint32_t max) override;
};

void extendable_physpagemap_managed::memmap(vmem &vm, std::vector<uint64_t> &pages, int need_pages) {
    while (pages.size() < need_pages) {
        uint64_t phys = ppagealloc(sizeof(PhyspageMap));
        if (phys == 0) {
            wild_panic("out of memory, phys pages");
        }
        pages.push_back(phys);
    }
    int i = 0;
    for (uint64_t page : pages) {
        vm.page(i).rwmap(page);
        ++i;
    }
}

uint32_t extendable_physpagemap_managed::max() const {
    return base_addr + size;
}

void extendable_physpagemap_managed::set_max(uint32_t max) {
    if (max >= base_addr) {
        max -= base_addr;
    } else {
        max = 0;
    }
    auto bits_per_page = this->simple_physpagemap_managed::max() - base_addr;
    uint32_t pages = max / bits_per_page;
    if ((max % bits_per_page) != 0) {
        ++pages;
    }
    uint32_t pages_needed = ((sizeof(*map) * pages) + PAGESIZE - 1) / PAGESIZE;
    if (pages_needed > phys.size()) {
        std::unique_ptr<vmem> nvm = std::make_unique<vmem>(sizeof(*map) * pages);
        memmap(*nvm, phys, pages_needed);
        map = (PhyspageMap *) nvm->pointer();
        vm = std::move(nvm);
    }
    size = max;
}

void extend_to_advanced_physpagemap() {
    physp = new extendable_physpagemap_managed(*((simple_physpagemap_managed *) physp));
}
#endif

