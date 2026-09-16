//
// Created by sigsegv on 18.04.2021.
//

#ifndef JEOKERNEL_PAGETABLE_IMPL_H
#define JEOKERNEL_PAGETABLE_IMPL_H

#include <pagetable.h>

#include "pagealloc.h"

#if !defined(__aarch64__)
inline pageentr &get_pml4t_pageentr64(pagetable &pml4t, uint64_t addr) {
    uint16_t vector{0};
    {
        uint64_t vector64 = addr >> 39;
        vector64 = vector64 & 511;
        vector = vector64;
    }
    return pml4t[vector];
}

inline pageentr &get_pdpt_pageentr64(pagetable &pdpt_ref, uint64_t addr) {
    uint16_t vector{0};
    {
        uint64_t vector64 = addr >> 30;
        vector64 = vector64 & 511;
        vector = vector64;
    }
    return pdpt_ref[vector];
}

inline pageentr &get_pdt_pageentr64(pagetable &pdt_ref, uint64_t addr) {
    uint16_t vector{0};
    {
        uint64_t vector64 = addr >> 21;
        vector64 = vector64 & 511;
        vector = vector64;
    }
    return pdt_ref[vector];
}

inline pageentr &get_pt_pageentr64(pagetable &pt_ref, uint64_t addr) {
    uint16_t vector{0};
    {
        uint64_t vector64 = addr >> 12;
        vector64 = vector64 & 511;
        vector = vector64;
    }
    return pt_ref[vector];
}
#endif

#if defined(__x86_64__) || defined(__i386__)
inline pageentr *get_pageentr64(pagetable &pml4t, uint64_t addr) {
    pageentr &pml4t_pe = get_pml4t_pageentr64(pml4t, addr);
    if (!pml4t_pe.present()) {
        return nullptr;
    }
    pageentr &pdpt = get_pdpt_pageentr64(pml4t_pe.get_subtable(), addr);
    if (!pdpt.present()) {
        return nullptr;
    }
    pageentr &pdt = get_pdt_pageentr64(pdpt.get_subtable(), addr);
    if (!pdt.present()) {
        return nullptr;
    }
    pageentr &pt = get_pt_pageentr64(pdt.get_subtable(), addr);
    return &pt;
}
#elif defined(__aarch64__)

struct PageentrAvailablePages {
    constexpr static const uint64_t max = 8;
    std::optional<uint64_t> p[max] = {{}, {}, {}, {}, {}, {}, {}, {}};

    constexpr uint64_t NumNeeded() {
        uint64_t c = 0;
        for (uint64_t i = 0; i < max; i++) {
            if (!p[i]) {
                c++;
                for (uint64_t j = i + 1; j < max; j++) {
                    if (p[j]) {
                        auto value = *(p[j]);
                        p[j].reset();
                        p[i] = value;
                        i++;
                    } else {
                        c++;
                    }
                }
                break;
            }
        }
        return c;
    }
    constexpr bool Fill(uint64_t phys) {
        for (uint64_t i = 0; i < max; i++) {
            if (!p[i]) {
                p[i] = phys;
                return true;
            }
        }
        return false;
    }
    constexpr std::optional<uint64_t> Use() {
        for (uint64_t i = 0; i < max; i++) {
            if (p[max - i - 1]) {
                uint64_t phys = *(p[max - i - 1]);
                p[max - i - 1].reset();
                return phys;
            }
        }
        return {};
    }
};

static_assert(PageentrAvailablePages().NumNeeded() == PageentrAvailablePages::max);
static_assert(PageentrAvailablePages().Fill(1234));
static_assert(!PageentrAvailablePages().Use());

constexpr PageentrAvailablePages TestInstanceIfPageentrAvailablePages() {
    PageentrAvailablePages ap{};
    ap.Fill(1234);
    return ap;
}
static_assert(TestInstanceIfPageentrAvailablePages().NumNeeded() == (PageentrAvailablePages::max - 1));
static_assert(*(TestInstanceIfPageentrAvailablePages().Use()) == 1234);

inline pageentr *get_pageentr64(pagetable &root, uint64_t vaddr, PageentrAvailablePages &available_pages) {
    uint64_t indices[4];
    indices[0] = (vaddr >> 39) & 0x1FF;
    indices[1] = (vaddr >> 30) & 0x1FF;
    indices[2] = (vaddr >> 21) & 0x1FF;
    indices[3] = (vaddr >> 12) & 0x1FF;

    pageentr *current_table = &root[0];

    for (int level = 0; level < 3; ++level) {
        pageentr &entry = current_table[indices[level]];
        if (!entry.valid()) {
            uint64_t paddr;
            {
                std::optional<uint64_t> paddr_o = available_pages.Use();
                if (!paddr_o) {
                    return nullptr;
                }
                paddr = *paddr_o;
            }
            entry.ppn() = (paddr >> 12);
            entry.table() = 1;
            entry.valid() = 1;
        }
        current_table = &entry.get_subtable()[0];
    }

    pageentr &leaf = current_table[indices[3]];
    return &leaf;
}

#endif

#endif //JEOKERNEL_PAGETABLE_IMPL_H
