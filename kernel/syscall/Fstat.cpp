//
// Created by sigsegv on 2/24/23.
//

#include "Fstat.h"
#include "SyscallCtx.h"
#include <sys/stat.h>
#include <exec/procthread.h>
#include <errno.h>

int64_t Fstat::Call(int64_t i_fd, int64_t uptr_statbuf, int64_t, int64_t, SyscallAdditionalParams &params) {
    uint32_t fd = (uint32_t) i_fd;
    SyscallCtx ctx{params};
    auto fdesc = ctx.GetProcess().get_file_descriptor(fd);
    if (!fdesc) {
        return -EBADF;
    }
    return ctx.Write(uptr_statbuf, sizeof(struct stat), [ctx, fdesc] (void *statbuf) {
        fdesc->stat(*((struct stat *) statbuf));
        return ctx.Return(0);
    });
}