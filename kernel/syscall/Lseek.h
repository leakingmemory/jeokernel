//
// Created by sigsegv on 3/1/23.
//

#ifndef JEOKERNEL_LSEEK_H
#define JEOKERNEL_LSEEK_H

#include "SyscallHandler.h"

class Lseek : public Syscall {
public:
    Lseek(SyscallHandler &handler) : Syscall(handler, 8) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_LSEEK_H
