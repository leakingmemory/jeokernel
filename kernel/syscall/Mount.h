//
// Created by sigsegv on 3/8/23.
//

#ifndef JEOKERNEL_MOUNT_H
#define JEOKERNEL_MOUNT_H

#include "SyscallHandler.h"
#include "SyscallAsyncThread.h"
#include <string>

class SyscallCtx;
class resolve_return_value;

class Mount : public Syscall, private SyscallAsyncThread {
public:
    Mount(SyscallHandler &handler) : Syscall(handler, 165), SyscallAsyncThread("[mount]") {}
private:
    resolve_return_value DoMount(SyscallCtx &ctx, const std::string &dev, const std::string &dir, const std::string &type, unsigned long flags, const std::string &opts);
public:
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_MOUNT_H
