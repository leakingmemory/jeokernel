//
// Created by sigsegv on 6/18/22.
//

#ifndef JEOKERNEL_FDESC_H
#define JEOKERNEL_FDESC_H

#include <cstdint>
#include <memory>
#include <functional>
#include <sys/stat.h>

class callctx;
class ProcThread;
class kfile;

struct file_descriptor_result {
    intptr_t result;
    bool async;
};

class FileDescriptorHandler {
public:
    virtual ~FileDescriptorHandler() = default;
    virtual std::shared_ptr<FileDescriptorHandler> clone() = 0;
    virtual std::shared_ptr<kfile> get_file() = 0;
    virtual bool can_read() = 0;
    virtual intptr_t read(void *ptr, intptr_t len) = 0;
    virtual intptr_t read(void *ptr, intptr_t len, uintptr_t offset) = 0;
    virtual intptr_t write(const void *ptr, intptr_t len) = 0;
    virtual bool stat(struct stat &st) = 0;
    virtual intptr_t ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) = 0;
};

class FileDescriptor {
private:
    std::shared_ptr<FileDescriptorHandler> handler;
    int fd;
public:
    FileDescriptor() : handler(), fd(0) {
    }
    FileDescriptor(const std::shared_ptr<FileDescriptorHandler> &handler, int fd) : handler(handler), fd(fd) {
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
    std::shared_ptr<kfile> get_file();
    bool can_read();
    int read(void *, intptr_t len);
    int read(void *, intptr_t len, uintptr_t offset);
    file_descriptor_result write(ProcThread *process, uintptr_t usersp_ptr, intptr_t len, std::function<void (intptr_t)> func);
    file_descriptor_result writev(ProcThread *process, uintptr_t usersp_iov_ptr, int iovcnt, std::function<void (intptr_t)> func);
    bool stat(struct stat &st);
    intptr_t ioctl(callctx &ctx, intptr_t cmd, intptr_t arg);
};

#endif //JEOKERNEL_FDESC_H
