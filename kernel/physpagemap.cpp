//
// Created by sigsegv on 3/16/22.
//

#include <physpagemap.h>
#include <new>
#include <core/vmem.h>
#include <memory>
#include <vector>
#include <pagealloc.h>
#include <klogger.h>

class extendable_physpagemap_managed;

class simple_physpagemap_managed : public physpagemap_managed {
    friend extendable_physpagemap_managed;
private:
    PhyspageMap *map;
public:
    explicit simple_physpagemap_managed(uint64_t addr) : physpagemap_managed(), map((PhyspageMap *) (void *) addr) {}
    ~simple_physpagemap_managed() override = default;
    void claim(uint32_t pageaddr) override;
    void release(uint32_t pageaddr) override;
    bool claimed(uint32_t pageaddr) override;
    uint32_t max() override;
    void set_max(uint32_t max) override;
};

void simple_physpagemap_managed::claim(uint32_t pageaddr) {
    map->claim(pageaddr);
}

void simple_physpagemap_managed::release(uint32_t pageaddr) {
    map->release(pageaddr);
}

bool simple_physpagemap_managed::claimed(uint32_t pageaddr) {
    return map->claimed(pageaddr);
}

uint32_t simple_physpagemap_managed::max() {
    return map->mappageaddr_to_pageaddr(1);
}

void simple_physpagemap_managed::set_max(uint32_t max) {
}

physpagemap_managed *physp = nullptr;
uint8_t space_for_simple_physpagemap[sizeof(simple_physpagemap_managed)];

void init_simple_physpagemap(uint64_t mapaddr) {
    physp = new ((void *) &(space_for_simple_physpagemap[0])) simple_physpagemap_managed(mapaddr);
}

physpagemap_managed *get_physpagemap() {
    return physp;
}

class extendable_physpagemap_managed : public simple_physpagemap_managed {
private:
    std::unique_ptr<vmem> vm;
    std::vector<uint64_t> phys;
    uint32_t size;
public:
    extendable_physpagemap_managed(const simple_physpagemap_managed &original) : simple_physpagemap_managed((uint64_t) original.map) {
        vm = std::make_unique<vmem>(sizeof(*map));
        phys.push_back((uint64_t) map);
        memmap(*vm, phys, 1);
        map = (PhyspageMap *) vm->pointer();
        size = this->simple_physpagemap_managed::max();
    }
    static void memmap(vmem &vm, std::vector<uint64_t> &pages, int need_pages);
    void relocate();
    uint32_t max() override;
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

uint32_t extendable_physpagemap_managed::max() {
    return size;
}

void extendable_physpagemap_managed::set_max(uint32_t max) {
    uint32_t pages = max / this->simple_physpagemap_managed::max();
    if ((max % this->simple_physpagemap_managed::max()) != 0) {
        ++pages;
    }
    if (pages > phys.size()) {
        std::unique_ptr<vmem> nvm = std::make_unique<vmem>(sizeof(*map) * pages);
        memmap(*nvm, phys, pages);
        map = (PhyspageMap *) nvm->pointer();
        vm = std::move(nvm);
    }
    size = max;
}

void extend_to_advanced_physpagemap(uint64_t base_mapaddr) {
    physp = new extendable_physpagemap_managed(*((simple_physpagemap_managed *) physp));
}

