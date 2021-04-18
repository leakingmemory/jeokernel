//
// Created by sigsegv on 18.04.2021.
//

#ifndef JEOKERNEL_MULTIBOOT_H
#define JEOKERNEL_MULTIBOOT_H

struct MultibootInfoHeader;
struct MultibootModuleInfo;

struct MultibootInfoHeaderPart {
    uint32_t type;
    uint32_t size;

    uint32_t padded_size() const;

    bool hasNext(MultibootInfoHeader &header) const;

    MultibootInfoHeaderPart &next() const;

    MultibootModuleInfo &get_type3() const;
};

struct MultibootInfoHeader {
    uint32_t total_size;
    uint32_t reserved;

    bool has_parts() {
        if (total_size > sizeof(MultibootInfoHeader)) {
            uint32_t size = total_size - sizeof(MultibootInfoHeader);
            if (size >= sizeof(MultibootInfoHeaderPart)) {
                auto &f = first_part();
                return (f.size != 0 && size >= f.size);
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
    MultibootInfoHeaderPart &first_part() {
        uint8_t *ptr = (uint8_t *) this;
        ptr += sizeof(MultibootInfoHeader);
        return *((MultibootInfoHeaderPart *) ptr);
    }
};

uint32_t MultibootInfoHeaderPart::padded_size() const {
    uint8_t off = (uint8_t) (size & 7);
    if (off == 0) {
        return size;
    } else {
        off = 8 - off;
        return size + off;
    }
}

bool MultibootInfoHeaderPart::hasNext(MultibootInfoHeader &header) const {
    uint32_t size = (uint32_t) this;
    size -= (uint32_t) &header;
    size += this->padded_size();
    size += sizeof(MultibootInfoHeaderPart);
    if (size <= header.total_size) {
        size -= sizeof(MultibootInfoHeaderPart);
        auto &n = next();
        if (n.size == 0) {
            return false;
        }
        size += n.size;
        return size <= header.total_size;
    } else {
        return false;
    }
}
MultibootInfoHeaderPart &MultibootInfoHeaderPart::next() const {
    uint8_t *ptr = (uint8_t *) this;
    ptr += padded_size();
    return *((MultibootInfoHeaderPart *) ptr);
}

struct MultibootModuleInfo {
    struct MultibootInfoHeaderPart part_header;
    uint32_t mod_start;
    uint32_t mod_end;
    uint8_t name[8];

    const char *get_name() const {
        return (const char *) &(name[0]);
    }
};

MultibootModuleInfo &MultibootInfoHeaderPart::get_type3() const {
    return *((MultibootModuleInfo *) this);
}

#endif //JEOKERNEL_MULTIBOOT_H
