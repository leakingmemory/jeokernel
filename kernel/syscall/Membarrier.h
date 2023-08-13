//
// Created by sigsegv on 8/13/23.
//

#ifndef JEOKERNEL_MEMBARRIER_H
#define JEOKERNEL_MEMBARRIER_H

#include "SyscallHandler.h"
#include "impl/SysMembarrierImpl.h"

class Membarrier : public Syscall, private SysMembarrierImpl {
public:
    Membarrier(SyscallHandler &handler) : Syscall(handler, 324) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_MEMBARRIER_H
