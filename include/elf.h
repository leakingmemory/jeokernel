//
// Created by sigsegv on 30.04.2021.
//

#ifndef JEOKERNEL_ELF_H
#define JEOKERNEL_ELF_H

#include <stdint.h>
#include <string.h>

#if defined(__cplusplus) && !defined(IA32)
#include <string>
#endif

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

#define PHT_NULL    0
#define PHT_LOAD    1
#define PHT_DYNAMIC 2
#define PHT_INTERP  3
#define PHT_NOTE    4
#define PHT_SHLIB   5
#define PHT_PHDR    6
#define PHT_TLS     7
#define PHT_LOOS    0x60000000
#define PHT_HIOS    0x6FFFFFFF
#define PHT_LOPROC  0x70000000
#define PHT_HIPROC  0x7FFFFFFF

#define SHF_WRITE               0x1
#define SHF_ALLOC               0x2
#define SHF_EXECINSTR           0x4
#define SHF_MERGE               0x10
#define SHF_STRINGS             0x20
#define SHF_INFO_LINK           0x40
#define SHF_LINK_ORDER          0x80
#define SHF_OS_NONCONFORMING 	0x100
#define SHF_GROUP               0x200
#define SHF_TLS                 0x400
#define SHF_MASKOS              0x0ff00000
#define SHF_MASKPROC            0xf0000000
#define SHF_ORDERED             0x4000000
#define SHF_EXCLUDE             0x8000000

#define SHT_NULL            0x0
#define SHT_PROGBITS        0x1
#define SHT_SYMTAB          0x2
#define SHT_STRTAB          0x3
#define SHT_RELA            0x4
#define SHT_HASH            0x5
#define SHT_DYNAMIC         0x6
#define SHT_NOTE            0x7
#define SHT_NOBITS          0x8
#define SHT_REL             0x9
#define SHT_SHLIB           0xA
#define SHT_DYNSYM          0xB
#define SHT_INIT_ARRAY      0xE
#define SHT_FINI_ARRAY      0xF
#define SHT_PREINIT_ARRAY   0x10
#define SHT_GROUP           0x11
#define SHT_SYMTAB_SHNDX    0x12
#define SHT_NUM             0x13

struct ELF32_section_entry {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
} __attribute__((__packed__));

struct ELF64_section_entry {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} __attribute__((__packed__));

#ifdef __cplusplus
typedef ELF64_section_entry Elf64_Shdr;
typedef ELF32_section_entry Elf32_Shdr;
#else
typedef struct ELF64_section_entry Elf64_Shdr;
typedef struct ELF32_section_entry Elf32_Shdr;
#endif

#ifdef __cplusplus

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

static_assert(sizeof(ELF64_section_entry) == 0x40);

struct ELF64_symbol_entry {
    uint32_t st_name;
    uint8_t st_info;
    uint8_t st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} __attribute__((__packed__));
