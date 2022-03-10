//
// Created by sigsegv on 3/9/22.
//

#ifndef JEOKERNEL_CHAINEDALLOCATORROOT_H
#define JEOKERNEL_CHAINEDALLOCATORROOT_H

#include "mallocator.h"

class ChainedAllocatorRoot : public MemoryAllocator {
private:
    MemoryAllocator *head;
    hw_spinlock chain_lock;
public:
    ChainedAllocatorRoot();
    ChainedAllocatorRoot(MemoryAllocator *head);
    MemoryAllocator *get_head();
    void *sm_allocate(uint32_t sz) override;
    uint32_t sm_free(void *ptr) override;
    uint32_t sm_sizeof(void *ptr) override;
    bool sm_owned(void *ptr) override;

    uint64_t sm_total_size() override;
    uint64_t sm_allocated_size() override;
};

ChainedAllocatorRoot *CreateChainedAllocatorRoot();


#endif //JEOKERNEL_CHAINEDALLOCATORROOT_H
