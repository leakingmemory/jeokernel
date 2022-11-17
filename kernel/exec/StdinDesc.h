//
// Created by sigsegv on 6/19/22.
//

#ifndef JEOKERNEL_STDINDESC_H
#define JEOKERNEL_STDINDESC_H

#include <exec/fdesc.h>
#include <tty/tty.h>

class StdinDesc : public FileDescriptorHandler {
private:
    std::shared_ptr<class tty> tty;
    explicit StdinDesc(std::shared_ptr<class tty> tty);
public:
    ~StdinDesc();
    static FileDescriptor Descriptor(std::shared_ptr<class tty> tty);
    std::shared_ptr<FileDescriptorHandler> clone() override;
    std::shared_ptr<kfile> get_file() override;
    void Notify() override;
    bool can_read() override;
    resolve_return_value read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len) override;
    resolve_return_value read(std::shared_ptr<callctx> ctx, void *ptr, intptr_t len, uintptr_t offset) override;
    intptr_t write(const void *ptr, intptr_t len) override;
    bool stat(struct stat64 &st) override;
    intptr_t ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) override;
};


#endif //JEOKERNEL_STDINDESC_H
