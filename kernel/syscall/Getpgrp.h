//
// Created by sigsegv on 10/10/22.
//

#ifndef JEOKERNEL_GETPGRP_H
#define JEOKERNEL_GETPGRP_H

#include "SyscallHandler.h"

class Getpgrp : public Syscall {
public:
    Getpgrp(SyscallHandler &handler) : Syscall(handler, 111) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_GETPGRP_H
