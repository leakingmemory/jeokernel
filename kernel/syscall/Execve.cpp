//
// Created by sigsegv on 11/12/22.
//

#include "Execve.h"
#include "SyscallCtx.h"
#include <errno.h>
#include <string>
#include <iostream>

int64_t Execve::Call(int64_t uptr_filename, int64_t uptr_argv, int64_t uptr_envp, int64_t flags, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};
    return ctx.ReadString(uptr_filename, [] (const std::string &rd_filename) {
        std::string filename{rd_filename};
        std::cout << "execve(" << filename << ", ...)\n";
        return resolve_return_value::Return(-EOPNOTSUPP);
    });
}