//
// Created by sigsegv on 6/19/22.
//

#ifndef JEOKERNEL_STDINDESC_H
#define JEOKERNEL_STDINDESC_H

#include <exec/fdesc.h>

class StdinDesc : public FileDescriptorHandler {
private:
    StdinDesc() = default;
public:
    static FileDescriptor Descriptor();
    std::shared_ptr<kfile> get_file() override;
    bool can_read() override;
    intptr_t read(void *ptr, intptr_t len) override;
    intptr_t read(void *ptr, intptr_t len, uintptr_t offset) override;
    intptr_t write(const void *ptr, intptr_t len) override;
    bool stat(struct stat &st) override;
    intptr_t ioctl(callctx &ctx, intptr_t cmd, intptr_t arg) override;
};


#endif //JEOKERNEL_STDINDESC_H
