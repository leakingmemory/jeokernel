
#include "plainvga32.h"
#include <strs.h>
#include <pagetable_impl.h>
#include <physpagemap.h>
#include <multiboot_impl.h>
#include <elf_impl.h>
#include <string.h>
#include <loaderconfig.h>
#include <new>
#include <stage1.h>

//#define DEBUG_ELF_ATTRIBUTES
//#define DEBUG_ELF_ATTRIBUTES_BRAKE_VAL 100000000

extern "C" {

void __text_start();
void __data_start();
void __data_end();
void __end();

uint32_t rdmsr(uint32_t addr) {
    uint32_t msr;
    asm("mov %1, %%ecx; rdmsr; mov %%eax, %0; " : "=r"(msr) : "r"(addr) : "%ecx", "%eax");
    return msr;
}

void wrmsr(uint32_t addr, uint32_t msr) {
    asm("mov %0, %%ecx; mov %1, %%eax; wrmsr; " :: "r"(addr), "r"(msr) : "%eax", "%ecx");
}

void vmem_dump(PhyspageMap *map) {
    pageentr *pt = (pageentr *) 0x4000;
    for (int i = 0; i < 512; i++) {
        char *addr = (char *) ((i << 1) + 0xb8000);
        if (pt[i].os_virt_avail) {
            if (!map->claimed(i)) {
                *addr = '+';
            } else {
                *addr = 'V';
            }
        } else if (!map->claimed(i)) {
            *addr = 'P';
        } else {
            *addr = '-';
        }
    }
}

void boot_stage1(void *multiboot_header_addr) {
    {
        MultibootInfoHeader &header = *((MultibootInfoHeader *) multiboot_header_addr);
        void *new_multiboot_header_addr = (void *) 0x1c000;
        memmove(new_multiboot_header_addr, multiboot_header_addr, header.total_size);
        multiboot_header_addr = new_multiboot_header_addr;
    }
    uint32_t physpagemap_addr = 0x1b000;
    PhyspageMap *physpageMap;
    physpageMap = new ((void *) physpagemap_addr) PhyspageMap();

    MultibootInfoHeader &header = *((MultibootInfoHeader *) multiboot_header_addr);
    plainvga32 vga;
    vga.display(0, 0, "| JEO Kernel 1.0 - Stage 1 boot");
    char *hex = (char *) 0x0500;
    hexstr((char (&)[8]) *hex, (uint32_t) __text_start);
    vga.display(1, 0, hex);
    hexstr((char (&)[8]) *hex, (uint32_t) __data_start);
    vga.display(1, 9, hex);
    hexstr((char (&)[8]) *hex, (uint32_t) __data_end);
    vga.display(1, 18, hex);
    hexstr((char (&)[8]) *hex, (uint32_t) __end);
    vga.display(1, 27, hex);
    uint32_t esp;
    asm("mov %%esp,%0" : "=r"(esp));
    hexstr((char (&)[8]) *hex, (uint32_t) esp);
    vga.display(1, 36, hex);

    uint64_t (&pml4t_raw)[512] = *((uint64_t (*)[512]) 0x1000);
    pageentr (&pml4t)[512] = (pageentr (&)[512]) pml4t_raw;
    uint64_t (&pdpt_raw)[512] = *((uint64_t (*)[512]) 0x2000);
    pageentr (&pdpt)[512] = (pageentr (&)[512]) pdpt_raw;
    uint64_t (&pdt_raw)[512] = *((uint64_t (*)[512]) 0x3000);
    pageentr (&pdt)[512] = (pageentr (&)[512]) pdt_raw;
    uint64_t (&pt_raw)[512] = *((uint64_t (*)[512]) 0x4000);
    pageentr (&pt)[512] = (pageentr (&)[512]) pt_raw;
    uint64_t (&pt_raw2)[512] = *((uint64_t (*)[512]) 0x5000);
    pageentr (&pt2)[512] = (pageentr (&)[512]) pt_raw2;
    uint64_t (&pt_raw3)[512] = *((uint64_t (*)[512]) 0x6000);
    pageentr (&pt3)[512] = (pageentr (&)[512]) pt_raw3;
    uint64_t (&pt_raw4)[512] = *((uint64_t (*)[512]) 0x7000);
    pageentr (&pt4)[512] = (pageentr (&)[512]) pt_raw4;
    uint64_t (&pt_raw5)[512] = *((uint64_t (*)[512]) 0xa000);
    pageentr (&pt5)[512] = (pageentr (&)[512]) pt_raw5;
    uint64_t (&pt_raw6)[512] = *((uint64_t (*)[512]) 0xb000);
    pageentr (&pt6)[512] = (pageentr (&)[512]) pt_raw6;
    uint64_t (&pt_raw7)[512] = *((uint64_t (*)[512]) 0xc000);
    pageentr (&pt7)[512] = (pageentr (&)[512]) pt_raw7;
    uint64_t (&pt_raw8)[512] = *((uint64_t (*)[512]) 0xd000);
    pageentr (&pt8)[512] = (pageentr (&)[512]) pt_raw8;
    uint64_t (&pt_raw9)[512] = *((uint64_t (*)[512]) 0xe000);
    pageentr (&pt9)[512] = (pageentr (&)[512]) pt_raw9;
    uint64_t (&pt_raw10)[512] = *((uint64_t (*)[512]) 0xf000);
    pageentr (&pt10)[512] = (pageentr (&)[512]) pt_raw10;
    uint64_t (&pt_raw11)[512] = *((uint64_t (*)[512]) 0x10000);
    pageentr (&pt11)[512] = (pageentr (&)[512]) pt_raw11;
    uint64_t (&pt_raw12)[512] = *((uint64_t (*)[512]) 0x11000);
    pageentr (&pt12)[512] = (pageentr (&)[512]) pt_raw12;
    uint64_t (&pt_raw13)[512] = *((uint64_t (*)[512]) 0x12000);
    pageentr (&pt13)[512] = (pageentr (&)[512]) pt_raw13;
    uint64_t (&pt_raw14)[512] = *((uint64_t (*)[512]) 0x13000);
    pageentr (&pt14)[512] = (pageentr (&)[512]) pt_raw14;
    uint64_t (&pt_raw15)[512] = *((uint64_t (*)[512]) 0x14000);
    pageentr (&pt15)[512] = (pageentr (&)[512]) pt_raw15;
    uint64_t (&pt_raw16)[512] = *((uint64_t (*)[512]) 0x15000);
    pageentr (&pt16)[512] = (pageentr (&)[512]) pt_raw16;
    uint64_t (&pt_raw17)[512] = *((uint64_t (*)[512]) 0x16000);
    pageentr (&pt17)[512] = (pageentr (&)[512]) pt_raw17;
    uint64_t (&pt_raw18)[512] = *((uint64_t (*)[512]) 0x17000);
    pageentr (&pt18)[512] = (pageentr (&)[512]) pt_raw18;
    uint64_t (&pt_raw19)[512] = *((uint64_t (*)[512]) 0x18000);
    pageentr (&pt19)[512] = (pageentr (&)[512]) pt_raw19;
    uint64_t (&pt_raw20)[512] = *((uint64_t (*)[512]) 0x19000);
    pageentr (&pt20)[512] = (pageentr (&)[512]) pt_raw20;

    for (uint16_t i = 0; i < 512; i++) {
        pml4t_raw[i] = 0;
        pdpt_raw[i] = 0;
        pdt_raw[i] = 0;
        pt_raw[i] = 0;
        pt_raw2[i] = 0;
        pt_raw3[i] = 0;
        pt_raw4[i] = 0;
        pt_raw5[i] = 0;
        pt_raw6[i] = 0;
        pt_raw7[i] = 0;
        pt_raw8[i] = 0;
        pt_raw9[i] = 0;
        pt_raw10[i] = 0;
        pt_raw11[i] = 0;
        pt_raw12[i] = 0;
        pt_raw13[i] = 0;
        pt_raw14[i] = 0;
        pt_raw15[i] = 0;
        pt_raw16[i] = 0;
        pt_raw17[i] = 0;
        pt_raw18[i] = 0;
        pt_raw19[i] = 0;
        pt_raw20[i] = 0;
    }

    pml4t[0].page_ppn = 0x2000 / 4096;
    pml4t[0].writeable = 1;
    pml4t[0].present = 1;
    pml4t[0].os_virt_avail = 1;
    pdpt[0].page_ppn = 0x3000 / 4096;
    pdpt[0].writeable = 1;
    pdpt[0].present = 1;
    pdpt[0].os_virt_avail = 1;
    pdt[0].page_ppn = 0x4000 / 4096;
    pdt[0].writeable = 1;
    pdt[0].present = 1;
    pdt[0].os_virt_avail = 1;
    pdt[1].page_ppn = 0x5000 / 4096;
    pdt[1].writeable = 1;
    pdt[1].present = 1;
    pdt[1].os_virt_avail = 1;
    pdt[2].page_ppn = 0x6000 / 4096;
    pdt[2].writeable = 1;
    pdt[2].present = 1;
    pdt[2].os_virt_avail = 1;
    pdt[3].page_ppn = 0x7000 / 4096;
    pdt[3].writeable = 1;
    pdt[3].present = 1;
    pdt[3].os_virt_avail = 1;
    pdt[4].page_ppn = 0xa000 / 4096;
    pdt[4].writeable = 1;
    pdt[4].present = 1;
    pdt[4].os_virt_avail = 1;
    pdt[5].page_ppn = 0xb000 / 4096;
    pdt[5].writeable = 1;
    pdt[5].present = 1;
    pdt[5].os_virt_avail = 1;
    pdt[6].page_ppn = 0xc000 / 4096;
    pdt[6].writeable = 1;
    pdt[6].present = 1;
    pdt[6].os_virt_avail = 1;
    pdt[7].page_ppn = 0xd000 / 4096;
    pdt[7].writeable = 1;
    pdt[7].present = 1;
    pdt[7].os_virt_avail = 1;
    pdt[8].page_ppn = 0xe000 / 4096;
    pdt[8].writeable = 1;
    pdt[8].present = 1;
    pdt[8].os_virt_avail = 1;
    pdt[9].page_ppn = 0xf000 / 4096;
    pdt[9].writeable = 1;
    pdt[9].present = 1;
    pdt[9].os_virt_avail = 1;
    pdt[10].page_ppn = 0x10000 / 4096;
    pdt[10].writeable = 1;
    pdt[10].present = 1;
    pdt[10].os_virt_avail = 1;
    pdt[11].page_ppn = 0x11000 / 4096;
    pdt[11].writeable = 1;
    pdt[11].present = 1;
    pdt[11].os_virt_avail = 1;
    pdt[12].page_ppn = 0x12000 / 4096;
    pdt[12].writeable = 1;
    pdt[12].present = 1;
    pdt[12].os_virt_avail = 1;
    pdt[13].page_ppn = 0x13000 / 4096;
    pdt[13].writeable = 1;
    pdt[13].present = 1;
    pdt[13].os_virt_avail = 1;
    pdt[14].page_ppn = 0x14000 / 4096;
    pdt[14].writeable = 1;
    pdt[14].present = 1;
    pdt[14].os_virt_avail = 1;
    pdt[15].page_ppn = 0x15000 / 4096;
    pdt[15].writeable = 1;
    pdt[15].present = 1;
    pdt[15].os_virt_avail = 1;
    pdt[16].page_ppn = 0x16000 / 4096;
    pdt[16].writeable = 1;
    pdt[16].present = 1;
    pdt[16].os_virt_avail = 1;
    pdt[17].page_ppn = 0x17000 / 4096;
    pdt[17].writeable = 1;
    pdt[17].present = 1;
    pdt[17].os_virt_avail = 1;
    pdt[18].page_ppn = 0x18000 / 4096;
    pdt[18].writeable = 1;
    pdt[18].present = 1;
    pdt[18].os_virt_avail = 1;
    pdt[19].page_ppn = 0x19000 / 4096;
    pdt[19].writeable = 1;
    pdt[19].present = 1;
    pdt[19].os_virt_avail = 1;
    /*
     * Make first 2MiB adressable,writable,execable
     */
    for (uint16_t i = 0; i < 512; i++) {
        if (i > 0) {
            pt[i].page_ppn = i;
            pt[i].writeable = 1;
            pt[i].present = 1;
        }
        {
            pt2[i].page_ppn = i + 512;
            pt2[i].writeable = 1;
            pt2[i].present = 1;
        }
        pt2[i].os_virt_avail = 1;
        pt3[i].os_virt_avail = 1;
        pt4[i].os_virt_avail = 1;
        pt5[i].os_virt_avail = 1;
        pt6[i].os_virt_avail = 1;
        pt7[i].os_virt_avail = 1;
        pt8[i].os_virt_avail = 1;
        pt9[i].os_virt_avail = 1;
        pt10[i].os_virt_avail = 1;
        pt11[i].os_virt_avail = 1;
        pt12[i].os_virt_avail = 1;
        pt13[i].os_virt_avail = 1;
        pt14[i].os_virt_avail = 1;
        pt15[i].os_virt_avail = 1;
        pt16[i].os_virt_avail = 1;
        pt17[i].os_virt_avail = 1;
        pt18[i].os_virt_avail = 1;
        pt19[i].os_virt_avail = 1;
        pt20[i].os_virt_avail = 1;
    }
    /*
     * Make second half, and next five, allocateable
     */
    for (uint16_t i = 0; i < 512; i++) {
        if (i >= 256) {
            pt[i].os_virt_avail = 1;
        } else {
            physpageMap->claim(i);
        }
        pt2[i].os_virt_avail = 1;
        pt3[i].os_virt_avail = 1;
        pt4[i].os_virt_avail = 1;
        pt5[i].os_virt_avail = 1;
        pt6[i].os_virt_avail = 1;
        pt7[i].os_virt_avail = 1;
        pt8[i].os_virt_avail = 1;
        pt9[i].os_virt_avail = 1;
        pt10[i].os_virt_avail = 1;
        pt11[i].os_virt_avail = 1;
        pt12[i].os_virt_avail = 1;
        pt13[i].os_virt_avail = 1;
        pt14[i].os_virt_avail = 1;
        pt15[i].os_virt_avail = 1;
        pt16[i].os_virt_avail = 1;
        pt17[i].os_virt_avail = 1;
        pt18[i].os_virt_avail = 1;
        pt19[i].os_virt_avail = 1;
        pt20[i].os_virt_avail = 1;
    }
    vga.display(0, 0, "/");

    // Our stack is at pt6[511]
    for (uint16_t i = 502; i < 512; i++) {
        pt6[i].os_virt_avail = 0;
        pt6[i].writeable = 1;
        pt6[i].present = 1;
        pt6[i].execution_disabled = 1;
        pt6[i].page_ppn = 0xa00 + i;
        physpageMap->claim(0xa00 + i);
    }
    pt6[502].os_virt_start = 1;
    pt6[502].present = 0; // Crash barrier

    hexstr((char (&)[8]) *hex, (uint32_t) multiboot_header_addr);
    vga.display(2, 13, "multiboot=");
    vga.display(2, 23, hex);

    bool one_kernel_as_module = false;
    uint32_t kernel_elf_start;
    uint32_t kernel_elf_end;
    {
        {
            uint32_t phaddr = (uint32_t) multiboot_header_addr;
            uint32_t end = phaddr + header.total_size;
            phaddr &= ~4095;
            while (phaddr < end) {
                auto *pe = get_pageentr64(pml4t, phaddr);
                physpageMap->claim(phaddr >> 12);
                phaddr += 4096;
                vga.display(0, 0, "#");
            }
        }
        bool multiple_modules = false;
        int ln = 3;
        vga.display(0, 0, "!");
        if (header.has_parts()) {
            vga.display(0, 0, "-");
            const MultibootInfoHeaderPart *part = header.first_part();
            bool hasNext = part->hasNext(header);
            vga.display(0, 0, "\\");
            do {
                vga.display(0, 0, "|");
                hexstr((char (&)[8]) *hex, (uint32_t) part);
                vga.display(ln, 0, hex);
                hexstr((char (&)[8]) *hex, part->size);
                vga.display(ln, 9, hex);
                hexstr((char (&)[8]) *hex, part->type);
                vga.display(ln, 18, hex);
                if (part->type == 3) {
                    const MultibootModuleInfo &moduleInfo = part->get_type3();
                    hexstr((char (&)[8]) *hex, moduleInfo.mod_start);
                    vga.display(ln, 27, hex);
                    hexstr((char (&)[8]) *hex, moduleInfo.mod_end);
                    vga.display(ln, 36, hex);
                    vga.display(ln, 45, moduleInfo.get_name());
                    if (!one_kernel_as_module) {
                        one_kernel_as_module = true;
                        kernel_elf_start = moduleInfo.mod_start;
                        kernel_elf_end = moduleInfo.mod_end;
                    } else {
                        multiple_modules = true;
                    }
                }
                if (hasNext) {
                    part = part->next();
                    hasNext = part->hasNext(header);
                }
                ln++;
            } while (hasNext);
            vga.display(0, 0, "/");
        }
        vga.display(ln, 0, "End of header");
        ln++;
        if (multiple_modules) {
            vga.display(ln, 0, "error: expected kernel as module, one module");
            while (1) {
            }
        } else if (!one_kernel_as_module) {
            vga.display(ln, 0, "error: expected 64bit kernel as module");
            while (1) {
            }
        }
        while (ln >= 3) {
            vga.display(ln, 0, "                                                                                ");
            ln--;
        }
    }
    if (one_kernel_as_module) {
        vga.display(3, 0, "Loading kernel64 ");
        hexstr((char (&)[8]) *hex, kernel_elf_start);
        vga.display(3, 18, hex);
        hexstr((char (&)[8]) *hex, kernel_elf_end);
        vga.display(3, 27, hex);

        /*
         * Allocate the kernel binary data so that the next stage can read headers without
         * being worried anybody have overwritten them.
         */
        for (uint32_t addr = (uint32_t) kernel_elf_start; addr < ((uint32_t) kernel_elf_end); addr += 4096) {
            uint32_t paddr = addr & ~4095;
            physpageMap->claim(paddr >> 12);
        }

        ELF kernel{(void *) kernel_elf_start, (void *) kernel_elf_end};
        if (kernel.is_valid()) {
            const auto &elf64_header = kernel.get_elf64_header();

            vga.display(4, 0, "entry ");
            hexstr((char (&)[8]) *hex, elf64_header.e_entry);
            vga.display(4, 20, hex);
            /*
            vga.display(5, 0, "phoff ");
            hexstr((char (&)[8]) hex, elf64_header.e_phoff);
            vga.display(5, 20, hex);
            vga.display(6, 0, "shoff ");
            hexstr((char (&)[8]) hex, elf64_header.e_shoff);
            vga.display(6, 20, hex);
            vga.display(7, 0, "ehsize ");
            hexstr((char (&)[8]) hex, elf64_header.e_ehsize);
            vga.display(7, 20, hex);
            vga.display(8, 0, "phentsize ");
            hexstr((char (&)[8]) hex, elf64_header.e_phentsize);
            vga.display(8, 20, hex);
            vga.display(9, 0, "phnum ");
            hexstr((char (&)[8]) hex, elf64_header.e_phnum);
            vga.display(9, 20, hex);
            vga.display(10, 0, "shentsize ");
            hexstr((char (&)[8]) hex, elf64_header.e_shentsize);
            vga.display(10, 20, hex);
            vga.display(11, 0, "shnum ");
            hexstr((char (&)[8]) hex, elf64_header.e_shnum);
            vga.display(11, 20, hex);
            vga.display(12, 0, "shstrndx ");
            hexstr((char (&)[8]) hex, elf64_header.e_shstrndx);
            vga.display(12, 20, hex);
            */

            uint32_t phdata = kernel_elf_end;
            uint8_t ln = 5;
            for (uint16_t i = 0; i < elf64_header.e_phnum; i++) {
                const auto &ph = elf64_header.get_program_entry(i);
                vga.display(ln, 0, "ph ");
                hexstr((char (&)[8]) *hex, ph.p_offset);
                vga.display(ln, 4, hex);
                hexstr((char (&)[8]) *hex, ph.p_vaddr);
                vga.display(ln, 13, hex);
                hexstr((char (&)[8]) *hex, ph.p_filesz);
                vga.display(ln, 22, hex);
                hexstr((char (&)[8]) *hex, ph.p_memsz);
                vga.display(ln, 31, hex);
                hexstr((char (&)[8]) *hex, ph.p_flags);
                vga.display(ln, 40, hex);
                ln++;

                if (ph.p_memsz > 0) {
                    uint32_t phaddr = kernel_elf_start + ph.p_offset;
                    {
                        uint32_t overrun = phdata & 4095;
                        if (overrun != 0) {
                            phdata += 4096 - overrun;
                        }
                    }
                    uint32_t vaddr = ph.p_vaddr;
                    uint32_t vaddr_end = vaddr + ph.p_memsz;
                    if (ph.p_filesz == ph.p_memsz) {
                        if ((phaddr & 4095) != 0 || (vaddr & 4095) != 0) {
                            uint32_t alignment_error = phaddr & 4095;
                            if (alignment_error != (vaddr & 4095)) {
                                vga.display(ln, 0, "error: kernel exec not page aligned");
                                while (1) {
                                    asm("hlt;");
                                }
                            }
                            phaddr -= alignment_error;
                            vaddr -= alignment_error;
                        }
                        while (vaddr < vaddr_end) {
                            pageentr *phys_pe = get_pageentr64(pml4t, phaddr);
                            uint32_t page_ppn = phaddr / 4096;
                            physpageMap->claim(page_ppn);
                            pageentr *pe = get_pageentr64(pml4t, vaddr);

                            /*
                             * Default read only and non-exec
                             */
                            pe->writeable = 0;
                            pe->execution_disabled = 1;

                            pe->os_virt_avail = 0;
                            pe->os_virt_start = 1;
                            pe->page_ppn = page_ppn;

                            vaddr += 4096;
                            phaddr += 4096;
                        }
                    } else {
                        uint32_t cp = ph.p_filesz;
                        while (vaddr < vaddr_end) {
                            for (uint32_t phdataaddr = 0x200000; phdataaddr < 0xb00000; phdataaddr += 0x1000) {
                                pageentr *phys_pe = get_pageentr64(pml4t, phdataaddr);
                                uint32_t page_ppn = phdataaddr >> 12;
                                if (!physpageMap->claimed(page_ppn)) {
                                    physpageMap->claim(page_ppn);
                                    pageentr *pe = get_pageentr64(pml4t, vaddr);
                                    pe->os_virt_avail = 0;
                                    pe->os_virt_start = 1;

                                    /*
                                     * Default read-only and non-exec
                                     */
                                    pe->writeable = 0;
                                    pe->execution_disabled = 1;

                                    pe->page_ppn = page_ppn;

                                    {
                                        uint8_t *z = (uint8_t *) phdataaddr;
                                        int i = 0;
                                        /*
                                         * Write data from ELF file if within the file size.
                                         */
                                        for (; cp > 0 && i < 4096; i++) {
                                            z[i] = *((uint8_t *) phaddr);
                                            ++phaddr;
                                            --cp;
                                        }
                                        /*
                                         * Zero if outside file size.
                                         */
                                        for (; i < 4096; i++) {
                                            z[i] = 0;
                                        }
                                    }

                                    goto good_data_page_alloc;
                                }
                            }
                            vga.display(0, 0, "ERROR: failed to allocate data page  ");
                            while(1) {
                                asm("hlt");
                            }
good_data_page_alloc:
                            vaddr += 4096;
                        }
                    }
                }
            }

            for (uint16_t i = 0; i < elf64_header.e_shnum; i++) {
                const auto &section = elf64_header.get_section_entry(i);
                if (section.sh_addr != 0 && section.sh_size != 0) {
#ifdef DEBUG_ELF_ATTRIBUTES
                    hexstr((char (&)[8]) *hex, section.sh_addr);
                    vga.display(22, 40, "Sec ");
                    vga.display(22, 44, &(hex[0]));
                    hexstr((char (&)[8]) *hex, section.sh_size);
                    vga.display(22, 53, &(hex[0]));
                    vga.display(23, 40, "                               ");
                    for (int delay = 0; delay < DEBUG_ELF_ATTRIBUTES_BRAKE_VAL; delay++) {
                        asm("nop");
                    }
#endif
                    uint32_t vaddr = (uint32_t) section.sh_addr;
                    uint32_t end_vaddr = vaddr + ((uint32_t) section.sh_size);
                    vaddr = vaddr & 0xFFFFF000;
                    for (; vaddr < end_vaddr; vaddr += 0x1000) {
                        pageentr *pe = get_pageentr64(pml4t, vaddr);
#ifdef DEBUG_ELF_ATTRIBUTES
                        hexstr((char (&)[8]) *hex, vaddr);
                        vga.display(23, 40, "Page ");
                        vga.display(23, 45, &(hex[0]));
#endif
                        if (section.sh_flags & SHF_WRITE) {
#ifdef DEBUG_ELF_ATTRIBUTES
                            vga.display(23, 54, "W");
#endif
                            pe->writeable = 1;
                        }
                        if (section.sh_flags & SHF_EXECINSTR) {
#ifdef DEBUG_ELF_ATTRIBUTES
                            vga.display(23, 55, "E");
#endif
                            pe->execution_disabled = 0;
                        }
#ifdef DEBUG_ELF_ATTRIBUTES
                        for (int delay = 0; delay < DEBUG_ELF_ATTRIBUTES_BRAKE_VAL; delay++) {
                            asm("nop");
                        }
#endif
                    }
                }
            }

            /*
             * TODO - Hardcoded to search for space in pt6 page table, the sixth
             * leaf table (table 2M-12M, search area 11M-12M).
             *
             * The stack at will remain available for Additional Processors to
             * bootstrap. See ap_trampoline.S / ap_started.S / ap_bootstrap.cpp
             */
            uint32_t stack_ptr = 0;
            uint16_t stack_pages = 7;
            ++stack_pages; // Crash barrier
            for (int i = 256; i < 511; i++) {
                int j = 0;
                while (j < stack_pages && (i+j) < 512) {
                    if (!pt6[i+j].os_virt_avail || physpageMap->claimed(0xa00 + i + j)) {
                        break;
                    }
                    j++;
                }
                if (stack_pages == j) {
                    stack_ptr = 0xa00 + i;
                    stack_ptr = (stack_ptr << 12) + (stack_pages * 4096);
                    pt6[i].os_virt_avail = 0;
                    pt6[i].os_virt_start = 1;
                    pt6[i].writeable = 0;
                    pt6[i].present = 0;
                    pt6[i].accessed = 0;
                    for (int j = 1; j < stack_pages; j++) {
                        pt6[i+j].os_virt_avail = 0;
                        pt6[i+j].os_virt_start = 0;
                        pt6[i+j].page_ppn = 0xa00 + i + j;
                        physpageMap->claim(0xa00 + i + j);
                        pt6[i+j].writeable = 1;
                        pt6[i+j].present = 1;
                        pt6[i+j].accessed = 0;
                        pt6[i+j].execution_disabled = 1;
                    }
                    goto stack_allocated;
                }
            }
            vga.display(0, 0, "ERROR: failed to allocate stack for next stage  ");
            while(1) {
                asm("hlt");
            }
stack_allocated:

            uint32_t entrypoint_addr = elf64_header.e_entry;

            {
                GDT_table<GDT_SIZE> &init_gdt64 = *((GDT_table<GDT_SIZE> *) GDT_ADDR);
                static_assert(sizeof(init_gdt64) <= GDT_MAX_SIZE);
                memset(&init_gdt64, 0, sizeof(init_gdt64));
                init_gdt64[0] = GDT(0, 0x1FFFF, 0, 0);
                init_gdt64[1] = GDT(0, 0xF0000, 0xA, 0x9A);
                init_gdt64[2] = GDT(0, 0, 0, 0x92);
                vga.display(19, 0, "GDT:");
                uint32_t *gdt64 = (uint32_t *) &init_gdt64;
                asm("lgdt (%0)" :: "r"(init_gdt64.pointer()));
                hexstr((char (&)[8]) *hex, gdt64[0]);
                vga.display(20, 8, hex);
                hexstr((char (&)[8]) *hex, gdt64[1]);
                vga.display(20, 0, hex);
                hexstr((char (&)[8]) *hex, gdt64[2]);
                vga.display(21, 8, hex);
                hexstr((char (&)[8]) *hex, gdt64[3]);
                vga.display(21, 0, hex);
                hexstr((char (&)[8]) *hex, gdt64[4]);
                vga.display(22, 8, hex);
                hexstr((char (&)[8]) *hex, gdt64[5]);
                vga.display(22, 0, hex);
                hexstr((char (&)[8]) *hex, gdt64[6]);
                vga.display(23, 8, hex);
                hexstr((char (&)[8]) *hex, gdt64[7]);
                vga.display(23, 0, hex);
                hexstr((char (&)[8]) *hex, gdt64[8]);
                vga.display(24, 8, hex);
                hexstr((char (&)[8]) *hex, gdt64[9]);
                vga.display(24, 0, hex);

                vga.display(0, 0, "Go for launch! ");

                {
                    uint32_t checklong;
                    asm("mov $0x80000000, %%eax; cpuid; mov %%eax, %0":"=r"(checklong) :: "%eax", "%ebx", "%ecx", "%edx");
                    if (checklong > 0x80000001) {
                        asm("mov $0x80000001, %%eax; cpuid; mov %%edx, %0":"=r"(checklong) :: "%eax", "%ebx", "%ecx", "%edx");
                        if ((checklong & (1 << 29)) == 0) {
                            vga.display(0, 0, "Error: No long mode (64bit) ");
                            while (1) {
                                asm("hlt");
                            }
                        }
                    } else {
                        vga.display(0, 0, "Error: No long mode (64bit) ");
                        while (1) {
                            asm("hlt");
                        }
                    }
                }
                // SSE FP enable:
                {
                    uint32_t cr0;
                    asm("mov %%cr0,%0":"=r"(cr0));
                    cr0 &= 0xFFFFFFFB; // coprocessor emu off
                    cr0 |= 2; // coprocessor monitor
                    asm("mov %0,%%cr0"::"r"(cr0));
                }
                {
                    uint32_t cr4;
                    asm("mov %%cr4,%0":"=r"(cr4));
                    cr4 |= 0x600; // FXSR XMMSEXCEPT
                    asm("mov %0,%%cr4"::"r"(cr4));
                }

                uint32_t cr3 = 0x1000;
                asm("mov %0,%%cr3; " :: "r"(cr3));
                asm("mov %%cr3, %0; " : "=r"(cr3));
                hexstr((char (&)[8]) *hex, (uint32_t) cr3);
                vga.display(1, 40, hex);
                vga.display(1, 36, "cr3=");
                uint32_t cr4;
                asm("mov %%cr4, %0; " : "=r"(cr4));
                cr4 |= 1 << 5;
                asm("mov %0, %%cr4; " :: "r"(cr4));
                asm("mov %%cr4, %0; " : "=r"(cr4));

                // enable long mode: 0x100 (1 << 8) and 0x800 (1 << 11)
                asm("mov $0xC0000080, %%ecx; rdmsr; or $0x900, %%eax; wrmsr; " ::: "%eax", "%ecx");

                uint32_t cr0;
                asm("mov %%cr0, %0" : "=r"(cr0));

                cr0 |= 1 << 16; // Write protected memory enabled
                cr0 |= 1 << 31; // Paging bit = 1
                asm("mov %0, %%cr0" :: "r"(cr0));

            }

            Stage1Data stage1Data = {
                    .multibootAddr = (uint32_t) multiboot_header_addr,
                    .physpageMapAddr = physpagemap_addr
            };
            uint32_t stage1DataPtr = (uint32_t) &stage1Data;

            //asm("mov $0x10,%%ax; mov %%ax, %%ds; mov %%ax, %%es; mov %%ax, %%fs; mov %%ax, %%ss; " ::: "%ax");
            asm("mov %0,%%eax; mov %1, %%ebx; mov %2, %%esi; jmp jumpto64" :: "r"(entrypoint_addr), "r"(stage1DataPtr), "r"(stack_ptr));
        } else {
            vga.display(4, 0, "error: ");
            vga.display(4, 7, kernel.get_error());
        }
    }

    //asm("mov $0x10,%%ax; mov %%ax, %%ds; mov %%ax, %%es; mov %%ax, %%fs; mov %%ax, %%ss; " ::: "%ax");

    while (1) {
    }
}

}