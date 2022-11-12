//
// Created by sigsegv on 8/2/22.
//

#include <iostream>
#include <exec/procthread.h>
#include <exec/usermem.h>
#include <errno.h>
#include "Readlink.h"
#include "SyscallCtx.h"

int64_t Readlink::Call(int64_t uptr_path, int64_t uptr_buf, int64_t bufsize, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};
    return ctx.ReadString(uptr_path, [ctx, uptr_buf, bufsize] (const std::string &u_path) {
        std::string path{u_path};
        return ctx.NestedWrite(uptr_buf, bufsize, [ctx, path] (void *ptr) {
            char *buf = (char *) buf;

            // TODO - implement symlinks

            return resolve_return_value::Return(-ENOENT);
        });
    });
}
