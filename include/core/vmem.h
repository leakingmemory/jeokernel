//
// Created by sigsegv on 01.05.2021.
//

#ifndef JEOKERNEL_VMEM_H
#define JEOKERNEL_VMEM_H

#include <cstdint>

class vmem_page {
private:
    uint64_t addr;
public:
    vmem_page(uint64_t addr) : addr(addr) {
    }

    void rmap(uint64_t paddr);
    void rwmap(uint64_t paddr, bool write_through = false, bool cache_disabled = false);
    void unmap();
};

class vmem {
private:
    uint64_t base;
    uint64_t size;
public:
    vmem(uint64_t size);
    ~vmem() {
        release();
    }

    void release();

    constexpr std::size_t pagesize() const {
        return 4096;
    }
    std::size_t npages() const;
    vmem_page page(std::size_t pnum);

    void reload();

    void *pointer() const {
        return (void *) base;
    }

    vmem(vmem &&mv) : base(mv.base), size(mv.size) {
        mv.base = 0;
    }
    vmem &operator =(vmem &&mv) {
        if (base != 0) {
            release();
        }
        base = mv.base;
        size = mv.size;
        mv.base = 0;
        return *this;
    }

    // No copies
    vmem(vmem &) = delete;
    vmem &operator =(vmem &) = delete;
};


#endif //JEOKERNEL_VMEM_H
