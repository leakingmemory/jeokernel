//
// Created by sigsegv on 9/12/22.
//

#ifndef JEOKERNEL_FILES_H
#define JEOKERNEL_FILES_H

#include <exec/fdesc.h>

class kfile;

class FsFileDescriptorHandler : public FileDescriptorHandler {
private:
    std::shared_ptr<kfile> file;
public:
    FsFileDescriptorHandler(const std::shared_ptr<kfile> &file) : FileDescriptorHandler(), file(file) {}
    intptr_t write(const void *ptr, intptr_t len) override;
    bool stat(struct stat &st) override;
};

#endif //JEOKERNEL_FILES_H
