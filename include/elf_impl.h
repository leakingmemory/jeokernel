//
// Created by sigsegv on 18.04.2021.
//

#ifndef JEOKERNEL_ELF_IMPL_H
#define JEOKERNEL_ELF_IMPL_H

#define ET_NONE 0
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3
#define ET_CORE 4
#define ET_LOOS 0xFE00
#define ET_HIOS 0xFEFF
#define ET_LOPROC 0xFF00
#define ET_HIPROC 0xFFFF

#define EI_NONE 0x00
#define EI_ATWE32100 0x01
#define EI_SPARC 0x02
#define EI_X86 0x03
#define EI_M68K 0x04
#define EI_M88K 0x05
#define EI_INTEL_MCU 0x06
#define EI_80860 0x07
#define EI_MIPS 0x08
#define EI_IBM_SYS370 0x09
#define EI_MIPS_RS3K_LE 0x0A
#define EI_HP_PA_RISC 0x0E
#define EI_INTEL80960 0x13
#define EI_POWERPC 0x14
#define EI_POWERPC64 0x15
#define EI_S390X 0x16
#define EI_IBM_SPU_SPC 0x17
#define EI_NEC_V800 0x24
#define EI_FUJITSU_FR20 0x25
#define EI_TRW_RH32 0x26
#define EI_MOTOROLA_RCE 0x27
#define EI_ARM 0x28
#define EI_ALPHA 0x29
#define EI_SUPERH 0x2A
#define EI_SPARC9 0x2B
#define EI_SIEMENS_TRICORE 0x2C
#define EI_ARGONAUT_RISC 0x2D
#define EI_HITACHI_H8_300 0x2E
#define EI_HITACHI_H8_300H 0x2F
#define EI_HITACHI_H8S 0x30
#define EI_HITACHI_H8_500 0x31
#define EI_IA64 0x32
#define EI_STANFORD_MIPS_X 0x33
#define EI_MOTOROLA_COLDFIRE 0x34
#define EI_MOTOROLA_M68HC12 0x35
#define EI_FUJITSU_MMA_MULTIMEDIA_ACCEL 0x36
#define EI_SIEMENS_PCP 0x37
#define EI_SONY_NCPU_RISC 0x38
#define EI_DENSO_NDR1 0x39
#define EI_MOTOROLA_STARCORE 0x3A
#define EI_TOYOTA_ME16 0x3B
#define EI_STM_ST100 0x3C
#define EI_ADV_LOGIC_TINYJ 0x3D
#define EI_AMD_X86_64 0x3E
#define EI_TMS320C6000 0x8C
#define EI_ARM64 0xB7
#define EI_RISC5 0xF3
#define EI_BPF 0xF7
#define EI_WDC_65C816 0x101


struct ELF_header {
    uint32_t e_magic;
    uint8_t e_class;
    uint8_t e_endian;
    uint8_t e_version;
    uint8_t e_osabi;
    uint64_t e_abiversion;
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version2;
};
static_assert(sizeof(ELF_header) == 0x18);

struct ELF64_program_entry {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct ELF64_header {
    ELF_header start;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;

    const ELF64_program_entry &get_program_entry(uint16_t index) const {
        uint8_t *ptr = (uint8_t *) this;
        ptr += e_phoff;
        index = index % e_phnum;
        ptr += e_phentsize * index;
        ELF64_program_entry *pe = (ELF64_program_entry *) ptr;
        return *pe;
    }
};

class ELF {
private:
    ELF_header &elf_header;
    ELF64_header &elf64_header;
    bool valid;
    const char *error;
    bool m64;
    bool le;
public:
    ELF(void *ptr, void *end_ptr);
    bool is_valid() const {
        return valid;
    }
    const char *get_error() const {
        return error;
    }
    const ELF_header &get_elf_header() const {
        return elf_header;
    }
    const ELF64_header &get_elf64_header() const {
        return elf64_header;
    }
};

ELF::ELF(void *ptr, void *end_ptr) : elf_header(*((ELF_header *) ptr)), elf64_header(*((ELF64_header *) ptr)), valid(false), error(nullptr) {
    {
        uint32_t size = (uint32_t) (((uint64_t) end_ptr) - ((uint64_t) ptr));
        if (size < sizeof(ELF64_header)) {
            error = "ELF size";
            return;
        }
    }
    if (elf_header.e_magic != 0x464C457F) {
        error = "ELF magic";
        return;
    }
    m64 = (elf_header.e_class == 2);
    if (!m64 && elf_header.e_class != 1) {
        error = "ELF class 64/32 invalid";
        return;
    }
    le = (elf_header.e_endian == 1);
    if (!le && elf_header.e_endian != 2) {
        error = "ELF endian le/be invalid";
        return;
    }
    if (elf_header.e_version != 1 || elf_header.e_version2 != 1) {
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
    if (elf_header.e_machine != EI_AMD_X86_64) {
        error = "Not X86-64";
        return;
    }
    valid = true;
}

#endif //JEOKERNEL_ELF_IMPL_H
