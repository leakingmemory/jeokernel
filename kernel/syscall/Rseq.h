//
// Created by sigsegv on 8/12/22.
//

#ifndef JEOKERNEL_O_RSEQ_H
#define JEOKERNEL_O_RSEQ_H

#include "SyscallHandler.h"

class Rseq : public Syscall {
public:
    Rseq(SyscallHandler &handler) : Syscall(handler, 334) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_O_RSEQ_H
