//
// Created by sigsegv on 17.04.2021.
//

#include <stdint.h>
#include "textconsole/b8000logger.h"
#include <pagealloc.h>
#include <multiboot.h>
#include <core/malloc.h>
#include <string.h>
#include <stack.h>

static const MultibootInfoHeader *multiboot_info = nullptr;
static normal_stack *stage1_stack = nullptr;

const MultibootInfoHeader &get_multiboot2() {
    return *multiboot_info;
}

void vmem_graph() {
    auto &pt = get_pml4t()[0].get_subtable()[0].get_subtable()[0].get_subtable();
    for (int i = 0; i < 512; i++) {
        char *addr = (char *) ((i << 1) + 0xb8000);
        if (pt[i].os_virt_avail) {
            if (pt[i].os_phys_avail) {
                *addr = '+';
            } else {
                *addr = 'V';
            }
        } else if (pt[i].os_phys_avail) {
            *addr = 'P';
        } else {
            *addr = '-';
        }
    }
}

pagetable *allocate_pageentr() {
    static_assert(sizeof(pagetable) == 4096);
    uint64_t addr = pv_fix_pagealloc(4096);
    if (addr != 0) {
        return (pagetable *) memset((void *) addr, 0, 4096);
    } else {
        return nullptr;
    }
}

extern "C" {

    void start_m64(const MultibootInfoHeader *multibootInfoHeaderPtr) {
        multiboot_info = multibootInfoHeaderPtr;
        /*
         * Let's try to alloc a stack
         */
        setup_simplest_malloc_impl();

        stage1_stack = new normal_stack;
        uint64_t stack = stage1_stack->get_addr();
        asm("mov %0, %%rax; mov %%rax,%%rsp; jmp secondboot; hlt;" :: "r"(stack) : "%rax");
    }

    void init_m64() {
        b8000logger *b8000Logger = new b8000logger();

        set_klogger(b8000Logger);

        {
            const auto &multiboot2 = get_multiboot2();
            get_klogger() << "Multiboot2 info at " << ((uint64_t) &multiboot2) << "\n";
            if (!multiboot2.has_parts()) {
                get_klogger() << "Error: Multiboot2 structure has no information\n";
                while (1) {
                    asm("hlt");
                }
            }
            uint64_t phys_mem_watermark = 0x200000;
            uint64_t end_phys_addr = 0x200000;
            {
                uint64_t phys_mem_added = 0;
                const auto *part = multiboot2.first_part();
                do {
                    if (part->type == 6) {
                        get_klogger() << "Memory map:\n";
                        auto &pml4t = get_pml4t();
                        const auto &memoryMap = part->get_type6();
                        int num = memoryMap.get_num_entries();
                        uint64_t phys_mem_watermark_next = phys_mem_watermark;
                        bool first_pass = true;
                        do {
                            if (!first_pass) {
                                get_klogger() << "Adding more physical memory:\n";
                            }
                            phys_mem_added = 0;
                            for (int i = 0; i < num; i++) {
                                const auto &entr = memoryMap.get_entry(i);
                                if (first_pass) {
                                    get_klogger() << " - Region " << entr.base_addr << " (" << entr.length << ") type "
                                                  << entr.type << "\n";
                                }
                                if (entr.type == 1) {
                                    uint64_t region_end = entr.base_addr + entr.length;
                                    if (region_end > end_phys_addr) {
                                        end_phys_addr = region_end;
                                    }
                                    if (region_end > phys_mem_watermark) {
                                        uint64_t start = entr.base_addr >= phys_mem_watermark ? entr.base_addr
                                                                                              : phys_mem_watermark;
                                        uint64_t last = start + 1;
                                        get_klogger() << "   - Added " << start << " - ";
                                        for (uint64_t addr = start;
                                             addr < (entr.base_addr + entr.length); addr += 0x1000) {
                                            pageentr *pe = get_pageentr64(pml4t, addr);
                                            if (pe == nullptr) {
                                                break;
                                            }
                                            pe->os_phys_avail = 1;
                                            last = addr + 0x1000;
                                            if (last > phys_mem_watermark_next) {
                                                phys_mem_watermark_next = last;
                                            }
                                            phys_mem_added += 0x1000;
                                        }
                                        --last;
                                        get_klogger() << last << "\n";
                                    }
                                }
                            }
                            if (phys_mem_watermark != phys_mem_watermark_next || phys_mem_added != 0) {
                                phys_mem_watermark = phys_mem_watermark_next;
                                get_klogger() << "Added " << phys_mem_added << " bytes, watermark "
                                              << phys_mem_watermark
                                              << ", end " << end_phys_addr << "\n";
                            }
                            uint32_t mem_ext_consumed = 0;
                            uint64_t memory_extended = 0;
                            for (int i = 0; i < 512; i++) {
                                if (pml4t[i].present == 0) {
                                    pagetable *pt = allocate_pageentr();
                                    if (pt == nullptr) {
                                        break;
                                    }
                                    pml4t[i].page_ppn = (((uint64_t) pt) >> 12);
                                    pml4t[i].writeable = 1;
                                    pml4t[i].present = 1;
                                    mem_ext_consumed += 4096;
                                }
                                auto &pdpt = pml4t[i].get_subtable();
                                for (int j = 0; j < 512; j++) {
                                    if (pdpt[j].present == 0) {
                                        pagetable *pt = allocate_pageentr();
                                        if (pt == nullptr) {
                                            goto done_with_mem_extension;
                                        }
                                        pdpt[j].page_ppn = (((uint64_t) pt) >> 12);
                                        pdpt[j].writeable = 1;
                                        pdpt[j].present = 1;
                                        mem_ext_consumed += 4096;
                                    }
                                    auto &pdt = pdpt[j].get_subtable();
                                    for (int k = 0; k < 512; k++) {
                                        if (pdt[k].present == 0) {
                                            pagetable *pt = allocate_pageentr();
                                            if (pt == nullptr) {
                                                goto done_with_mem_extension;
                                            }
                                            for (int l = 0; l < 512; l++) {
                                                (*pt)[l].os_virt_avail = 1;
                                            }
                                            pdt[k].page_ppn = (((uint64_t) pt) >> 12);
                                            pdt[k].writeable = 1;
                                            pdt[k].present = 1;
                                            mem_ext_consumed += 4096;
                                            memory_extended += 512 * 4096;
                                        }
                                        uint64_t addr = i;
                                        addr = (addr << 9) + j;
                                        addr = (addr << 9) + k;
                                        addr = addr << (9 + 12);
                                        if (addr > end_phys_addr) {
                                            goto done_with_mem_extension;
                                        }
                                    }
                                }
                            }
done_with_mem_extension:
                            if (memory_extended != 0 || mem_ext_consumed != 0) {
                                get_klogger() << "Virtual memory extended by " << memory_extended << ", consumed "
                                              << mem_ext_consumed << "\n";
                            }
                            first_pass = false;
                        } while (phys_mem_added > 0);
                        get_klogger() << "Done with mapping physical memory\n";
                    }
                    if (part->hasNext(multiboot2)) {
                        part = part->next();
                    } else {
                        part = nullptr;
                    }
                } while (part != nullptr);
            }
        }

        while (1) {
            asm("hlt");
        }
    }
}