//
// Created by sigsegv on 2/22/23.
//

#ifndef JEOKERNEL_GETXATTR_H
#define JEOKERNEL_GETXATTR_H

#include "SyscallHandler.h"

class Getxattr : public Syscall {
public:
    Getxattr(SyscallHandler &handler) : Syscall(handler, 191) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_GETXATTR_H
