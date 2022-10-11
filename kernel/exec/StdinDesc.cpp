//
// Created by sigsegv on 6/19/22.
//

#include "StdinDesc.h"
#include <errno.h>
#include <iostream>
#include <exec/procthread.h>

FileDescriptor StdinDesc::Descriptor() {
    std::shared_ptr<StdinDesc> handler{new StdinDesc};
    FileDescriptor fd{handler, 0};
    return fd;
}

std::shared_ptr<FileDescriptorHandler> StdinDesc::clone() {
    return std::make_shared<StdinDesc>((const StdinDesc &) *this);
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

intptr_t StdinDesc::ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    if (process == nullptr) {
        return -EOPNOTSUPP;
    }
    auto tty = process->GetProcess()->GetTty();
    if (tty) {
        return tty->ioctl(ctx, cmd, arg);
    }
    return -EOPNOTSUPP;
}