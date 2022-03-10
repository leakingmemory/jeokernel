//
// Created by sigsegv on 23.07.2021.
//

#ifndef JEOKERNEL_CHAINEDALLOCATOR_H
#define JEOKERNEL_CHAINEDALLOCATOR_H

#include "mallocator.h"

class ChainedAllocatorRoot;

class ChainedAllocator : public MemoryAllocator {
    friend ChainedAllocatorRoot;
private:
    MemoryAllocator *next;
    MemoryAllocator *allocator;
public:
    ChainedAllocator(MemoryAllocator *allocator) : MemoryAllocator(), allocator(allocator), next(nullptr) {
    }
    virtual ~ChainedAllocator();
    void *sm_allocate(uint32_t sz) override;
    uint32_t sm_free(void *ptr) override;
    uint32_t sm_sizeof(void *ptr) override;
    bool sm_owned(void *ptr) override;

    uint64_t sm_total_size() override;
    uint64_t sm_allocated_size() override;
};

ChainedAllocator *CreateChainedAllocator();


#endif //JEOKERNEL_CHAINEDALLOCATOR_H
