//
// Created by sigsegv on 10/7/22.
//

#include "Getcwd.h"
#include "SyscallCtx.h"
#include <exec/procthread.h>
#include <errno.h>
#include <iostream>

int Getcwd::DoGetcwd(ProcThread &process, char *buf, intptr_t len) {
    std::shared_ptr<kfile> cwd_ref = process.GetCwd();
    if (!cwd_ref) {
        return -ENOENT;
    }
    kdirectory *dir = dynamic_cast<kdirectory*>(&(*cwd_ref));
    if (dir == nullptr) {
        return -ENOTDIR;
    }
    std::string path{dir->Kpath()};
    if (path.size() >= len) {
        return -ERANGE;
    }
    std::cout << "getcwd returns \"" << path << "\"\n";
    memcpy(buf, path.c_str(), path.size() + 1);
    return 0;
}

int64_t Getcwd::Call(int64_t uptr_buf, int64_t len, int64_t, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};
    if (uptr_buf == 0) {
        return -EINVAL;
    }
    return ctx.Write(uptr_buf, len, [this, ctx, uptr_buf, len] (void *ptr) {
        auto result = DoGetcwd(ctx.GetProcess(), (char *) ptr, len);
        if (result == 0) {
            return ctx.Return(uptr_buf);
        }
        return ctx.Return(result);
    });
}
