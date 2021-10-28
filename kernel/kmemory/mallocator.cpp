//
// Created by sigsegv on 23.04.2021.
//

#include <new>
#include <pagealloc.h>
#include <concurrency/critical_section.h>
#include <mutex>
#include "mallocator.h"

void *BasicMemoryAllocatorImpl::sm_allocate(uint32_t size) {
    void *ptr = memoryArea.alloctable.sm_allocate(size);
    if (ptr != nullptr) {
        uint64_t vmem = (uint64_t) ptr;
        uint64_t vmem_end = vmem + size;
        vmem = vmem & 0xFFFFFFFFFFFFF000;
        for (; vmem < vmem_end; vmem += 0x1000) {
            std::optional<pageentr> pe = get_pageentr(vmem);
            if (!pe->present) {
                pe->page_ppn = ppagealloc(4096) >> 12;
                pe->present = 1;
                pe->writeable = 1;
                pe->user_access = 0;
                pe->execution_disabled = 1;
                pe->write_through = 0;
                pe->cache_disabled = 0;
                pe->accessed = 0;
                update_pageentr(vmem, *pe);
            }
        }
        reload_pagetables();
    }
    return ptr;
}

void BasicMemoryAllocatorImpl::sm_free(void *ptr) {
    memoryArea.alloctable.sm_free(ptr);
}
uint32_t BasicMemoryAllocatorImpl::sm_sizeof(void *ptr) {
    return memoryArea.alloctable.sm_sizeof(ptr);
}


BasicMemoryAllocatorImpl::~BasicMemoryAllocatorImpl() {
    uint64_t pageaddr_end = ((uint64_t) this) + sizeof(*this);
    for (uint64_t pageaddr = (uint64_t) this; pageaddr < pageaddr_end; pageaddr += 0x1000) {
        std::optional<pageentr> pe = get_pageentr(pageaddr);
        if (pe->present) {
            uint64_t paddr = pe->page_ppn;
            paddr = paddr << 12;
            pe->present = 0;
            pe->page_ppn = 0;
            ppagefree(paddr, 4096);
        }
    }
    reload_pagetables();
}

bool BasicMemoryAllocatorImpl::sm_owned(void *ptr) {
    return memoryArea.alloctable.sm_owned(ptr);
}

void *BasicMemoryAllocator::sm_allocate(uint32_t size) {
    critical_section cli{};
    std::lock_guard lock(_lock);
    return impl->sm_allocate(size);
}

void BasicMemoryAllocator::sm_free(void *ptr) {
    critical_section cli{};
    std::lock_guard lock(_lock);
    return impl->sm_free(ptr);
}

BasicMemoryAllocator::BasicMemoryAllocator(BasicMemoryAllocatorImpl *impl) : impl(impl), _lock() {
}

uint32_t BasicMemoryAllocator::sm_sizeof(void *ptr) {
    critical_section cli{};
    std::lock_guard lock(_lock);
    return impl->sm_sizeof(ptr);
}

bool BasicMemoryAllocator::sm_owned(void *ptr) {
    critical_section cli{};
    std::lock_guard lock(_lock);
    return impl->sm_owned(ptr);
}

BasicMemoryAllocator::~BasicMemoryAllocator() {
    impl->~BasicMemoryAllocatorImpl();
}

BasicMemoryAllocator *CreateBasicMemoryAllocator() {
    void *memoryAllocatorP = (BasicMemoryAllocatorImpl *) vpagealloc(0x41000);
    {
        {
            std::optional<pageentr> pe = get_pageentr((uint64_t) memoryAllocatorP);
            uint64_t ppage = ppagealloc(4096);
            pe->page_ppn = ppage >> 12;
            pe->writeable = 1;
            pe->execution_disabled = 1;
            pe->write_through = 0;
            pe->cache_disabled = 0;
            pe->accessed = 0;
            pe->present = 1;
            update_pageentr((uint64_t) memoryAllocatorP, *pe);
        }
        {
            std::optional<pageentr> pe = get_pageentr(((uint64_t) memoryAllocatorP) + 4096);
            uint64_t ppage = ppagealloc(4096);
            pe->page_ppn = ppage >> 12;
            pe->writeable = 1;
            pe->execution_disabled = 1;
            pe->write_through = 0;
            pe->cache_disabled = 0;
            pe->accessed = 0;
            pe->present = 1;
            update_pageentr(((uint64_t) memoryAllocatorP) + 4096, *pe);
        }
    }
    reload_pagetables();
    BasicMemoryAllocatorImpl *impl = new (memoryAllocatorP) BasicMemoryAllocatorImpl();
    BasicMemoryAllocator *allocator = (BasicMemoryAllocator *) impl->sm_allocate(sizeof(*allocator));
    return new ((void *) allocator) BasicMemoryAllocator(impl);
}