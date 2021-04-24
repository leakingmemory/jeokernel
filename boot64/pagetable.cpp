//
// Created by sigsegv on 20.04.2021.
//

#include <stdint.h>
#include <pagetable_impl.h>

pagetable &get_pml4t() {
    return *((pagetable *) 0x1000);
}

uint64_t get_phys_from_virt(uint64_t vaddr) {
    uint64_t paddr = get_pageentr64(get_pml4t(), vaddr)->page_ppn;
    paddr = paddr << 12;
    return paddr;
}
