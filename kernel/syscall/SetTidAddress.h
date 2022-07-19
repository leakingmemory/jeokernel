//
// Created by sigsegv on 7/17/22.
//

#ifndef JEOKERNEL_SETTIDADDRESS_H
#define JEOKERNEL_SETTIDADDRESS_H

#include "SyscallHandler.h"

class SetTidAddress : public Syscall {
public:
    SetTidAddress(SyscallHandler &handler) : Syscall(handler, 218) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_SETTIDADDRESS_H
