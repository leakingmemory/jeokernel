//
// Created by sigsegv on 18.04.2021.
//

#ifndef JEOKERNEL_MULTIBOOT_H
#define JEOKERNEL_MULTIBOOT_H

struct MultibootInfoHeader;
struct MultibootModuleInfo;
struct MultibootMemoryMap;

struct MultibootInfoHeaderPart {
    uint32_t type;
    uint32_t size;

    uint32_t padded_size() const;

    bool hasNext(const MultibootInfoHeader &header) const;

    const MultibootInfoHeaderPart *next() const;

    const MultibootModuleInfo &get_type3() const;
    const MultibootMemoryMap &get_type6() const;
};

struct MultibootInfoHeader {
    uint32_t total_size;
    uint32_t reserved;

    bool has_parts() const {
        if (total_size > sizeof(MultibootInfoHeader)) {
            uint32_t size = total_size - sizeof(MultibootInfoHeader);
            if (size >= sizeof(MultibootInfoHeaderPart)) {
                auto *f = first_part();
                return (f->size != 0 && size >= f->size);
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
    const MultibootInfoHeaderPart *first_part() const {
        uint8_t *ptr = (uint8_t *) this;
        ptr += sizeof(MultibootInfoHeader);
        return ((MultibootInfoHeaderPart *) ptr);
    }
};

struct MultibootModuleInfo {
    MultibootInfoHeaderPart part_header;
    uint32_t mod_start;
    uint32_t mod_end;
    uint8_t name[8];

    const char *get_name() const {
        return (const char *) &(name[0]);
    }
};

struct MultibootMemoryMapEntry {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
};

struct MultibootMemoryMap {
    MultibootInfoHeaderPart part_header;
    uint32_t entry_size;
    uint32_t entry_version;

    int get_num_entries() const {
        uint32_t entrs = part_header.size - sizeof(*this);
        return entrs / entry_size;
    }
    const MultibootMemoryMapEntry *get_entries_ptr() const {
        uint8_t *ptr = (uint8_t *) this;
        ptr += sizeof(*this);
        return (const MultibootMemoryMapEntry *) ptr;
    }
    const MultibootMemoryMapEntry &get_entry(int n) const {
        int max = get_num_entries();
        if (n >= max) {
            n = max - 1;
        }
        return get_entries_ptr()[n];
    }
} __attribute__((__packed__));

const MultibootInfoHeader &get_multiboot2();

#endif //JEOKERNEL_MULTIBOOT_H
