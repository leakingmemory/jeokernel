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
    intptr_t write(const void *ptr, intptr_t len) override;
    bool stat(struct stat &st) override;
};


#endif //JEOKERNEL_STDINDESC_H
