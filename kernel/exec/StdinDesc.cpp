//
// Created by sigsegv on 6/19/22.
//

#include "StdinDesc.h"
#include <errno.h>

FileDescriptor StdinDesc::Descriptor() {
    std::shared_ptr<StdinDesc> handler{new StdinDesc};
    FileDescriptor fd{handler, 0};
    return fd;
}

std::shared_ptr<kfile> StdinDesc::get_file() {
    return {};
}

bool StdinDesc::can_read() {
    return true;
}

intptr_t StdinDesc::read(void *ptr, intptr_t len) {
    return -EIO;
}

intptr_t StdinDesc::read(void *ptr, intptr_t len, uintptr_t offset) {
    return -EIO;
}

intptr_t StdinDesc::write(const void *ptr, intptr_t len) {
    return 0;
}

bool StdinDesc::stat(struct stat &st) {
    struct stat s{.st_mode = S_IFCHR | 00666, .st_blksize = 1024};
    st = s;
    return true;
}