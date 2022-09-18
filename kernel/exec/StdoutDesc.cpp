//
// Created by sigsegv on 6/19/22.
//

#include "StdoutDesc.h"
#include <string>
#include <klogger.h>
#include <errno.h>

FileDescriptor StdoutDesc::StdoutDescriptor() {
    std::shared_ptr<StdoutDesc> desc{new StdoutDesc};
    FileDescriptor fd{desc, 1};
    return fd;
}

FileDescriptor StdoutDesc::StderrDescriptor() {
    std::shared_ptr<StdoutDesc> desc{new StdoutDesc};
    FileDescriptor fd{desc, 2};
    return fd;
}

std::shared_ptr<kfile> StdoutDesc::get_file() {
    return {};
}

intptr_t StdoutDesc::read(void *ptr, intptr_t len) {
    return -EIO;
}

intptr_t StdoutDesc::read(void *ptr, intptr_t len, uintptr_t offset) {
    return -EIO;
}

intptr_t StdoutDesc::write(const void *ptr, intptr_t len) {
    if (len > 0) {
        std::string str{(const char *) ptr, (std::size_t) len};
        get_klogger() << str.c_str();
        return len;
    }
    return 0;
}

bool StdoutDesc::stat(struct stat &st) {
    struct stat s{.st_mode = S_IFCHR | 00666, .st_blksize = 1024};
    st = s;
    return true;
}