//
// Created by sigsegv on 18.04.2021.
//

#ifndef JEOKERNEL_ELF_IMPL_H
#define JEOKERNEL_ELF_IMPL_H

#include <elf.h>

ELF::ELF(void *ptr, void *end_ptr) : elf_header((ELF_header *) ptr), valid(false), error(nullptr) {
    {
#ifdef IA32
        uint32_t size = (uint32_t) (((uint64_t) end_ptr) - ((uint64_t) ptr));
#else
        uint64_t size = (uint64_t) (((uint64_t) end_ptr) - ((uint64_t) ptr));
#endif
        if (size < sizeof(ELF64_header)) {
            error = "ELF size";
            return;
        }
    }
    if (read_elf<typename std::remove_const<decltype(elf_header->e_magic)>::type>(elf_header->e_magic) != 0x464C457F) {
        error = "ELF magic";
        return;
    }
    auto e_class = read_elf<typename std::remove_const<decltype(elf_header->e_class)>::type>(elf_header->e_class);
    m64 = (e_class == 2);
    if (!m64 && e_class != 1) {
        error = "ELF class 64/32 invalid";
        return;
    }
    auto e_endian = read_elf<typename std::remove_const<decltype(elf_header->e_endian)>::type>(elf_header->e_endian);
    le = (e_endian == 1);
    if (!le && e_endian != 2) {
        error = "ELF endian le/be invalid";
        return;
    }
    if (read_elf<typename std::remove_const<decltype(elf_header->e_version)>::type>(elf_header->e_version) != 1 || read_elf<typename std::remove_const<decltype(elf_header->e_version2)>::type>(elf_header->e_version2) != 1) {
        error = "ELF version number invalid";
        return;
    }
    if (!m64) {
        error = "32bit ELF not supported";
        return;
    }
    if (!le) {
        error = "BE not supported";
        return;
    }
    auto e_machine = read_elf<typename std::remove_const<decltype(elf_header->e_machine)>::type>(elf_header->e_machine);
#if defined(__aarch64__)
    if (e_machine != EI_ARM64) {
        error = "Not arm64";
        return;
    }
#else
    if (e_machine != EI_AMD_X86_64) {
        error = "Not X86-64";
        return;
    }
#endif
    valid = true;
}

ELF &ELF::operator=(const ELF &cp) {
    elf_header = cp.elf_header;
    valid = cp.valid;
    error = cp.error;
    m64 = cp.m64;
    le = cp.le;
    return *this;
}

#endif //JEOKERNEL_ELF_IMPL_H
