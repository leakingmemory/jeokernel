//
// Created by sigsegv on 23.07.2021.
//

#ifndef JEOKERNEL_CHAINEDALLOCATOR_H
#define JEOKERNEL_CHAINEDALLOCATOR_H

#include "mallocator.h"

class ChainedAllocator : public MemoryAllocator {
private:
    ChainedAllocator *next;
    MemoryAllocator *allocator;
    hw_spinlock chain_lock;
public:
    ChainedAllocator(MemoryAllocator *allocator) : MemoryAllocator(), allocator(allocator), next(nullptr), chain_lock() {
    }
    virtual ~ChainedAllocator();
    void *sm_allocate(uint32_t sz) override;
    void sm_free(void *ptr) override;
    uint32_t sm_sizeof(void *ptr) override;
    bool sm_owned(void *ptr) override;
};

ChainedAllocator *CreateChainedAllocator();


#endif //JEOKERNEL_CHAINEDALLOCATOR_H