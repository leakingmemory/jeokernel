//
// Created by sigsegv on 20.04.2021.
//

#include <pagealloc.h>

uint64_t vpagealloc(uint64_t size) {
    if ((size & 4095) != 0) {
        size += 4096;
    }
    size /= 4096;
    pageentr (&pml4t)[512] = *((pageentr (*)[512]) 0x1000);
    uint64_t starting_addr = 0;
    uint64_t count = 0;
    int i = 512;
    while (i > 0) {
        --i;
        if (pml4t[i].os_virt_avail) {
            auto &pdpt = pml4t[i].get_subtable();
            int j = 512;
            while (j > 0) {
                --j;
                if (pdpt[j].os_virt_avail) {
                    auto &pdt = pdpt[j].get_subtable();
                    int k = 512;
                    while (k > 0) {
                        --k;
                        if (pdt[k].os_virt_avail) {
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
                                            pageentr &pe = get_pageentr64(pml4t, addr);
                                            pe.os_virt_avail = 0; // GRAB
                                            if (addr == starting_addr) {
                                                pe.os_virt_start = 1; // GRAB
                                            }
                                        }
                                        return starting_addr;
                                    }
                                } else {
                                    count = 0;
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
uint64_t ppagealloc(uint64_t size) {
    if ((size & 4095) != 0) {
        size += 4096;
    }
    size /= 4096;
    pageentr (&pml4t)[512] = *((pageentr (*)[512]) 0x1000);
    uint64_t starting_addr = 0;
    uint64_t count = 0;
    for (int i = 0; i < 512; i++) {
        if (pml4t[i].os_virt_avail) {
            auto &pdpt = pml4t[i].get_subtable();
            for (int j = 0; j < 512; j++) {
                if (pdpt[j].os_virt_avail) {
                    auto &pdt = pdpt[j].get_subtable();
                    for (int k = 0; k < 512; k++) {
                        if (pdt[k].os_virt_avail) {
                            auto &pt = pdt[k].get_subtable();
                            for (int l = 0; l < 512; l++) {
                                /* the page acquire check */
                                if (pt[l].os_phys_avail) {
                                    if (starting_addr != 0) {
                                        count++;
                                        if (count == size) {
                                            uint64_t ending_addr = starting_addr + (count * 4096);
                                            for (uint64_t addr = starting_addr; addr < ending_addr; addr += 4096) {
                                                pageentr &pe = get_pageentr64(pml4t, addr);
                                                pe.os_phys_avail = 0; // GRAB
                                                if (addr == starting_addr) {
                                                    pe.os_phys_start = 1; // GRAB
                                                }
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
                                            pt[l].os_phys_start = 1; // GRAB
                                            pt[l].present = 0;
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
void vpagefree(uint64_t addr) {

}
void ppagefree(uint64_t addr) {

}

void *pagealloc(uint64_t size) {
    pageentr (&pml4t)[512] = *((pageentr (*)[512]) 0x1000);
    uint64_t vpages = vpagealloc(size);
    if (vpages != 0) {
        uint64_t ppages = ppagealloc(size);
        if (ppages != 0) {
            for (uint64_t offset = 0; offset < size; offset += 4096) {
                pageentr &pe = get_pageentr64(pml4t, vpages + offset);
                uint64_t page_ppn = vpages + offset;
                page_ppn = page_ppn >> 12;
                pe.page_ppn = page_ppn;
                pe.present = 1;
                pe.writeable = 1;
            }

            uint64_t cr3 = 0x1000;
            asm("mov %0,%%cr3; " :: "r"(cr3));
        } else {
            vpagefree(vpages);
            vpages = 0;

            uint64_t cr3 = 0x1000;
            asm("mov %0,%%cr3; " :: "r"(cr3));
        }
    }
    return (void *) vpages;
}
void pagefree(void *vaddr) {

}
