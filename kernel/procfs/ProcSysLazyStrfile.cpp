//
// Created by sigsegv on 2/28/23.
//

#include "ProcSysLazyStrfile.h"

std::size_t ProcSysLazyStrfile::Size() {
    {
        std::unique_lock lock{mtx};
        if (!initialized) {
            lock.release();
            std::string str = GetContent();
            return str.size();
        }
    }
    return str.size();
}

file_read_result ProcSysLazyStrfile::Read(uint64_t offset, void *ptr, std::size_t length) {
    {
        std::unique_lock lock{mtx};
        if (!initialized) {
            lock.release();
            std::string str = GetContent();
            lock = {mtx};
            if (!initialized) {
                this->str = str;
                initialized = true;
            }
        }
    }
    const char *p = str.c_str();
    uint64_t size = str.size();
    if (offset > size) {
        return {.size = 0, .status = fileitem_status::SUCCESS};
    }
    p += offset;
    size -= offset;
    if (size > length) {
        size = length;
    }
    uint64_t max = 0xffffffff;
    if (size > max) {
        size = max;
    }
    memcpy(ptr, p, (size_t) size);
    return {.size = (size_t) size, .status = fileitem_status::SUCCESS};
}