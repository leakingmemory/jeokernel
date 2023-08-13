//
// Created by sigsegv on 8/12/23.
//

#include <exec/procthread.h>
#include <errno.h>
#include "Readv.h"
#include "SyscallCtx.h"

int64_t Readv::Call(int64_t fd, int64_t user_iov_ptr, int64_t iovcnt, int64_t, SyscallAdditionalParams &params) {
    std::shared_ptr<callctx> ctx = std::make_shared<SyscallCtx>(params);
    auto desc = ctx->GetProcess().get_file_descriptor(fd);
    if (!desc.Valid()) {
        return -EBADF;
    }
    return desc.readv(ctx, user_iov_ptr, iovcnt);
}