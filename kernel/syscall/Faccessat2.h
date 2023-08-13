//
// Created by sigsegv on 8/13/23.
//

#ifndef JEOKERNEL_FACCESSAT2_H
#define JEOKERNEL_FACCESSAT2_H

#include "SyscallHandler.h"
#include "impl/SysFaccessatImpl.h"

class Faccessat2 : public Syscall, private SysFaccessatImpl {
public:
    Faccessat2(SyscallHandler &handler) : Syscall(handler, 439) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_FACCESSAT2_H
