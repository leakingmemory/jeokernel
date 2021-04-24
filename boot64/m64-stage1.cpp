//
// Created by sigsegv on 17.04.2021.
//

#include <stdint.h>
#include "textconsole/b8000logger.h"
#include <pagealloc.h>
#include <multiboot.h>
#include <core/malloc.h>

static const MultibootInfoHeader *multiboot_info = nullptr;

const MultibootInfoHeader &get_multiboot2() {
    return *multiboot_info;
}

extern "C" {

    void start_m64(const MultibootInfoHeader *multibootInfoHeaderPtr) {
        multiboot_info = multibootInfoHeaderPtr;
        /*
         * Let's try to alloc a stack
         */
        uint64_t stack = (uint64_t) pagealloc(16384);
        if (stack != 0) {
            stack += 16384 - 16;
            asm("mov %0, %%rax; mov %%rax,%%rsp; jmp secondboot; hlt;" :: "r"(stack) : "%rax");
        }
        b8000 b8000Logger{};
        b8000Logger << "Error: Unable to install new stack!\n";
        asm("hlt");
    }

    void init_m64() {
        setup_simplest_malloc_impl();

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
                        for (int i = 0; i < num; i++) {
                            const auto &entr = memoryMap.get_entry(i);
                            get_klogger() << " - Region " << entr.base_addr << " (" << entr.length << ") type "
                                          << entr.type << "\n";
                            if (entr.type == 1) {
                                uint64_t region_end = entr.base_addr + entr.length;
                                if (region_end > end_phys_addr) {
                                    end_phys_addr = region_end;
                                }
                                if (region_end > phys_mem_watermark) {
                                    uint64_t start = entr.base_addr >= phys_mem_watermark ? entr.base_addr : phys_mem_watermark;
                                    uint64_t last = start + 1;
                                    get_klogger() << "   - Added " << start << " - ";
                                    for (uint64_t addr = start; addr < (entr.base_addr + entr.length); addr += 0x1000) {
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
                        phys_mem_watermark = phys_mem_watermark_next;
                        get_klogger() << "Added " << phys_mem_added << " bytes, watermark " << phys_mem_watermark << ", end " << end_phys_addr << "\n";
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