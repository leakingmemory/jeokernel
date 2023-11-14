//
// Created by sigsegv on 3/8/23.
//

#ifndef JEOKERNEL_MOUNT_H
#define JEOKERNEL_MOUNT_H

#include "SyscallHandler.h"
#include <string>

class SyscallCtx;
class resolve_return_value;

class Mount : public Syscall {
public:
    Mount(SyscallHandler &handler) : Syscall(handler, 165) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_MOUNT_H
