//
// Created by sigsegv on 18.04.2021.
//

#ifndef JEOKERNEL_MULTIBOOT_H
#define JEOKERNEL_MULTIBOOT_H

#include <acpi/ACPI.h>

struct MultibootInfoHeader;
struct MultibootModuleInfo;
struct MultibootMemoryMap;
struct MultibootFramebuffer;
struct MultibootRsdp1;
struct MultibootRsdp2;

struct MultibootInfoHeaderPart {
    uint32_t type;
    uint32_t size;

    uint32_t padded_size() const;

    bool hasNext(const MultibootInfoHeader &header) const;

    const MultibootInfoHeaderPart *next() const;

    const MultibootModuleInfo &get_type3() const;
    const MultibootMemoryMap &get_type6() const;
    const MultibootFramebuffer &get_type8() const;
    const MultibootRsdp1 &get_type14() const;
    const MultibootRsdp2 &get_type15() const;
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

struct MultibootFramebufferColor {

} __attribute__((__packed__));

struct MultibootFramebuffer {
    MultibootInfoHeaderPart part_header;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t reserved;
    /* only available when framebuffer_type == 0 : */
    uint32_t framebuffer_palette_number_of_colors;
    MultibootFramebufferColor framebuffer_palette[1];
} __attribute__((__packed__));

struct MultibootRsdp1 {
    MultibootInfoHeaderPart part_header;
    RSDPv1descriptor rsdp;
} __attribute__((__packed__));

struct MultibootRsdp2 {
    MultibootInfoHeaderPart part_header;
    RSDPv2descriptor rsdp;
} __attribute__((__packed__));

const MultibootInfoHeader &get_multiboot2();

#endif //JEOKERNEL_MULTIBOOT_H
