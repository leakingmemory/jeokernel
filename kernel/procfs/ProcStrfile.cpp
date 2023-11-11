//
// Created by sigsegv on 2/24/23.
//

#include "ProcStrfile.h"
#include "procfs_fsresourcelockfactory.h"
#include <string.h>

std::shared_ptr<ProcStrfile> ProcStrfile::Create(const std::string &str) {
    procfs_fsresourcelockfactory lockfactory{};
    std::shared_ptr<ProcStrfile> procStrfile{new ProcStrfile(lockfactory, str)};
    procStrfile->SetSelfRef(procStrfile);
    return procStrfile;
}

std::size_t ProcStrfile::Size() {
    return str.size();
}

file_read_result ProcStrfile::Read(uint64_t offset, void *ptr, std::size_t length) {
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