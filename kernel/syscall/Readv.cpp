//
// Created by sigsegv on 8/12/23.
//

#include <exec/procthread.h>
#include <errno.h>
#include "Readv.h"
#include "SyscallCtx.h"

class Readv_Call : public referrer {
private:
    std::weak_ptr<Readv_Call> selfRef{};
    Readv_Call() : referrer("Readv_Call") {}
public:
    static std::shared_ptr<Readv_Call> Create();
    std::string GetReferrerIdentifier() override;
    intptr_t Call(const std::shared_ptr<callctx> &ctx, int fd, intptr_t user_iov_ptr, intptr_t iovcnt);
};

std::shared_ptr<Readv_Call> Readv_Call::Create() {
    std::shared_ptr<Readv_Call> readvCall{new Readv_Call()};
    std::weak_ptr<Readv_Call> weakPtr{readvCall};
    readvCall->selfRef = weakPtr;
    return readvCall;
}

std::string Readv_Call::GetReferrerIdentifier() {
    return "";
}

intptr_t Readv_Call::Call(const std::shared_ptr<callctx> &ctx, int fd, intptr_t user_iov_ptr, intptr_t iovcnt) {
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    auto desc = ctx->GetProcess().get_file_descriptor(selfRef, fd);
    if (!desc) {
        return -EBADF;
    }
    return desc->readv(ctx, user_iov_ptr, iovcnt);
}

int64_t Readv::Call(int64_t fd, int64_t user_iov_ptr, int64_t iovcnt, int64_t, SyscallAdditionalParams &params) {
    std::shared_ptr<callctx> ctx = std::make_shared<SyscallCtx>(params);
    return Readv_Call::Create()->Call(ctx, (int) fd, user_iov_ptr, iovcnt);
}
