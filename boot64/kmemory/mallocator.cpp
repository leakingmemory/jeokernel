//
// Created by sigsegv on 23.04.2021.
//

#include <pagealloc.h>
#include "mallocator.h"

void *MemoryAllocator::sm_allocate(uint32_t size) {
    void *ptr = memoryArea.alloctable.sm_allocate(size);
    if (ptr != nullptr) {
        uint64_t vmem = (uint64_t) ptr;
        uint64_t vmem_end = vmem + size;
        vmem = vmem & 0xFFFFFFFFFFFFF000;
        for (; vmem < vmem_end; vmem += 0x1000) {
            pageentr &pe = get_pageentr64(get_pml4t(), vmem);
            if (!pe.present) {
                pe.page_ppn = ppagealloc(4096) >> 12;
                pe.present = 1;
                pe.writeable = 1;
                pe.user_access = 0;
                pe.execution_disabled = 1;
                pe.accessed = 0;
            }
        }
        reload_pagetables();
    }
    return ptr;
}

MemoryAllocator::~MemoryAllocator() {
    uint64_t pageaddr_end = ((uint64_t) this) + sizeof(*this);
    for (uint64_t pageaddr = (uint64_t) this; pageaddr < pageaddr_end; pageaddr += 0x1000) {
        pageentr &pe = get_pageentr64(get_pml4t(), pageaddr);
        if (pe.present) {
            uint64_t paddr = pe.page_ppn;
            paddr = paddr << 12;
            pe.present = 0;
            pe.page_ppn = 0;
            ppagefree(paddr, 4096);
        }
    }
    reload_pagetables();
}
