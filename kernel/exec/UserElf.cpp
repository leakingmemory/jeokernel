//
// Created by sigsegv on 6/9/22.
//

#include <iostream>
#include <kfs/kfiles.h>
#include "UserElf.h"

UserElf::UserElf(const std::shared_ptr<kfile> &file) : file(file), elf64Header(), valid(false) {
    if (file->Size() < sizeof(elf64Header)) {
        return;
    }
    if (file->Read(0, (void *) &elf64Header, sizeof(elf64Header)) != sizeof(elf64Header)) {
        std::cerr << "ELF: Error reading elf header\n";
        return;
    }
    if (elf64Header.start.e_magic != 0x464C457F) {
        return;
    }
    m64 = (elf64Header.start.e_class == 2);
    if (!m64 && elf64Header.start.e_class != 1) {
        return;
    }
    le = (elf64Header.start.e_endian == 1);
    if (!le && elf64Header.start.e_endian != 2) {
        return;
    }
    if (elf64Header.start.e_version != 1 || elf64Header.start.e_version2 != 1) {
        return;
    }
    if (!m64) {
        return;
    }
    if (!le) {
        return;
    }
    if (elf64Header.start.e_machine != EI_AMD_X86_64) {
        return;
    }
    valid = true;
}

std::shared_ptr<ELF64_program_entry> UserElf::get_program_entry(uint16_t index) {
    std::shared_ptr<ELF64_program_entry> pe{new ELF64_program_entry};
    auto offset = elf64Header.get_program_entry_offset(index);
    auto rd = file->Read(offset, (void *) &(*pe), sizeof(*pe));
    if (rd == sizeof(*pe)) {
        return pe;
    }
    return {};
}

std::shared_ptr<ELF64_section_entry> UserElf::get_section_entry(uint16_t index) {
    std::shared_ptr<ELF64_section_entry> se{new ELF64_section_entry};
    auto offset = elf64Header.get_section_entry_offset(index);
    auto rd = file->Read(offset, (void *) &(*se), sizeof(*se));
    if (rd == sizeof(*se)) {
        return se;
    }
    return {};
}
