//
// Created by sigsegv on 6/19/22.
//

#include "StdoutDesc.h"
#include <string>
#include <klogger.h>
#include <errno.h>
#include <iostream>
#include <exec/procthread.h>

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

bool StdoutDesc::can_read() {
    return false;
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

file_descriptor_result StdoutDesc::ioctl(intptr_t cmd, intptr_t arg, std::function<void (intptr_t)> func) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    if (process == nullptr) {
        return {.result = -EOPNOTSUPP, .async = false};
    }
    auto tty = process->GetProcess()->GetTty();
    if (tty) {
        auto result = tty->ioctl(cmd, arg, func);
        return {.result = result.result, .async = result.async};
    }
    return {.result = -EOPNOTSUPP, .async = false};
}