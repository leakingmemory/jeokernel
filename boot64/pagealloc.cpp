//
// Created by sigsegv on 20.04.2021.
//

#include <pagealloc.h>
#include <klogger.h>
#include <concurrency/hw_spinlock.h>
#include <concurrency/critical_section.h>
#include <mutex>
#include <sstream>

#define _get_pml4t()  (*((pagetable *) 0x1000))

uint64_t vpagealloc(uint64_t size) {
    critical_section cli{};
    std::lock_guard lock{get_pagetables_lock()};

    if ((size & 4095) != 0) {
        size += 4096;
    }
    size /= 4096;
    pagetable &pml4t = _get_pml4t();
    uint64_t starting_addr = 0;
    uint64_t count = 0;
    int i = 512;
    while (i > 0) {
        --i;
        if (pml4t[i].present) {
            auto &pdpt = pml4t[i].get_subtable();
            int j = 512;
            while (j > 0) {
                --j;
                if (pdpt[j].present) {
                    auto &pdt = pdpt[j].get_subtable();
                    int k = 512;
                    while (k > 0) {
                        --k;
                        if (pdt[k].present) {
                            auto &pt = pdt[k].get_subtable();
                            int l = 512;
                            while (l > 0) {
                                --l;
                                /* the page acquire check */
                                if (pt[l].os_virt_avail) {
                                    count++;
                                    if (count == size) {
                                        starting_addr = i << 9;
                                        starting_addr |= j;
                                        starting_addr = starting_addr << 9;
                                        starting_addr |= k;
                                        starting_addr = starting_addr << 9;
                                        starting_addr |= l;
                                        starting_addr = starting_addr << 12;
                                        uint64_t ending_addr = starting_addr + (count * 4096);
                                        for (uint64_t addr = starting_addr; addr < ending_addr; addr += 4096) {
                                            pageentr *pe = get_pageentr64(pml4t, addr);
                                            pe->os_virt_avail = 0; // GRAB
                                            if (addr == starting_addr) {
                                                pe->os_virt_start = 1; // GRAB
                                            }
                                            pe->present = 0;
                                        }
                                        return starting_addr;
                                    }
                                } else {
                                    count = 0;
                                }
                            }
                        } else {
                            count = 0;
                        }
                    }
                } else {
                    count = 0;
                }
            }
        } else {
            count = 0;
        }
    }
    return 0;
}

