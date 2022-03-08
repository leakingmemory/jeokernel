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
        next->~ChainedAllocator();
    }
    allocator->~MemoryAllocator();
}

void * ChainedAllocator::sm_allocate(uint32_t sz) {
    void *ptr = allocator->sm_allocate(sz);
    if (ptr == nullptr) {
        {
            std::lock_guard lock(chain_lock);
            if (next == nullptr) {
                next = CreateChainedAllocator();
            }
        }
        ptr = next->sm_allocate(sz);
    }
    return ptr;
}

void ChainedAllocator::sm_free(void *ptr) {
    if (allocator->sm_owned(ptr)) {
        allocator->sm_free(ptr);
    } else if (next != nullptr) {
        next->sm_free(ptr);
    } else {
        wild_panic("Free ptr not ours");
    }
}

bool ChainedAllocator::sm_owned(void *ptr) {
    if (allocator->sm_owned(ptr)) {
        return true;
    }
    ChainedAllocator *n = nullptr;
    {
        std::lock_guard lock{chain_lock};
        n = next;
    }
    if (n != nullptr) {
        return n->sm_owned(ptr);
    } else {
        return false;
    }
}

uint32_t ChainedAllocator::sm_sizeof(void *ptr) {
    if (allocator->sm_owned(ptr)) {
        return allocator->sm_sizeof(ptr);
    }
    ChainedAllocator *n = nullptr;
    {
        std::lock_guard lock{chain_lock};
        n = next;
    }
    if (n != nullptr) {
        return n->sm_sizeof(ptr);
    } else {
        wild_panic("Sizeof ptr not ours");
    }
}
