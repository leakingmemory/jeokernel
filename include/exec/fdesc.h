//
// Created by sigsegv on 6/18/22.
//

#ifndef JEOKERNEL_FDESC_H
#define JEOKERNEL_FDESC_H

#include <cstdint>
#include <memory>
#include <functional>

class Process;

class FileDescriptorHandler {
public:
    virtual intptr_t write(const void *ptr, intptr_t len) = 0;
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
    ~FileDescriptor() = default;
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
    void write(Process *process, uintptr_t usersp_ptr, intptr_t len, std::function<void (intptr_t)> func);
};

#endif //JEOKERNEL_FDESC_H
