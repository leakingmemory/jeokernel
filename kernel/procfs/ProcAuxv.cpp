//
// Created by sigsegv on 2/23/23.
//

#include "ProcAuxv.h"
#include <string.h>

std::size_t ProcAuxv::Size() {
    return sizeof((*auxv)[0]) * auxv->size();
}

file_read_result ProcAuxv::Read(uint64_t offset, void *ptr, std::size_t length) {
    auto size = sizeof((*auxv)[0]) * auxv->size();
    if (offset >= size) {
        return {.size = 0, .status = fileitem_status::SUCCESS};
    }
    size -= offset;
    if (length > size) {
        length = size;
    }
    if (length == 0) {
        return {.size = 0, .status = fileitem_status::SUCCESS};
    }
    auto *buf = (uint8_t *) auxv->data();
    buf += offset;
    memcpy(ptr, buf, length);
    return {.size = length, .status = fileitem_status::SUCCESS};
}
