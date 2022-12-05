//
// Created by sigsegv on 12/5/22.
//

#include "Chdir.h"
#include "SyscallCtx.h"
#include <exec/procthread.h>
#include <errno.h>

int Chdir::DoChdir(std::shared_ptr<Process> proc, const std::string &filename) {
    auto file = proc->ResolveFile(filename);
    switch (file.status) {
        case kfile_status::SUCCESS:
            break;
        case kfile_status::IO_ERROR:
            return -EIO;
        case kfile_status::NOT_DIRECTORY:
            return -ENOTDIR;
    }
    if (!file.result) {
        return -ENOENT;
    }
    std::shared_ptr<kdirectory> dir{file.result};
    if (!dir) {
        return -ENOTDIR;
    }
    proc->SetCwd(file.result);
    return 0;
}

int64_t Chdir::Call(int64_t uptr_filename, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};
    return ctx.ReadString(uptr_filename, [this, ctx] (const std::string &r_filename) {
        std::string filename{r_filename};
        Queue([this, ctx, filename] () {
            auto res = DoChdir(ctx.GetProcess().GetProcess(), filename);
            ctx.ReturnWhenNotRunning(res);
        });
        return ctx.Async();
    });
}