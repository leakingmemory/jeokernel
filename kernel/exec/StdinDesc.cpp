//
// Created by sigsegv on 6/19/22.
//

#include "StdinDesc.h"
#include <errno.h>
#include <iostream>
#include <exec/procthread.h>
#include <fcntl.h>

//#define STDIN_READ_DEBUG

StdinDesc::StdinDesc(std::shared_ptr<class tty> tty) : tty(tty) {
}

StdinDesc::~StdinDesc() noexcept {
    tty->Unsubscribe(this);
}

std::shared_ptr<FileDescriptor> StdinDesc::Descriptor(std::shared_ptr<class tty> tty) {
    std::shared_ptr<StdinDesc> handler{new StdinDesc(tty)};
    {
        std::shared_ptr<FileDescriptorHandler> fdhandler{handler};
        tty->Subscribe(fdhandler);
    }
    std::shared_ptr<FileDescriptor> fd = FileDescriptor::Create(handler, 0, O_RDONLY);
    return fd;
}

std::shared_ptr<FileDescriptorHandler> StdinDesc::clone() {
    return std::make_shared<StdinDesc>((const StdinDesc &) *this);
}

reference<kfile> StdinDesc::get_file(std::shared_ptr<class referrer> &referrer) {
    return {};
}

void StdinDesc::Notify() {
    SetReadyRead(tty->HasInput());
}

bool StdinDesc::can_seek() {
    return false;
}

bool StdinDesc::can_read() {
    return true;
}

intptr_t StdinDesc::seek(intptr_t offset, SeekWhence whence) {
    return -EINVAL;
}

resolve_return_value StdinDesc::read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len) {
#ifdef STDIN_READ_DEBUG
    std::cout << "stdin read(ptr, " << std::dec << len << ")\n";
#endif
    auto result = tty->Read(ptr, len);
    Notify();
    return resolve_return_value::Return(result);
}

resolve_return_value StdinDesc::read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len, uintptr_t offset) {
#ifdef STDIN_READ_DEBUG
    std::cout << "stdin read(ptr, " << std::dec << len <<", offset:" << offset << ")\n";
#endif
    return resolve_return_value::Return(-EOPNOTSUPP);
}

intptr_t StdinDesc::write(const void *ptr, intptr_t len) {
    return 0;
}

bool StdinDesc::stat(struct stat64 &st) const {
    struct stat64 s{.st_mode = S_IFCHR | 00666, .st_blksize = 1024};
    st = s;
    return true;
}

bool StdinDesc::stat(struct statx &st) const {
    struct statx s{.stx_blksize = 1024, .stx_mode = S_IFCHR | 00666};
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

int StdinDesc::readdir(const std::function<bool (kdirent &dirent)> &) {
    return -ENOTDIR;
}