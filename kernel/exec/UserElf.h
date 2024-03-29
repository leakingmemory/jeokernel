//
// Created by sigsegv on 6/9/22.
//

#ifndef JEOKERNEL_USERELF_H
#define JEOKERNEL_USERELF_H

#include <memory>
#include <elf.h>

class kfile;

class UserElf : public referrer {
private:
    std::weak_ptr<UserElf> weakPtr{};
    reference<kfile> file{};
    ELF64_header elf64Header{};
    bool m64;
    bool le;
    bool valid{false};
    UserElf();
    std::string GetReferrerIdentifier() override;
    void Init(const std::shared_ptr<UserElf> &selfRef, const reference<kfile> &file);
    void PostInit();
public:
    static std::shared_ptr<UserElf> Create(const reference<kfile> &file);
    bool is_valid() const {
        return valid;
    }
    uintptr_t get_program_header_addr() const {
        return elf64Header.e_phoff;
    }
    uint16_t get_size_of_program_entry() const {
        return elf64Header.e_phentsize;
    }
    uint16_t get_num_program_entries() const {
        return elf64Header.e_phnum;
    }
    std::shared_ptr<ELF64_program_entry> get_program_entry(uint16_t index);
    uint16_t get_num_section_entries() const {
        return elf64Header.e_shnum;
    }
    std::shared_ptr<ELF64_section_entry> get_section_entry(uint16_t index);

    uint64_t get_entrypoint_addr() const {
        return elf64Header.e_entry;
    }
};


#endif //JEOKERNEL_USERELF_H
