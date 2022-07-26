//
// Created by sigsegv on 7/16/22.
//

#ifndef JEOKERNEL_MMAP_H
#define JEOKERNEL_MMAP_H

#include "SyscallHandler.h"

class Mmap : public Syscall {
public:
    Mmap(SyscallHandler &handler) : Syscall(handler, 9) {}
    int64_t Call(int64_t addr, int64_t len, int64_t prot, int64_t flags, SyscallAdditionalParams &params) override;
};


#endif //JEOKERNEL_MMAP_H
