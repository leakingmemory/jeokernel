//
// Created by sigsegv on 9/23/22.
//

#include "Ioctl.h"
#include "SyscallCtx.h"
#include <exec/procthread.h>
#include <errno.h>

class Ioctl_Call : public referrer {
private:
    std::weak_ptr<Ioctl_Call> selfRef{};
    Ioctl_Call() : referrer("Ioctl_Call") {}
public:
    static std::shared_ptr<Ioctl_Call> Create();
    std::string GetReferrerIdentifier() override;
    intptr_t Call(SyscallCtx &ctx, int fd, intptr_t cmd, intptr_t arg);
};

std::shared_ptr<Ioctl_Call> Ioctl_Call::Create() {
    std::shared_ptr<Ioctl_Call> ioctlCall{new Ioctl_Call()};
    std::weak_ptr<Ioctl_Call> weakPtr{ioctlCall};
    ioctlCall->selfRef = weakPtr;
    return ioctlCall;
}

std::string Ioctl_Call::GetReferrerIdentifier() {
    return "";
}

intptr_t Ioctl_Call::Call(SyscallCtx &ctx, int fd, intptr_t cmd, intptr_t arg) {
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    auto desc = ctx.GetProcess().get_file_descriptor(selfRef, fd);
    if (!desc) {
        return -EBADF;
    }
    return desc->ioctl(ctx, cmd, arg);
}

int64_t Ioctl::Call(int64_t fd, int64_t cmd, int64_t arg, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};
    return Ioctl_Call::Create()->Call(ctx, fd, cmd, arg);
}