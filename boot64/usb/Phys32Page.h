//
// Created by sigsegv on 6/13/21.
//

#ifndef JEOKERNEL_PHYS32PAGE_H
#define JEOKERNEL_PHYS32PAGE_H

#include <cstdint>
#include <core/vmem.h>

class Phys32Page {
    vmem vm;
    size_t size;
    uint32_t physaddr;
public:
    explicit Phys32Page(size_t size);
    ~Phys32Page();
    void *Pointer() const {
        return vm.pointer();
    }
    uint32_t PhysAddr() {
        return physaddr;
    }
};


#endif //JEOKERNEL_PHYS32PAGE_H
