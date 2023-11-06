//
// Created by sigsegv on 9/12/22.
//

#ifndef JEOKERNEL_FILES_H
#define JEOKERNEL_FILES_H

#include <exec/fdesc.h>
#include "concurrency/hw_spinlock.h"
#include "kfs/kfiles.h"

class kfile;

class FsStat {
public:
    static void Stat(const kfile &file, struct stat64 &st);
    static void Stat(const kfile &file, statx &st);
};

class FsFileDescriptorHandler : public FileDescriptorHandler {
private:
    std::shared_ptr<kfile> file;
    size_t offset;
    bool openRead;
    bool openWrite;
    bool nonblock;
public:
    FsFileDescriptorHandler(const std::shared_ptr<kfile> &file, bool openRead, bool openWrite, bool nonblock) : FileDescriptorHandler(), file(file), offset(0), openRead(openRead), openWrite(openWrite), nonblock(nonblock) {}
    FsFileDescriptorHandler(const FsFileDescriptorHandler &cp);
    FsFileDescriptorHandler(FsFileDescriptorHandler &&) = delete;
    FsFileDescriptorHandler &operator = (const FsFileDescriptorHandler &) = delete;
    FsFileDescriptorHandler &operator = (FsFileDescriptorHandler &&) = delete;
    std::shared_ptr<FileDescriptorHandler> clone() override;
    std::shared_ptr<kfile> get_file() override;
    bool can_seek() override;
    bool can_read() override;
    intptr_t seek(intptr_t offset, SeekWhence whence) override;
    resolve_return_value read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len) override;
    resolve_return_value read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len, uintptr_t offset) override;
    intptr_t write(const void *ptr, intptr_t len) override;
    bool stat(struct stat64 &st) const override;
    bool stat(struct statx &st) const override;
    intptr_t ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) override;
    int readdir(const std::function<bool (kdirent &dirent)> &) override;
};

class FsDirectoryDescriptorHandler : public FileDescriptorHandler {
private:
    std::shared_ptr<kdirectory> dir;
    bool readdirInited;
    std::vector<std::shared_ptr<kdirent>> dirents;
    std::vector<std::shared_ptr<kdirent>>::iterator iterator;
public:
    FsDirectoryDescriptorHandler(const std::shared_ptr<kdirectory> &dir) : FileDescriptorHandler(), dir(dir), readdirInited(false), dirents(), iterator() {}
    FsDirectoryDescriptorHandler(const FsDirectoryDescriptorHandler &);
    std::shared_ptr<FileDescriptorHandler> clone() override;
    std::shared_ptr<kfile> get_file() override;
    bool can_seek() override;
    bool can_read() override;
    intptr_t seek(intptr_t offset, SeekWhence whence) override;
    resolve_return_value read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len) override;
    resolve_return_value read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len, uintptr_t offset) override;
    intptr_t write(const void *ptr, intptr_t len) override;
    bool stat(struct stat64 &st) const override;
    bool stat(struct statx &st) const override;
    intptr_t ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) override;
    int readdir(const std::function<bool (kdirent &dirent)> &) override;
};

#endif //JEOKERNEL_FILES_H
