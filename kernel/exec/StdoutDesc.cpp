//
// Created by sigsegv on 6/19/22.
//

#include "StdoutDesc.h"
#include <string>
#include <klogger.h>
#include <errno.h>
#include <iostream>
#include <exec/procthread.h>
#include <fcntl.h>

reference<FileDescriptor> StdoutDesc::StdoutDescriptor(const std::shared_ptr<class referrer> &referrer) {
    std::shared_ptr<StdoutDesc> desc{new StdoutDesc};
    desc->SetSelfRef(desc);
    return FileDescriptor::CreateFromPointer(referrer, desc, 1, O_WRONLY);
}

reference<FileDescriptor> StdoutDesc::StderrDescriptor(const std::shared_ptr<class referrer> &referrer) {
    std::shared_ptr<StdoutDesc> desc{new StdoutDesc};
    desc->SetSelfRef(desc);
    return FileDescriptor::CreateFromPointer(referrer, desc, 2, O_WRONLY);
}

reference<FileDescriptorHandler> StdoutDesc::clone(const std::shared_ptr<class referrer> &referrer) {
    auto newDesc = std::make_shared<StdoutDesc>((const StdoutDesc &) *this);
    newDesc->SetSelfRef(newDesc);
    return newDesc->CreateReference(referrer);
}

reference<kfile> StdoutDesc::get_file(std::shared_ptr<class referrer> &referrer) {
    return {};
}

bool StdoutDesc::can_seek() {
    return false;
}

bool StdoutDesc::can_read() {
    return false;
}

intptr_t StdoutDesc::seek(intptr_t offset, SeekWhence whence) {
    return -EINVAL;
}

resolve_return_value StdoutDesc::read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len) {
    return resolve_return_value::Return(-EIO);
}

resolve_return_value StdoutDesc::read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len, uintptr_t offset) {
    return resolve_return_value::Return(-EIO);
}

intptr_t StdoutDesc::write(const void *ptr, intptr_t len) {
    if (len > 0) {
        std::string str{(const char *) ptr, (std::size_t) len};
        get_klogger() << str.c_str();
        return len;
    }
    return 0;
}

bool StdoutDesc::stat(struct stat64 &st) const {
    struct stat64 s{.st_mode = S_IFCHR | 00666, .st_blksize = 1024};
    st = s;
    return true;
}

bool StdoutDesc::stat(struct statx &st) const {
    struct statx s{.stx_blksize = 1024, .stx_mode = S_IFCHR | 00666};
    st = s;
    return true;
}

intptr_t StdoutDesc::ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) {
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

int StdoutDesc::readdir(const std::function<bool (kdirent &dirent)> &) {
    return -ENOTDIR;
}