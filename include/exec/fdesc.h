//
// Created by sigsegv on 6/18/22.
//

#ifndef JEOKERNEL_FDESC_H
#define JEOKERNEL_FDESC_H

#include <cstdint>
#include <memory>
#include <functional>
#include <sys/stat.h>

class ProcThread;

class FileDescriptorHandler {
public:
    virtual ~FileDescriptorHandler() = default;
    virtual intptr_t write(const void *ptr, intptr_t len) = 0;
    virtual bool stat(struct stat &st) = 0;
};

class FileDescriptor {
private:
    std::shared_ptr<FileDescriptorHandler> handler;
    int fd;
public:
    FileDescriptor() : handler(), fd(0) {
    }
    FileDescriptor(std::shared_ptr<FileDescriptorHandler> handler, int fd) : handler(handler), fd(fd) {
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
    void write(ProcThread *process, uintptr_t usersp_ptr, intptr_t len, std::function<void (intptr_t)> func);
    void writev(ProcThread *process, uintptr_t usersp_iov_ptr, int iovcnt, std::function<void (intptr_t)> func);
    bool stat(struct stat &st);
};

#endif //JEOKERNEL_FDESC_H
