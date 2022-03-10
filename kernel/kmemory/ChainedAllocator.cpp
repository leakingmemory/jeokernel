//
// Created by sigsegv on 23.07.2021.
//

#include <new>
#include <klogger.h>
#include <mutex>
#include "ChainedAllocator.h"
#include "concurrency/critical_section.h"

ChainedAllocator *CreateChainedAllocator() {
    auto allocator = CreateBasicMemoryAllocator();
    ChainedAllocator *chainedAllocator = (ChainedAllocator *) allocator->sm_allocate(sizeof(*chainedAllocator));
    chainedAllocator = new ((void *) chainedAllocator) ChainedAllocator(allocator);
    return chainedAllocator;
}

ChainedAllocator::~ChainedAllocator() noexcept {
    if (next != nullptr) {
        next->~MemoryAllocator();
    }
    allocator->~MemoryAllocator();
}

void * ChainedAllocator::sm_allocate(uint32_t sz) {
    void *ptr = allocator->sm_allocate(sz);
    if (ptr == nullptr) {
        if (next == nullptr) {
            return ptr;
        }
        ptr = next->sm_allocate(sz);
    }
    return ptr;
}

uint32_t ChainedAllocator::sm_free(void *ptr) {
    if (allocator->sm_owned(ptr)) {
        return allocator->sm_free(ptr);
    } else if (next != nullptr) {
        return next->sm_free(ptr);
    } else {
        return 0;
    }
}

bool ChainedAllocator::sm_owned(void *ptr) {
    if (allocator->sm_owned(ptr)) {
        return true;
    }
    if (next != nullptr) {
        return next->sm_owned(ptr);
    } else {
        return false;
    }
}

uint32_t ChainedAllocator::sm_sizeof(void *ptr) {
    if (allocator->sm_owned(ptr)) {
        return allocator->sm_sizeof(ptr);
    }
    if (next != nullptr) {
        return next->sm_sizeof(ptr);
    } else {
        wild_panic("Sizeof ptr not ours");
    }
}
