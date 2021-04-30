//
// Created by sigsegv on 30.04.2021.
//

#include <pagealloc.h>
#include <string>
#include "KernelElf.h"

KernelElf::KernelElf(const MultibootInfoHeader &multibootInfoHeader) : ptr(nullptr), end_ptr(nullptr) {
    const MultibootModuleInfo *kernel_info = nullptr;

    if (multibootInfoHeader.has_parts()) {
        const MultibootInfoHeaderPart *mb_hdr = multibootInfoHeader.first_part();
        while (mb_hdr != nullptr) {
            if (mb_hdr->type == 3) {
                if (kernel_info != nullptr) {
                    wild_panic("Multiboot: Which one of the modules is the kernel?");
                }
                kernel_info = &(mb_hdr->get_type3());
            }
            if (mb_hdr->hasNext(multibootInfoHeader)) {
                mb_hdr = mb_hdr->next();
            } else {
                mb_hdr = nullptr;
            }
        }
    } else {
        wild_panic("Couldn't find the kernel in the multiboot2");
    }

    uint64_t vsize = kernel_info->mod_end - kernel_info->mod_start;
    {
        uint64_t overshoot = vsize & 0xFFF;
        if (overshoot != 0) {
            vsize += 0x1000 - overshoot;
        }
    }
    uint64_t vstart = vpagealloc(vsize);
    ptr = (void *) vstart;
    end_ptr = (void *) (vstart + (kernel_info->mod_end - kernel_info->mod_start));
    if (vstart == 0) {
        wild_panic("Failed to alloc vpages for reading kernel elf");
    }
    for (uint64_t i = 0; i < vsize; i += 0x1000) {
        pageentr *pe = get_pageentr64(get_pml4t(), vstart + i);
        pe->page_ppn = (((uint64_t) kernel_info->mod_start) + i) >> 12;
        pe->writeable = 0;
        pe->accessed = 0;
        pe->user_access = 0;
        pe->execution_disabled = 1;
        pe->dirty = 0;
        pe->present = 1;
    }
    reload_pagetables();

    kernel_elf = ELF(ptr, end_ptr);
}

KernelElf::~KernelElf() {
    if (ptr != nullptr) {
        vpagefree((uint64_t) ptr);
        ptr = nullptr;
    }
}

const ELF &KernelElf::elf() const {
    return kernel_elf;
}

static int lim = 0;

std::tuple<uint64_t, std::string> KernelElf::get_symbol(void *ptr) const {
    const ELF64_header &elf64 = kernel_elf.get_elf64_header();
    const auto *symtab = elf64.get_symtab();
    const char *strtab = elf64.get_strtab();
    uint32_t strtab_len = elf64.get_strtab_len();
    if (symtab != nullptr && strtab != nullptr) {
        uint64_t addr = (uint64_t) ptr;
        uint64_t best = 0;
        std::string str{"0x0"};
        for (uint32_t i = 0; i < elf64.get_symbols(*symtab); i++) {
            const ELF64_symbol_entry &sym = elf64.get_symbol(*symtab, i);
            if (sym.st_value < addr && sym.st_value > best && sym.st_name != 0 && sym.st_name < strtab_len) {
                best = sym.st_value;
                str = (strtab + sym.st_name);
            }
        }
        return std::make_tuple<uint64_t, std::string>(best, str);
    }
    return std::make_tuple<uint64_t, std::string>(0, "0x0");
}
