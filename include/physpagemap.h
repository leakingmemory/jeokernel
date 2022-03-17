//
// Created by sigsegv on 3/16/22.
//

#ifndef JEOKERNEL_PHYSPAGEMAP_H
#define JEOKERNEL_PHYSPAGEMAP_H

#include <cstdint>

struct PhyspageMap {
    uint32_t map[1024];

    PhyspageMap() : map() {
        for (unsigned int & i : map) {
            i = 0;
        }
    }

    constexpr void claim(uint32_t pageaddr) {
        uint32_t index = pageaddr >> 5;
        uint32_t bit = pageaddr & 0x1F;
        map[index] |= 1 << bit;
    }
    constexpr void release(uint32_t pageaddr) {
        uint32_t index = pageaddr >> 5;
        uint32_t bit = pageaddr & 0x1F;
        uint32_t mask = 1 << bit;
        map[index] &= ~mask;
    }
    constexpr bool claimed(uint32_t pageaddr) {
        uint32_t index = pageaddr >> 5;
        uint32_t bit = pageaddr & 0x1F;
        uint32_t mask = 1 << bit;
        return (map[index] & mask) != 0;
    }
    static constexpr uint32_t byteaddr_to_pageaddr(uint32_t size) {
        return size << 3;
    }
    static constexpr uint32_t mapaddr_to_pageaddr(uint32_t size) {
        return size << 5;
    }
    static constexpr uint32_t mappageaddr_to_pageaddr(uint32_t mappage) {
        return mappage << (12 + 3);
    }
};

static_assert(PhyspageMap::mappageaddr_to_pageaddr(1) == (4096*8));
static_assert((sizeof(PhyspageMap)*8) == PhyspageMap::mappageaddr_to_pageaddr(1));
static_assert(sizeof(PhyspageMap) == 4096);

class physpagemap_managed {
public:
    physpagemap_managed() {}
    virtual ~physpagemap_managed() {}
    physpagemap_managed(const physpagemap_managed &) = delete;
    physpagemap_managed(physpagemap_managed &&) = delete;
    physpagemap_managed &operator =(const physpagemap_managed &) = delete;
    physpagemap_managed &operator =(physpagemap_managed &&) = delete;

    virtual void claim(uint32_t pageaddr) = 0;
    virtual void release(uint32_t pageaddr) = 0;
    virtual bool claimed(uint32_t pageaddr) = 0;
    virtual uint32_t max() = 0;
    virtual void set_max(uint32_t max) = 0;
};

void init_simple_physpagemap(uint64_t mapaddr);
void extend_to_advanced_physpagemap(uint64_t base_mapaddr);
physpagemap_managed *get_physpagemap();

#endif //JEOKERNEL_PHYSPAGEMAP_H
