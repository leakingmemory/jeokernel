//
// Created by sigsegv on 10/5/22.
//

#ifndef JEOKERNEL_SYSINFO_CALL_H
#define JEOKERNEL_SYSINFO_CALL_H

#include "SyscallHandler.h"

class Sysinfo : public Syscall {
public:
    Sysinfo(SyscallHandler &handler) : Syscall(handler, 99) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_SYSINFO_CALL_H
