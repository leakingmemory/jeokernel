//
// Created by sigsegv on 9/8/22.
//

#include "Access.h"
#include <exec/usermem.h>
#include <exec/procthread.h>
#include <kfs/kfiles.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include "SyscallCtx.h"

int Access::DoAccess(ProcThread &proc, std::string filename, int mode) {
    constexpr int allAccess = F_OK | X_OK | W_OK | R_OK;
    if ((mode & allAccess) != mode) {
        return -EINVAL;
    }
    auto fileResolve = proc.ResolveFile(filename);
    if (fileResolve.status != kfile_status::SUCCESS) {
        switch (fileResolve.status) {
            case kfile_status::IO_ERROR:
                return -EIO;
            case kfile_status::NOT_DIRECTORY:
                return -ENOTDIR;
            default:
                return -EIO;
        }
    }
    auto file = fileResolve.result;
    if (!file) {
        return -ENOENT;
    }
    int m = file->Mode();
    if ((m & mode) == mode) {
        return 0;
    }
    mode = mode & ~m;
    m = m >> 3;
    {
        auto gid = proc.getgid();
        auto f_gid = file->Gid();
        if (gid == f_gid) {
            if ((m & mode) == mode) {
                return 0;
            }
            mode = mode & ~m;
        }
    }
    m = m >> 3;
    {
        auto uid = proc.getuid();
        auto f_uid = file->Uid();
        if (uid == f_uid || uid == 0) {
            if ((m & mode) == mode) {
                return 0;
            }
        }
    }
    return -EACCES;
}

int64_t Access::Call(int64_t uptr_filename, int64_t mode, int64_t, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};

    return ctx.ReadString(uptr_filename, [this, ctx, mode] (const std::string &u_filename) {
        std::string filename{u_filename};

        Queue([this, ctx, filename, mode] () mutable {
            auto res = DoAccess(ctx.GetProcess(), filename, mode);
            std::cout << "access(" << filename << ", " << std::hex << mode << std::dec << ") => " << res << "\n";
            ctx.ReturnWhenNotRunning(res);
        });
        return resolve_return_value::AsyncReturn();
    });
}