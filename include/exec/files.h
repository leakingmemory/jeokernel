//
// Created by sigsegv on 9/12/22.
//

#ifndef JEOKERNEL_FILES_H
#define JEOKERNEL_FILES_H

#include <exec/fdesc.h>

class kfile;

class FsStat {
public:
    static void Stat(kfile &file, struct stat &st);
};

class FsFileDescriptorHandler : public FileDescriptorHandler {
private:
    std::shared_ptr<kfile> file;
    size_t offset;
public:
    FsFileDescriptorHandler(const std::shared_ptr<kfile> &file) : FileDescriptorHandler(), file(file), offset(0) {}
    intptr_t read(void *ptr, intptr_t len) override;
    intptr_t read(void *ptr, intptr_t len, uintptr_t offset) override;
    intptr_t write(const void *ptr, intptr_t len) override;
    bool stat(struct stat &st) override;
};

#endif //JEOKERNEL_FILES_H
