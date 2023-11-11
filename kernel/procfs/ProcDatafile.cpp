//
// Created by sigsegv on 2/23/23.
//

#include "ProcDatafile.h"

ProcDatafile *ProcDatafile::GetResource() {
    return this;
}

uint32_t ProcDatafile::Mode() {
    return mode;
}

void ProcDatafile::SetMode(uint32_t mode) {
    this->mode = mode;
}

uintptr_t ProcDatafile::InodeNum() {
    return 0;
}

uint32_t ProcDatafile::BlockSize() {
    return 0;
}

uintptr_t ProcDatafile::SysDevId() {
    return 0;
}

file_getpage_result ProcDatafile::GetPage(std::size_t pagenum) {
    return {.page = {}, .status = fileitem_status::NOT_SUPPORTED_FS_FEATURE};
}