//
// Created by sigsegv on 2/25/23.
//

#include "Fadvise64.h"
#include "SyscallCtx.h"
#include <exec/procthread.h>
#include <errno.h>

int64_t Fadvise64::Call(int64_t i_fd, int64_t offset, int64_t len, int64_t advice, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};
    int32_t fd = (int32_t) (i_fd & 0x0ffffffff);
    auto fdesc = ctx.GetProcess().has_file_descriptor(fd);
    if (!fdesc) {
        return EBADF;
    }
    return 0;
}