static_assert(sizeof(ELF64_symbol_entry) == 0x18);

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

    uintptr_t get_program_entry_offset(uint16_t index) const {
        uintptr_t off = e_phoff;
        index = index % e_phnum;
        off += e_phentsize * index;
        return off;
    }
    const ELF64_program_entry &get_program_entry(uint16_t index) const {
        uint8_t *ptr = (uint8_t *) this;
        ptr += get_program_entry_offset(index);
        ELF64_program_entry *pe = (ELF64_program_entry *) ptr;
        return *pe;
    }
    uintptr_t get_section_entry_offset(uint16_t index) const {
        uintptr_t off = e_shoff;
        index = index % e_shnum;
        off += e_shentsize * index;
        return off;
    }
    const ELF64_section_entry &get_section_entry(uint16_t index) const {
        uint8_t *ptr = (uint8_t *) this;
        ptr += e_shoff;
        index = index % e_shnum;
        ptr += e_shentsize * index;
        ELF64_section_entry *se = (ELF64_section_entry *) ptr;
        return *se;
    }
    const ELF64_section_entry *get_rela_dyn_section() const {
        const char *strtab = nullptr;
        for (uint16_t i = 0; i < e_shnum; i++) {
            const ELF64_section_entry &se = get_section_entry(i);
            const int8_t *ptr = (int8_t *) ((void *) &start);
            ptr += se.sh_offset;
            const char *str = (const char *) ptr;
            if (se.sh_type == SHT_RELA && strcmp(get_cstring(se.sh_name), ".rela.dyn") == 0) {
                return &se;
            }
        }
        return nullptr;
    }
    const ELF64_section_entry *get_dynstr_section() const {
        const char *strtab = nullptr;
        for (uint16_t i = 0; i < e_shnum; i++) {
            const ELF64_section_entry &se = get_section_entry(i);
            const int8_t *ptr = (int8_t *) ((void *) &start);
            ptr += se.sh_offset;
            const char *str = (const char *) ptr;
            if (se.sh_type == SHT_STRTAB && strcmp(get_cstring(se.sh_name), ".dynstr") == 0) {
                return &se;
            }
        }
        return nullptr;
    }
    const char *get_strtab() const {
        const char *strtab = nullptr;
        for (uint16_t i = 0; i < e_shnum; i++) {
            const ELF64_section_entry &se = get_section_entry(i);
            const int8_t *ptr = (int8_t *) ((void *) &start);
            ptr += se.sh_offset;
            const char *str = (const char *) ptr;
            if (se.sh_type == SHT_STRTAB && se.sh_name < se.sh_size) {
                strtab = str;
            }
        }
        return strtab;
    }
    uint32_t get_strtab_len() const {
        uint32_t size = 0;
        for (uint16_t i = 0; i < e_shnum; i++) {
            const ELF64_section_entry &se = get_section_entry(i);
            if (se.sh_type == SHT_STRTAB && se.sh_name < se.sh_size) {
                size = se.sh_size;
            }
        }
        return size;
    }

    const ELF64_section_entry *get_symtab() const {
        for (uint16_t i = 0; i < e_shnum; i++) {
            const ELF64_section_entry &se = get_section_entry(i);
            if (se.sh_type == SHT_SYMTAB) {
                return &se;
            }
        }
        return nullptr;
    }

    const ELF64_section_entry *get_dynsym() const {
        for (uint16_t i = 0; i < e_shnum; i++) {
            const ELF64_section_entry &se = get_section_entry(i);
            if (se.sh_type == SHT_DYNSYM) {
                return &se;
            }
        }
        return nullptr;
    }

    const ELF64_section_entry *get_init_table() const {
        for (uint16_t i = 0; i < e_shnum; i++) {
            const ELF64_section_entry &se = get_section_entry(i);
            if (se.sh_type == SHT_INIT_ARRAY) {
                return &se;
            }
        }
        return nullptr;
    }

    static const char *get_strtab_cstring(const char *strtab, uint32_t strtab_len, uint32_t index) {
        if (index >= strtab_len) {
            return "";
        }
        const char *str = strtab + index;
        return str;
    }
    const char *get_strtab_cstring(ELF64_section_entry *section, uint32_t index) {
        if (section != nullptr) {
            const int8_t *ptr = (int8_t *) ((void *) &start);
            ptr += section->sh_offset;
            const char *str = (const char *) ptr;
            return get_strtab_cstring(str, section->sh_size, index);
        }
        return nullptr;
    }

    const char *get_cstring(uint32_t index) const {
        const char *strtab = get_strtab();
        auto strtab_len = get_strtab_len();
        return get_strtab_cstring(strtab, strtab_len, index);
    }

#if defined(__cplusplus) && !defined(LOADER)
    std::string get_string(uint32_t index) const {
        return std::string(get_cstring(index));
    }

    const ELF64_section_entry *get_ctors_table() const {
        for (uint16_t i = 0; i < e_shnum; i++) {
            const ELF64_section_entry &se = get_section_entry(i);
            if (se.sh_type == SHT_PROGBITS && get_string(se.sh_name) == ".ctors") {
                return &se;
            }
        }
        return nullptr;
    }
#endif

    const uint32_t get_symbols(const ELF64_section_entry &symtab) const {
        return symtab.sh_size / sizeof(ELF64_symbol_entry);
    }
    const ELF64_symbol_entry &get_symbol(const ELF64_section_entry &symtab, uint32_t i) const {
        uint8_t *ptr = (uint8_t *) (void *) this;
        ptr += symtab.sh_offset;
        ELF64_symbol_entry *syms = (ELF64_symbol_entry *) (void *) ptr;
        i = i % get_symbols(symtab);
        return syms[i];
    }
};

#define R_X86_64_NONE       0
#define R_X86_64_64         1
#define R_X86_64_GLOB_DAT   6
#define R_X86_64_JUMP_SLOT  7
#define R_X86_64_RELATIVE   8

struct ELF64_rela_dyn {
    uint64_t offset;
    uint32_t rela_type;
    uint32_t sym_index;
    int64_t addendum;
};

struct ELF64_auxv {
    uint64_t type;
    union {
        int64_t value;
        uintptr_t uintptr;
    };
};

#define AT_NULL     0
#define AT_PHDR     3
#define AT_PHENT    4
#define AT_PHNUM    5
#define AT_PAGESZ   6
#define AT_BASE     7
#define AT_ENTRY    9
#define AT_RANDOM   25

class ELF {
private:
    union {
        ELF_header *elf_header;
        ELF64_header *elf64_header;
    };
    bool valid;
    const char *error;
    bool m64;
    bool le;
public:
    ELF() : elf_header(nullptr), valid(false) { }

    ELF(void *ptr, void *end_ptr);

    ELF &operator =(const ELF &);

    bool is_valid() const {
        return valid;
    }
    const char *get_error() const {
        return error;
    }
    const ELF_header &get_elf_header() const {
        return *elf_header;
    }
    const ELF64_header &get_elf64_header() const {
        return *elf64_header;
    }
};
#endif

#endif //JEOKERNEL_ELF_H
