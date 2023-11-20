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
#include <resource/referrer.h>
#include <resource/reference.h>
#include <resource/resource.h>

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

template <class T> class reference;
class referrer;

enum class SeekWhence { SET, CUR, END };

class FileDescriptorHandler : public resource<FileDescriptorHandler> {
protected:
    hw_spinlock fdmtx;
private:
    std::vector<FdSubscription> subscriptions;
    bool readyRead;
public:
    FileDescriptorHandler();
protected:
    FileDescriptorHandler(const FileDescriptorHandler &cp);
    void CopyFrom(const FileDescriptorHandler &cp);
public:
    FileDescriptorHandler * GetResource() override;
    FileDescriptorHandler(FileDescriptorHandler &&) = delete;
    FileDescriptorHandler &operator =(const FileDescriptorHandler &) = delete;
    FileDescriptorHandler &operator =(FileDescriptorHandler &&) = delete;
    virtual ~FileDescriptorHandler() = default;
    void Subscribe(int fd, Select select);
    void SetReadyRead(bool ready);
    virtual void Notify();
    virtual reference<FileDescriptorHandler> clone(const std::shared_ptr<class referrer> &referrer) = 0;
    virtual reference<kfile> get_file(std::shared_ptr<class referrer> &referrer) = 0;
    virtual bool can_seek() = 0;
    virtual bool can_read() = 0;
    virtual intptr_t seek(intptr_t offset, SeekWhence whence) = 0;
    virtual resolve_return_value read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len) = 0;
    virtual resolve_return_value read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len, uintptr_t offset) = 0;
    virtual intptr_t write(const void *ptr, intptr_t len) = 0;
    virtual bool stat(struct stat64 &st) const = 0;
    virtual bool stat(struct statx &st) const = 0;
    virtual intptr_t ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) = 0;
    virtual int readdir(const std::function<bool (kdirent &dirent)> &) = 0;
};

class FileDescriptor : public resource<FileDescriptor>, public referrer {
private:
    std::weak_ptr<FileDescriptor> selfRef{};
    reference<FileDescriptorHandler> handler{};
    int fd;
    int openFlags;
private:
    FileDescriptor(int fd, int openFlags) : referrer("FileDescriptor"), fd(fd), openFlags(openFlags) {
    }
public:
    static reference<FileDescriptor> CreateFromPointer(const std::shared_ptr<class referrer> &referrer, const std::shared_ptr<FileDescriptorHandler> &handler, int fd, int openFlags);
    static reference<FileDescriptor> Create(const std::shared_ptr<class referrer> &referrer, const reference<FileDescriptorHandler> &handler, int fd, int openFlags);
    std::string GetReferrerIdentifier() override;
    FileDescriptor * GetResource() override;
    virtual ~FileDescriptor() = default;
    int FD() {
        return fd;
    }
    reference<FileDescriptorHandler> GetHandler(const std::shared_ptr<class referrer> &referrer) {
        return handler.CreateReference(referrer);
    }
    void Subscribe(int fd, Select select);
    reference<kfile> get_file(std::shared_ptr<class referrer> &referrer) const;
    bool can_seek();
    bool can_read();
    intptr_t seek(intptr_t offset, SeekWhence whence);
    resolve_return_value read(std::shared_ptr<callctx> ctx, void *, intptr_t len);
    resolve_return_value read(std::shared_ptr<callctx> ctx, void *, intptr_t len, uintptr_t offset);
    intptr_t readv(std::shared_ptr<callctx> ctx, uintptr_t usersp_iov_ptr, int iovcnt);
    file_descriptor_result write(ProcThread *process, uintptr_t usersp_ptr, intptr_t len, std::function<void (intptr_t)> func);
    file_descriptor_result writev(ProcThread *process, uintptr_t usersp_iov_ptr, int iovcnt, std::function<void (intptr_t)> func);
    bool stat(struct stat &st) const;
    bool stat(struct statx &st) const;
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
