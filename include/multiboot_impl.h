//
// Created by sigsegv on 30.04.2021.
//

#ifndef JEOKERNEL_MULTIBOOT_IMPL_H
#define JEOKERNEL_MULTIBOOT_IMPL_H

#include <multiboot.h>

bool MultibootInfoHeaderPart::hasNext(const MultibootInfoHeader &header) const {
#ifdef IA32
    uint32_t size = (uint32_t) this;
    size -= (uint32_t) &header;
#else
    uint32_t size = (uint64_t) this;
    size -= (uint64_t) &header;
#endif
    size += this->padded_size();
    size += sizeof(MultibootInfoHeaderPart);
    if (size <= header.total_size) {
        size -= sizeof(MultibootInfoHeaderPart);
        auto *n = next();
        if (n->size == 0) {
            return false;
        }
        size += n->size;
        return size <= header.total_size;
    } else {
        return false;
    }
}

const MultibootInfoHeaderPart *MultibootInfoHeaderPart::next() const {
    uint8_t *ptr = (uint8_t *) this;
    ptr += padded_size();
    return ((MultibootInfoHeaderPart *) ptr);
}

uint32_t MultibootInfoHeaderPart::padded_size() const {
    uint8_t off = (uint8_t) (size & 7);
    if (off == 0) {
        return size;
    } else {
        off = 8 - off;
        return size + off;
    }
}

const MultibootModuleInfo &MultibootInfoHeaderPart::get_type3() const {
    return *((MultibootModuleInfo *) this);
}

const MultibootMemoryMap &MultibootInfoHeaderPart::get_type6() const {
    return *((MultibootMemoryMap *) this);
}

const MultibootRsdp1 &MultibootInfoHeaderPart::get_type14() const {
    return *((const MultibootRsdp1 *) this);
}

const MultibootRsdp2 &MultibootInfoHeaderPart::get_type15() const {
    return *((const MultibootRsdp2 *) this);
}

#endif //JEOKERNEL_MULTIBOOT_IMPL_H
