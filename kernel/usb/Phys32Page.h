//
// Created by sigsegv on 6/13/21.
//

#ifndef JEOKERNEL_PHYS32PAGE_H
#define JEOKERNEL_PHYS32PAGE_H

#include <cstdint>
#include <core/vmem.h>

class Phys32Page {
public:
    typedef uint32_t physaddr_t;
private:
    vmem vm;
    size_t size;
    physaddr_t physaddr;
public:
    explicit Phys32Page(size_t size);
    ~Phys32Page();
    void *Pointer() const {
        return vm.pointer();
    }
    size_t Size() {
        return size;
    }
    uint32_t PhysAddr() {
        return physaddr;
    }
};


#endif //JEOKERNEL_PHYS32PAGE_H
