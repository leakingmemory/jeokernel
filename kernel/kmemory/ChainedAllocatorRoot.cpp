//
// Created by sigsegv on 3/9/22.
//

#include <new>
#include <mutex>
#include "ChainedAllocatorRoot.h"
#include "ChainedAllocator.h"

ChainedAllocatorRoot::ChainedAllocatorRoot() : ChainedAllocatorRoot(CreateBasicMemoryAllocator()) {}

ChainedAllocatorRoot::ChainedAllocatorRoot(MemoryAllocator *head) : head(head), chain_lock() {
}

MemoryAllocator *ChainedAllocatorRoot::get_head() {
    std::lock_guard lock{chain_lock};
    return head;
}

void *ChainedAllocatorRoot::sm_allocate(uint32_t sz) {
    void *ptr = get_head()->sm_allocate(sz);
    if (ptr != nullptr) {
        return ptr;
    }
    ChainedAllocator *head = CreateChainedAllocator();
    ptr = head->sm_allocate(sz);
    if (ptr == nullptr) {
        head->~ChainedAllocator();
        return ptr;
    }
    {
        std::lock_guard lock{chain_lock};
        head->next = this->head;
        this->head = head;
    }
    return ptr;
}

void ChainedAllocatorRoot::sm_free(void *ptr) {
    return get_head()->sm_free(ptr);
}

uint32_t ChainedAllocatorRoot::sm_sizeof(void *ptr) {
    return get_head()->sm_sizeof(ptr);
}

bool ChainedAllocatorRoot::sm_owned(void *ptr) {
    return get_head()->sm_owned(ptr);
}

ChainedAllocatorRoot *CreateChainedAllocatorRoot() {
    BasicMemoryAllocator *memoryAllocator = CreateBasicMemoryAllocator();
    ChainedAllocatorRoot *allocatorRoot =
            new (memoryAllocator->sm_allocate(sizeof(ChainedAllocatorRoot)))
            ChainedAllocatorRoot(memoryAllocator);
    return allocatorRoot;
}