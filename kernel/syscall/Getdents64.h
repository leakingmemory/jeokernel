//
// Created by sigsegv on 11/18/22.
//

#ifndef JEOKERNEL_GETDENTS64_H
#define JEOKERNEL_GETDENTS64_H

#include "SyscallHandler.h"
#include "SyscallAsyncThread.h"

class FileDescriptor;

class Getdents64 : public Syscall, private SyscallAsyncThread {
public:
    Getdents64(SyscallHandler &handler) : Syscall(handler, 217), SyscallAsyncThread("[dents64]") {}
    int GetDents64(const FileDescriptor &desc, void *dirents, size_t count);
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_GETDENTS64_H
