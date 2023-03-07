//
// Created by sigsegv on 3/6/23.
//

#include "Mkdir.h"
#include "SyscallCtx.h"
#include <errno.h>
#include <iostream>

int64_t Mkdir::Call(int64_t uptr_pathname, int64_t u_mode, int64_t, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};
    int32_t mode = (int32_t) u_mode;
    return ctx.ReadString(uptr_pathname, [ctx, mode] (const std::string &pathname) {
        std::cout << "mkdir(" << pathname << ", " << std::oct << mode << std::dec << ")\n";
        return ctx.Return(-EOPNOTSUPP);
    });
}