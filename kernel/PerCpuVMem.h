//
// Created by sigsegv on 6/16/22.
//

#ifndef JEOKERNEL_PERCPUVMEM_H
#define JEOKERNEL_PERCPUVMEM_H

#include <pagealloc.h>
#include <vector>

class PerCpuVMem {
private:
    VPerCpuPagetables pagetables;
    std::vector<uint16_t> freePages;
    PerCpuVMem();
public:
    static PerCpuVMem &Instance();
    uintptr_t AllocStack();
};


#endif //JEOKERNEL_PERCPUVMEM_H
