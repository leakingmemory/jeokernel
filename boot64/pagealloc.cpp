//
// Created by sigsegv on 20.04.2021.
//

#include <pagealloc.h>

uint64_t vpagealloc(uint64_t size) {
    if ((size & 4095) != 0) {
        size += 4096;
    }
    size /= 4096;
    pagetable &pml4t = get_pml4t();
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
uint64_t ppagealloc(uint64_t size) {
    if ((size & 4095) != 0) {
        size += 4096;
    }
    size /= 4096;
    pagetable &pml4t = get_pml4t();
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
                                if (pt[l].os_phys_avail) {
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
    return 0;
}
uint64_t vpagefree(uint64_t addr) {
    pagetable &pml4t = get_pml4t();
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

void reload_pagetables() {
    uint64_t cr3 = 0x1000;
    asm("mov %0,%%cr3; " :: "r"(cr3));
}

void ppagefree(uint64_t addr, uint64_t size) {
    pagetable &pml4t = get_pml4t();
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
        pe.present = 0;
        ++addr;
        --size;
    }
}

void *pagealloc(uint64_t size) {
    pagetable &pml4t = get_pml4t();
    uint64_t vpages = vpagealloc(size);
    if (vpages != 0) {
        uint64_t ppages = ppagealloc(size);
        if (ppages != 0) {
            for (uint64_t offset = 0; offset < size; offset += 4096) {
                pageentr *pe = get_pageentr64(pml4t, vpages + offset);
                uint64_t page_ppn = ppages + offset;
                page_ppn = page_ppn >> 12;
                pe->page_ppn = page_ppn;
                pe->present = 1;
                pe->writeable = 1;
                pe->execution_disabled = 1;
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