void pmemcounts() {
    uint64_t scan = 0;
    uint64_t free_pages = 0;
    {
        critical_section cli{};
        std::lock_guard lock{get_pagetables_lock()};

        pagetable &pml4t = _get_pml4t();
        uint64_t count = 0;
        for (int i = 0; i < 512; i++) {
            if (pml4t[i].present) {
                auto &pdpt = pml4t[i].get_subtable();
                for (int j = 0; j < 512; j++) {
                    if (pdpt[j].present) {
                        auto &pdt = pdpt[j].get_subtable();
                        for (int k = 0; k < 512; k++) {
                            if (pdt[k].present) {
                                auto &pt = pdt[k].get_subtable();
                                for (int l = 0; l < 512; l++) {
                                    ++scan;
                                    /* the page acquire check */
                                    if (pt[l].os_phys_avail) {
                                        ++free_pages;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    get_klogger() << "Ppages " << scan << " pages scanned, " << free_pages << " free encountered\n";
}

//#define DEBUG_PALLOC_FAILURE
uint64_t ppagealloc(uint64_t size) {
#ifdef DEBUG_PALLOC_FAILURE
    uint64_t scan = 0;
    uint64_t free_pages = 0;
    {
#endif
    critical_section cli{};
    std::lock_guard lock{get_pagetables_lock()};

    if ((size & 4095) != 0) {
        size += 4096;
    }
    size /= 4096;
    pagetable &pml4t = _get_pml4t();
    uint64_t starting_addr = 0;
    uint64_t count = 0;
    for (int i = 0; i < 512; i++) {
        if (pml4t[i].present) {
            auto &pdpt = pml4t[i].get_subtable();
            for (int j = 0; j < 512; j++) {
                if (pdpt[j].present) {
                    auto &pdt = pdpt[j].get_subtable();
                    for (int k = 0; k < 512; k++) {
                        if (pdt[k].present) {
                            auto &pt = pdt[k].get_subtable();
                            for (int l = 0; l < 512; l++) {
#ifdef DEBUG_PALLOC_FAILURE
                                ++scan;
#endif
                                /* the page acquire check */
                                if (pt[l].os_phys_avail) {
#ifdef DEBUG_PALLOC_FAILURE
                                    ++free_pages;
#endif
                                    if (starting_addr != 0) {
                                        count++;
                                        if (count == size) {
                                            uint64_t ending_addr = starting_addr + (count * 4096);
                                            for (uint64_t addr = starting_addr; addr < ending_addr; addr += 4096) {
                                                pageentr *pe = get_pageentr64(pml4t, addr);
                                                pe->os_phys_avail = 0; // GRAB
                                            }
                                            return starting_addr;
                                        }
                                    } else {
                                        starting_addr = i << 9;
                                        starting_addr |= j;
                                        starting_addr = starting_addr << 9;
                                        starting_addr |= k;
                                        starting_addr = starting_addr << 9;
                                        starting_addr |= l;
                                        starting_addr = starting_addr << 12;
                                        if (size == 1) {
                                            pt[l].os_phys_avail = 0; // GRAB
                                            return starting_addr;
                                        } else {
                                            count = 1;
                                        }
                                    }
                                } else {
                                    starting_addr = 0;
                                }
                            }
                        } else {
                            starting_addr = 0;
                        }
                    }
                } else {
                    starting_addr = 0;
                }
            }
        } else {
            starting_addr = 0;
        }
    }
#ifdef DEBUG_PALLOC_FAILURE
    }
    get_klogger() << "Ppage allocation failure " << scan << " pages scanned, " << free_pages << " free encountered\n";
#endif
    return 0;
}
uint32_t ppagealloc32(uint32_t size) {
    critical_section cli{};
    std::lock_guard lock{get_pagetables_lock()};

    if ((size & 4095) != 0) {
        size += 4096;
    }
    size /= 4096;
    pagetable &pml4t = _get_pml4t();
    uint32_t starting_addr = 0;
    uint32_t count = 0;
    if (pml4t[0].present) {
        auto &pdpt = pml4t[0].get_subtable();
        for (int j = 0; j < 4; j++) {
            if (pdpt[j].present) {
                auto &pdt = pdpt[j].get_subtable();
                for (int k = 0; k < 512; k++) {
                    if (pdt[k].present) {
                        auto &pt = pdt[k].get_subtable();
                        for (int l = 0; l < 512; l++) {
                            /* the page acquire check */
                            if (pt[l].os_phys_avail) {
                                if (starting_addr != 0) {
                                    count++;
                                    if (count == size) {
                                        uint32_t ending_addr = starting_addr + (count * 4096);
                                        for (uint32_t addr = starting_addr; addr < ending_addr; addr += 4096) {
                                            pageentr *pe = get_pageentr64(pml4t, addr);
                                            pe->os_phys_avail = 0; // GRAB
                                        }
                                        return starting_addr;
                                    }
                                } else {
                                    starting_addr = j;
                                    starting_addr = starting_addr << 9;
                                    starting_addr |= k;
                                    starting_addr = starting_addr << 9;
                                    starting_addr |= l;
                                    starting_addr = starting_addr << 12;
                                    if (size == 1) {
                                        pt[l].os_phys_avail = 0; // GRAB
                                        return starting_addr;
                                    } else {
                                        count = 1;
                                    }
                                }
                            } else {
                                starting_addr = 0;
                            }
                        }
                    } else {
                        starting_addr = 0;
                    }
                }
            } else {
                starting_addr = 0;
            }
        }
    } else {
        starting_addr = 0;
    }
    return 0;
}
uint64_t vpagefree(uint64_t addr) {
    critical_section cli{};
    std::lock_guard lock{get_pagetables_lock()};

    pagetable &pml4t = _get_pml4t();
    addr = addr >> 12;
    uint64_t first = addr;
    uint64_t size;
    while (true) {
        uint64_t paddr = addr;
        int l = paddr & 511;
        paddr = paddr >> 9;
        int k = paddr & 511;
        paddr = paddr >> 9;
        int j = paddr & 511;
        paddr = paddr >> 9;
        int i = paddr & 511;
        if (pml4t[i].present == 0) {
            break;
        }
        auto &pdpt = pml4t[i].get_subtable();
        if (pdpt[j].present == 0) {
            break;
        }
        auto &pdt = pdpt[j].get_subtable();
        if (pdt[k].present == 0) {
            break;
        }
        pageentr &pe = pdt[k].get_subtable()[l];
        if (pe.os_virt_avail || (first != addr && pe.os_virt_start)) {
            break;
        }
        pe.os_virt_avail = 1;
        pe.os_virt_start = 0;
        pe.present = 0;
        ++addr;
        ++size;
    }
    return size << 12;
}

uint64_t pv_fix_pagealloc(uint64_t size) {
    critical_section cli{};
    std::lock_guard lock{get_pagetables_lock()};

    if ((size & 4095) != 0) {
        size += 4096;
    }
    size /= 4096;
    pagetable &pml4t = _get_pml4t();
    uint64_t starting_addr = 0;
    uint64_t count = 0;
    for (int i = 0; i < 512; i++) {
        if (pml4t[i].present) {
            auto &pdpt = pml4t[i].get_subtable();
            for (int j = 0; j < 512; j++) {
                if (pdpt[j].present) {
                    auto &pdt = pdpt[j].get_subtable();
                    for (int k = 0; k < 512; k++) {
                        if (pdt[k].present) {
                            auto &pt = pdt[k].get_subtable();
                            for (int l = 0; l < 512; l++) {
                                /* the page acquire check */
                                if (pt[l].os_phys_avail == 1 && pt[l].os_virt_avail == 1) {
                                    if (starting_addr != 0) {
                                        count++;
                                        if (count == size) {
                                            uint64_t ending_addr = starting_addr + (count * 4096);
                                            for (uint64_t addr = starting_addr; addr < ending_addr; addr += 4096) {
                                                pageentr *pe = get_pageentr64(pml4t, addr);
                                                pe->os_virt_avail = 0; // GRAB
                                                pe->os_phys_avail = 0; // GRAB
                                                if (addr == starting_addr) {
                                                    pe->os_virt_start = 1;
                                                }
                                                pe->page_ppn = addr >> 12;
                                                pe->present = 1;
                                                pe->writeable = 1;
                                                pe->execution_disabled = 1;
                                                pe->accessed = 0;
                                                pe->user_access = 0;
                                            }
                                            reload_pagetables();
                                            return starting_addr;
                                        }
                                    } else {
                                        starting_addr = i << 9;
                                        starting_addr |= j;
                                        starting_addr = starting_addr << 9;
                                        starting_addr |= k;
                                        starting_addr = starting_addr << 9;
                                        starting_addr |= l;
                                        starting_addr = starting_addr << 12;
                                        if (size == 1) {
                                            pageentr &pe = pt[l];
                                            pe.os_virt_avail = 0; // GRAB
                                            pe.os_phys_avail = 0; // GRAB
                                            pe.os_virt_start = 1; // starting point
                                            pe.page_ppn = starting_addr >> 12;
                                            pe.present = 1;
                                            pe.writeable = 1;
                                            pe.execution_disabled = 1;
                                            pe.accessed = 0;
                                            pe.user_access = 0;
                                            return starting_addr;
                                        } else {
                                            count = 1;
                                        }
                                    }
                                } else {
                                    starting_addr = 0;
                                }
                            }
                        } else {
                            starting_addr = 0;
                        }
                    }
                } else {
                    starting_addr = 0;
                }
            }
        } else {
            starting_addr = 0;
        }
    }
    return 0;
}

uint64_t pv_fix_pagefree(uint64_t vaddr) {
    uint64_t vai = (uint64_t) vaddr;
    uint64_t phys = get_phys_from_virt(vai);
    uint64_t size = vpagefree(vai);
    ppagefree(phys, size);
    reload_pagetables();
    return size;
}

uint64_t alloc_stack(uint64_t size) {
    uint64_t vaddr_top = 0;
    uint64_t vaddr_bottom = vpagealloc(size + 4096);
    if (vaddr_bottom != 0) {
        uint64_t vaddr_start = vaddr_bottom + 4096;
        uint64_t paddr_start = ppagealloc(size);
        if (paddr_start != 0) {
            for (uint64_t offset = 0; offset < size; offset += 4096) {
                std::optional<pageentr> pe = get_pageentr(vaddr_start + offset);
                uint64_t page_ppn = paddr_start + offset;
                page_ppn = page_ppn >> 12;
                pe->page_ppn = page_ppn;
                pe->present = 1;
                pe->writeable = 1;
                pe->write_through = 0;
                pe->cache_disabled = 0;
                pe->execution_disabled = 1;
                update_pageentr(vaddr_start + offset, *pe);
            }
            vaddr_top = vaddr_start + size;

            reload_pagetables();
        } else {
            vpagefree(vaddr_bottom);
            vaddr_top = 0;
        }
    }
    return vaddr_top;
}

void free_stack(uint64_t vaddr) {
    uint64_t size = 0;
    uint64_t phys_addr = 0;
    vaddr = vaddr >> 12;
    {
        critical_section cli{};
        std::lock_guard lock{get_pagetables_lock()};

        while (true) {
            --vaddr;
            uint64_t paddr = vaddr;
            int l = paddr & 511;
            paddr = paddr >> 9;
            int k = paddr & 511;
            paddr = paddr >> 9;
            int j = paddr & 511;
            paddr = paddr >> 9;
            int i = paddr & 511;
            if (_get_pml4t()[i].present == 0) {
                wild_panic("Stack is outside allocatable");
            }
            auto &pdpt = _get_pml4t()[i].get_subtable();
            if (pdpt[j].present == 0) {
                wild_panic("Stack is outside allocatable");
            }
            auto &pdt = pdpt[j].get_subtable();
            if (pdt[k].present == 0) {
                wild_panic("Stack is outside allocatable");
            }
            pageentr &pe = pdt[k].get_subtable()[l];
            if (pe.os_virt_avail == 1) {
                get_klogger() << "\nPage " << vaddr << " not allocated\n";
                wild_panic("Stack address given runs into unallocated vpage");
            }
            bool first_page = pe.os_virt_start == 1;
            if (pe.present) {
                if (first_page) {
                    wild_panic("This was not a stack allocation");
                }
                phys_addr = pe.page_ppn;
                ++size;
            } else {
                if (!first_page) {
                    wild_panic("Part of stack is not present");
                }
            }

            pe.os_virt_avail = 1;
            pe.os_virt_start = 0;

            if (first_page) {
                break;
            }

            pe.present = 0;
        }
    }
    phys_addr = phys_addr << 12;
    size = size << 12;

    ppagefree(phys_addr, size);
    reload_pagetables();
}

void reload_pagetables() {
    uint64_t cr3 = 0x1000;
    asm("mov %0,%%cr3; " :: "r"(cr3));
}

void ppagefree(uint64_t addr, uint64_t size) {
    critical_section cli{};
    std::lock_guard lock{get_pagetables_lock()};

    pagetable &pml4t = _get_pml4t();
    addr = addr >> 12;
    if ((size & 4095) != 0) {
        size += 4096;
    }
    size = size >> 12;
    uint64_t first = addr;
    while (size > 0) {
        uint64_t paddr = addr;
        int l = paddr & 511;
        paddr = paddr >> 9;
        int k = paddr & 511;
        paddr = paddr >> 9;
        int j = paddr & 511;
        paddr = paddr >> 9;
        int i = paddr & 511;
        pageentr &pe = pml4t[i].get_subtable()[j].get_subtable()[k].get_subtable()[l];
        pe.os_phys_avail = 1;
        pe.os_zero = 1;
        ++addr;
        --size;
    }
}

void *pagealloc(uint64_t size) {
    uint64_t vpages = vpagealloc(size);
    if (vpages != 0) {
        uint64_t ppages = ppagealloc(size);
        if (ppages != 0) {
            for (uint64_t offset = 0; offset < size; offset += 4096) {
                std::optional<pageentr> pe = get_pageentr(vpages + offset);
                uint64_t page_ppn = ppages + offset;
                page_ppn = page_ppn >> 12;
                pe->page_ppn = page_ppn;
                pe->present = 1;
                pe->writeable = 1;
                pe->execution_disabled = 1;
                pe->write_through = 0;
                pe->cache_disabled = 0;
                update_pageentr(vpages + offset, *pe);
            }

            reload_pagetables();
        } else {
            vpagefree(vpages);
            vpages = 0;

            reload_pagetables();
        }
    }
    return (void *) vpages;
}
void pagefree(void *vaddr) {
    uint64_t vai = (uint64_t) vaddr;
    uint64_t phys = get_phys_from_virt(vai);
    uint64_t size = vpagefree(vai);
    ppagefree(phys, size);
    reload_pagetables();
}
