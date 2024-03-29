//
// Created by sigsegv on 6/9/22.
//

#include <iostream>
#include <kfs/kfiles.h>
#include "UserElf.h"

UserElf::UserElf() : referrer("UserElf") {
}

std::string UserElf::GetReferrerIdentifier() {
    return "";
}

void UserElf::Init(const std::shared_ptr<UserElf> &selfRef, const reference<kfile> &file) {
    std::weak_ptr<UserElf> weakPtr{selfRef};
    this->weakPtr = weakPtr;
    std::shared_ptr<class referrer> referrer = selfRef;
    this->file = file.CreateReference(referrer);
    PostInit();
}

void UserElf::PostInit() {
    if (file->Size() < sizeof(elf64Header)) {
        return;
    }
    {
        auto readResult = file->Read(0, (void *) &elf64Header, sizeof(elf64Header));
        if (readResult.result != sizeof(elf64Header)) {
            std::cerr << "ELF: Error reading elf header\n";
            return;
        }
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

std::shared_ptr<UserElf> UserElf::Create(const reference<kfile> &file) {
    std::shared_ptr<UserElf> userElf{new UserElf()};
    userElf->Init(userElf, file);
    return userElf;
}

std::shared_ptr<ELF64_program_entry> UserElf::get_program_entry(uint16_t index) {
    std::shared_ptr<ELF64_program_entry> pe{new ELF64_program_entry};
    auto offset = elf64Header.get_program_entry_offset(index);
    auto rd = file->Read(offset, (void *) &(*pe), sizeof(*pe));
    if (rd.result == sizeof(*pe)) {
        return pe;
    }
    return {};
}

std::shared_ptr<ELF64_section_entry> UserElf::get_section_entry(uint16_t index) {
    std::shared_ptr<ELF64_section_entry> se{new ELF64_section_entry};
    auto offset = elf64Header.get_section_entry_offset(index);
    auto rd = file->Read(offset, (void *) &(*se), sizeof(*se));
    if (rd.result == sizeof(*se)) {
        return se;
    }
    return {};
}
