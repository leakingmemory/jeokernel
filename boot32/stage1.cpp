
#include "plainvga32.h"
#include <strs.h>
#include <pagetable_impl.h>
#include <multiboot.h>
#include <elf_impl.h>

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

void boot_stage1(void *multiboot_header_addr) {
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
    uint64_t (&pt_raw5)[512] = *((uint64_t (*)[512]) 0x8000);
    pageentr (&pt5)[512] = (pageentr (&)[512]) pt_raw5;

    for (uint16_t i = 0; i < 512; i++) {
        pml4t_raw[i] = 0;
        pdpt_raw[i] = 0;
        pdt_raw[i] = 0;
        pt_raw[i] = 0;
        pt_raw2[i] = 0;
        pt_raw3[i] = 0;
        pt_raw4[i] = 0;
        pt_raw5[i] = 0;
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
    pdt[4].page_ppn = 0x8000 / 4096;
    pdt[4].writeable = 1;
    pdt[4].present = 1;
    pdt[4].os_virt_avail = 1;
    /*
     * Make first 2MiB adressable,writable,execable
     */
    for (uint16_t i = 1; i < 512; i++) {
        pt[i].page_ppn = i;
        pt[i].writeable = 1;
        pt[i].present = 1;
        pt2[i].os_virt_avail = 1;
        pt3[i].os_virt_avail = 1;
        pt4[i].os_virt_avail = 1;
        pt5[i].os_virt_avail = 1;
    }
    /*
     * Make second half allocateable
     */
    for (uint16_t i = 256; i < 512; i++) {
        pt[i].os_phys_avail = 1;
        pt[i].os_virt_avail = 1;
    }
    vga.display(0, 0, "/");

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
                auto &pe = get_pageentr64(pml4t, phaddr);
                pe.os_virt_avail = 0;
                pe.os_virt_start = 1;
                pe.os_phys_avail = 0;
                phaddr += 4096;
                vga.display(0, 0, "#");
            }
        }
        bool multiple_modules = false;
        int ln = 3;
        vga.display(0, 0, "!");
        if (header.has_parts()) {
            vga.display(0, 0, "-");
            MultibootInfoHeaderPart *part = &header.first_part();
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
                    MultibootModuleInfo &moduleInfo = part->get_type3();
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
                    part = &part->next();
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
            pageentr &pe = get_pageentr64(pml4t, paddr);
            pe.os_phys_avail = 0;
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
                    uint32_t phdata = kernel_elf_end;
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
                            pageentr &phys_pe = get_pageentr64(pml4t, phaddr);
                            phys_pe.os_phys_avail = 0;
                            uint32_t page_ppn = phaddr / 4096;
                            vga.display(ln, 0, "pagemap ");
                            hexstr((char (&)[8]) *hex, vaddr);
                            vga.display(ln, 8, hex);
                            pageentr &pe = get_pageentr64(pml4t, vaddr);
                            pe.os_virt_avail = 0;
                            pe.os_virt_start = 1;
                            hexstr((char (&)[8]) *hex, pe.page_ppn);
                            vga.display(ln, 17, hex);
                            pe.page_ppn = page_ppn;
                            vga.display(ln, 26, " -> ");
                            hexstr((char (&)[8]) *hex, pe.page_ppn);
                            vga.display(ln, 30, hex);
                            ln++;

                            vaddr += 4096;
                            phaddr += 4096;
                        }
                    } else {
                        while (vaddr < vaddr_end) {
                            pageentr &phys_pe = get_pageentr64(pml4t, phdata);
                            phys_pe.os_phys_avail = 0;
                            uint32_t page_ppn = phdata / 4096;
                            vga.display(ln, 0, "pagzmap ");
                            hexstr((char (&)[8]) *hex, vaddr);
                            vga.display(ln, 8, hex);
                            pageentr &pe = get_pageentr64(pml4t, vaddr);
                            hexstr((char (&)[8]) *hex, pe.page_ppn);
                            pe.os_virt_avail = 0;
                            pe.os_virt_start = 1;
                            vga.display(ln, 17, hex);
                            pe.page_ppn = page_ppn;
                            vga.display(ln, 26, " -> ");
                            hexstr((char (&)[8]) *hex, pe.page_ppn);
                            vga.display(ln, 30, hex);
                            ln++;

                            {
                                uint8_t *z = (uint8_t *) phdata;
                                for (int i = 0; i < 4096; i++) {
                                    z[i] = 0;
                                }
                            }

                            vaddr += 4096;
                            phdata += 4096;
                        }
                    }
                }
            }

            {
                GDT_table<3> &init_gdt64 = *((GDT_table<3> *) 0x09000);
                init_gdt64 = GDT_table<3>();
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

                uint32_t cr3 = 0x1000;
                asm("mov %0,%%cr3; " :: "r"(cr3));
                asm("mov %%cr3, %0; " : "=r"(cr3));
                hexstr((char (&)[8]) *hex, (uint32_t) cr3);
                vga.display(1, 40, hex);
                vga.display(1, 36, "cr3=");
                uint32_t cr4;
                asm("mov %%cr4, %0; " : "=r"(cr4));
                hexstr((char (&)[8]) *hex, (uint32_t) cr4);
                vga.display(1, 53, hex);
                vga.display(1, 49, "cr4#");
                cr4 |= 1 << 5;
                asm("mov %0, %%cr4; " :: "r"(cr4));
                asm("mov %%cr4, %0; " : "=r"(cr4));
                hexstr((char (&)[8]) *hex, (uint32_t) cr4);
                vga.display(1, 53, hex);
                vga.display(1, 49, "cr4=");

                uint32_t efer = rdmsr(0xC0000080);
                hexstr((char (&)[8]) *hex, (uint32_t) efer);
                vga.display(1, 67, hex);
                vga.display(1, 62, "efer#");

                wrmsr(0xC0000080, efer | /*long mode enable:*/(1 << 8) | /*no exec bit enabl:*/(1 << 11));

                efer = rdmsr(0xC0000080);
                hexstr((char (&)[8]) *hex, (uint32_t) efer);
                vga.display(1, 67, hex);
                vga.display(1, 62, "efer=");

                uint32_t cr0;
                asm("mov %%cr0, %0" : "=r"(cr0));
                hexstr((char (&)[8]) *hex, (uint32_t) cr0);
                vga.display(2, 4, hex);
                vga.display(2, 0, "cr0#");

                cr0 |= 1 << 16; // Write protected memory enabled
                cr0 |= 1 << 31; // Paging bit = 1
                asm("mov %0, %%cr0" :: "r"(cr0));

                asm("mov %%cr0, %0" : "=r"(cr0));
                //hexstr((char (&)[8]) *hex, (uint32_t) cr0);
                //vga.display(2, 4, hex);
                //vga.display(2, 0, "cr0=");

            }


            for (int i = 0; i < 1000000; i++) {
            }
            //asm("mov $0x10,%%ax; mov %%ax, %%ds; mov %%ax, %%es; mov %%ax, %%fs; mov %%ax, %%ss; " ::: "%ax");
            asm("mov %0,%%eax; mov %1, %%ebx; jmp jumpto64" :: "r"(elf64_header.e_entry), "r"(multiboot_header_addr));
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