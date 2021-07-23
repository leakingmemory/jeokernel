//
// Created by sigsegv on 23.04.2021.
//

#include <pagealloc.h>
#include <concurrency/critical_section.h>
#include <mutex>
#include "mallocator.h"

void *BasicMemoryAllocator::sm_allocate(uint32_t size) {
    critical_section cli{};
    std::lock_guard lock{_lock};

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

void BasicMemoryAllocator::sm_free(void *ptr) {
    critical_section cli{};
    std::lock_guard lock{_lock};

    memoryArea.alloctable.sm_free(ptr);
}
uint32_t BasicMemoryAllocator::sm_sizeof(void *ptr) {
    critical_section cli{};
    std::lock_guard lock{_lock};

    return memoryArea.alloctable.sm_sizeof(ptr);
}


BasicMemoryAllocator::~BasicMemoryAllocator() {
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

bool BasicMemoryAllocator::sm_owned(void *ptr) {
    return memoryArea.alloctable.sm_owned(ptr);
}
