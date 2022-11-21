//
// Created by sigsegv on 6/18/22.
//

#ifndef JEOKERNEL_FDESC_H
#define JEOKERNEL_FDESC_H

#include <cstdint>
#include <memory>
#include <functional>
#include <sys/stat.h>
#include <exec/fdselect.h>
#include <concurrency/hw_spinlock.h>
#include <vector>
#include <exec/resolve_return.h>

class callctx;
class ProcThread;
class kfile;
class kdirent;

struct file_descriptor_result {
    intptr_t result;
    bool async;
};

struct FdSubscription {
    Select impl;
    int fd;
};

class FileDescriptorHandler {
private:
    hw_spinlock mtx;
    std::vector<FdSubscription> subscriptions;
    bool readyRead;
public:
    FileDescriptorHandler();
    virtual ~FileDescriptorHandler() = default;
protected:
    FileDescriptorHandler(const FileDescriptorHandler &cp);
public:
    void Subscribe(int fd, Select select);
    void SetReadyRead(bool ready);
    virtual void Notify();
    virtual std::shared_ptr<FileDescriptorHandler> clone() = 0;
    virtual std::shared_ptr<kfile> get_file() = 0;
    virtual bool can_read() = 0;
    virtual resolve_return_value read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len) = 0;
    virtual resolve_return_value read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len, uintptr_t offset) = 0;
    virtual intptr_t write(const void *ptr, intptr_t len) = 0;
    virtual bool stat(struct stat64 &st) = 0;
    virtual intptr_t ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) = 0;
    virtual int readdir(const std::function<bool (kdirent &dirent)> &) = 0;
};

class FileDescriptor {
private:
    std::shared_ptr<FileDescriptorHandler> handler;
    int fd;
    int openFlags;
public:
    FileDescriptor() : handler(), fd(0), openFlags(0) {
    }
    FileDescriptor(const std::shared_ptr<FileDescriptorHandler> &handler, int fd, int openFlags) : handler(handler), fd(fd), openFlags(openFlags) {
    }
    virtual ~FileDescriptor() = default;
    int FD() {
        return fd;
    }
    bool Valid() {
        if (handler) {
            return true;
        } else {
            return false;
        }
    }
    std::shared_ptr<FileDescriptorHandler> GetHandler() {
        return handler;
    }
    std::shared_ptr<kfile> get_file() const;
    bool can_read();
    resolve_return_value read(std::shared_ptr<callctx> ctx, void *, intptr_t len);
    resolve_return_value read(std::shared_ptr<callctx> ctx, void *, intptr_t len, uintptr_t offset);
    file_descriptor_result write(ProcThread *process, uintptr_t usersp_ptr, intptr_t len, std::function<void (intptr_t)> func);
    file_descriptor_result writev(ProcThread *process, uintptr_t usersp_iov_ptr, int iovcnt, std::function<void (intptr_t)> func);
    bool stat(struct stat &st);
    intptr_t ioctl(callctx &ctx, intptr_t cmd, intptr_t arg);
    int get_open_flags() {
        return openFlags;
    }
    void set_open_flags(int flags) {
        openFlags = flags;
    }
    int readdir(const std::function<bool (kdirent &dirent)> &) const;
};

#endif //JEOKERNEL_FDESC_H
