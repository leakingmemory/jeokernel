//
// Created by sigsegv on 9/12/22.
//

#ifndef JEOKERNEL_FILES_H
#define JEOKERNEL_FILES_H

#include <exec/fdesc.h>
#include "concurrency/hw_spinlock.h"

class kfile;

class FsStat {
public:
    static void Stat(kfile &file, struct stat &st);
};

class FsFileDescriptorHandler : public FileDescriptorHandler {
private:
    hw_spinlock mtx;
    std::shared_ptr<kfile> file;
    size_t offset;
    bool openRead;
    bool openWrite;
    bool nonblock;
public:
    FsFileDescriptorHandler(const std::shared_ptr<kfile> &file, bool openRead, bool openWrite, bool nonblock) : FileDescriptorHandler(), mtx(), file(file), offset(0), openRead(openRead), openWrite(openWrite), nonblock(nonblock) {}
    FsFileDescriptorHandler(const FsFileDescriptorHandler &cp);
    std::shared_ptr<FileDescriptorHandler> clone() override;
    std::shared_ptr<kfile> get_file() override;
    bool can_read() override;
    resolve_return_value read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len) override;
    resolve_return_value read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len, uintptr_t offset) override;
    intptr_t write(const void *ptr, intptr_t len) override;
    bool stat(struct stat &st) override;
    intptr_t ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) override;
};

#endif //JEOKERNEL_FILES_H
