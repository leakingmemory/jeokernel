//
// Created by sigsegv on 18.04.2021.
//

#ifndef JEOKERNEL_PAGETABLE_IMPL_H
#define JEOKERNEL_PAGETABLE_IMPL_H

#include <pagetable.h>

pageentr &get_pml4t_pageentr64(pageentr (&pml4t)[512], uint64_t addr) {
    uint16_t vector{0};
    {
        uint64_t vector64 = addr >> 39;
        vector64 = vector64 & 511;
        vector = vector64;
    }
    return pml4t[vector];
}

pageentr &get_pdpt_pageentr64(pageentr (&pdpt_ref)[512], uint64_t addr) {
    uint16_t vector{0};
    {
        uint64_t vector64 = addr >> 30;
        vector64 = vector64 & 511;
        vector = vector64;
    }
    return pdpt_ref[vector];
}

pageentr &get_pdt_pageentr64(pageentr (&pdt_ref)[512], uint64_t addr) {
    uint16_t vector{0};
    {
        uint64_t vector64 = addr >> 21;
        vector64 = vector64 & 511;
        vector = vector64;
    }
    return pdt_ref[vector];
}

pageentr &get_pt_pageentr64(pageentr (&pt_ref)[512], uint64_t addr) {
    uint16_t vector{0};
    {
        uint64_t vector64 = addr >> 12;
        vector64 = vector64 & 511;
        vector = vector64;
    }
    return pt_ref[vector];
}

pageentr &get_pageentr64(pageentr (&pml4t)[512], uint64_t addr) {
    pageentr &pml4t_pe = get_pml4t_pageentr64(pml4t, addr);
    pageentr &pdpt = get_pdpt_pageentr64(pml4t_pe.get_subtable(), addr);
    pageentr &pdt = get_pdt_pageentr64(pdpt.get_subtable(), addr);
    pageentr &pt = get_pt_pageentr64(pdt.get_subtable(), addr);
    return pt;
}

#endif //JEOKERNEL_PAGETABLE_IMPL_H
