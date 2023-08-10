//
// Created by sigsegv on 8/10/23.
//

#include <sys/stat.h>
#include <exec/procthread.h>
#include <errno.h>
#include <exec/files.h>
#include "Stat.h"
#include "SyscallCtx.h"

int64_t Stat::Call(int64_t uptr_filename, int64_t uptr_statbuf, int64_t, int64_t, SyscallAdditionalParams &handler) {
    SyscallCtx ctx{handler};
    auto task_id = get_scheduler()->get_current_task_id();

    return ctx.ReadString(uptr_filename, [this, ctx, uptr_statbuf, task_id] (const std::string &inFilename) {
        std::string filename{inFilename};
        return ctx.NestedWrite(uptr_statbuf, sizeof(struct stat), [this, ctx, filename, task_id] (void *ptr) {
            auto *stbuf = (struct stat *) ptr;
            Queue(task_id, [ctx, filename, stbuf] () {
                auto fileResolve = ctx.GetProcess().ResolveFile(filename);
                if (fileResolve.status != kfile_status::SUCCESS) {
                    int err{-EIO};
                    switch (fileResolve.status) {
                        case kfile_status::IO_ERROR:
                            err = -EIO;
                            break;
                        case kfile_status::NOT_DIRECTORY:
                            err = -ENOTDIR;
                            break;
                        default:
                            err = -EIO;
                    }
                    ctx.ReturnWhenNotRunning(err);
                    return;
                }
                auto file = fileResolve.result;
                if (!file) {
                    ctx.ReturnWhenNotRunning(-ENOENT);
                    return;
                }
                struct stat st;
                FsStat::Stat(*file, st);
                memcpy(stbuf, &st, sizeof(st));
                ctx.ReturnWhenNotRunning(0);
            });
            return ctx.Async();
        });
    });
}