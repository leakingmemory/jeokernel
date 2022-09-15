//
// Created by sigsegv on 9/14/22.
//

#ifndef JEOKERNEL_READ_H
#define JEOKERNEL_READ_H

#include "SyscallHandler.h"
#include "SyscallAsyncThread.h"

class Read : public Syscall, private SyscallAsyncThread {
public:
    Read(SyscallHandler &handler) : Syscall(handler, 0), SyscallAsyncThread("[fread]") {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_READ_H
