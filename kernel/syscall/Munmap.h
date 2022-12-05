//
// Created by sigsegv on 12/5/22.
//

#ifndef JEOKERNEL_MUNMAP_H
#define JEOKERNEL_MUNMAP_H

#include "SyscallHandler.h"

class Munmap : Syscall {
public:
    Munmap(SyscallHandler &handler) : Syscall(handler, 11) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_MUNMAP_H
