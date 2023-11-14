//
// Created by sigsegv on 8/22/23.
//

#ifndef JEOKERNEL_PIPE_H
#define JEOKERNEL_PIPE_H

#include "fdesc.h"
#include <memory>
#include <string>
#include <concurrency/hw_spinlock.h>

class PipeDescriptorHandler;

struct PipeQueuedRead {
    std::function<void ()> func;
    intptr_t len;
};

class PipeEnd {
    friend PipeDescriptorHandler;
private:
    hw_spinlock mtx;
    std::weak_ptr<PipeEnd> otherEnd;
    std::vector<std::weak_ptr<PipeDescriptorHandler>> fds{};
    std::vector<PipeQueuedRead> readQueue{};
    std::string buffer{};
    bool eof{false};
public:
    PipeEnd() : otherEnd() {}
    PipeEnd(const std::shared_ptr<PipeEnd> &otherEnd) : otherEnd(otherEnd) {}
    ~PipeEnd();
    void AddFd(const std::shared_ptr<PipeDescriptorHandler> &fd);
    void Notify();
};

class PipeDescriptorHandler : public FileDescriptorHandler {
private:
    std::shared_ptr<PipeEnd> end;
    std::weak_ptr<PipeDescriptorHandler> self_ref;
private:
    PipeDescriptorHandler(const std::shared_ptr<PipeEnd> &end) : end(end) {}
public:
    static std::shared_ptr<PipeDescriptorHandler> CreateHandler(const std::shared_ptr<PipeEnd> &end) {
        std::shared_ptr<PipeDescriptorHandler> handler{new PipeDescriptorHandler(end)};
        end->AddFd(handler);
        std::weak_ptr<PipeDescriptorHandler> weakPtr{handler};
        handler->self_ref = weakPtr;
        return handler;
    }
    static void CreateHandlerPair(std::shared_ptr<PipeDescriptorHandler> &a, std::shared_ptr<PipeDescriptorHandler> &b);
    void Notify() override;
    std::shared_ptr<FileDescriptorHandler> clone() override;
    reference<kfile> get_file(std::shared_ptr<class referrer> &referrer) override;
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

#endif //JEOKERNEL_PIPE_H
