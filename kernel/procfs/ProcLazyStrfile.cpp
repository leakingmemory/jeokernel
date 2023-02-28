//
// Created by sigsegv on 2/26/23.
//

#include "ProcLazyStrfile.h"
#include <iostream>

std::size_t ProcLazyStrfile::Size() {
    {
        std::unique_lock lock{mtx};
        if (!initialized) {
            lock.release();
            std::shared_ptr<Process> process = this->process.lock();
            if (process) {
                std::string str = GetContent(*process);
                return str.size();
            } else {
                return 0;
            }
        }
    }
    return str.size();
}

file_read_result ProcLazyStrfile::Read(uint64_t offset, void *ptr, std::size_t length) {
    {
        std::unique_lock lock{mtx};
        if (!initialized) {
            lock.release();
            std::shared_ptr<Process> process = this->process.lock();
            std::string str{};
            if (process) {
                str = GetContent(*process);
            }
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