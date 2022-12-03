//
// Created by sigsegv on 8/10/22.
//

#include <exec/procthread.h>
#include <exec/usermem.h>
#include <exec/files.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <iostream>
#include "Newfstatat.h"
#include "SyscallCtx.h"

//#define DEBUG_NEWFSTATAT

int64_t Newfstatat::Call(int64_t dfd, int64_t uptr_filename, int64_t uptr_statbuf, int64_t flag, SyscallAdditionalParams &params) {
    if (uptr_filename == 0) {
        return -EINVAL;
    }

    constexpr int suppFlags = AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW | AT_SYMLINK_FOLLOW | AT_NO_AUTOMOUNT;
    constexpr int nonsuppFlags = ~suppFlags;

    if ((flag & nonsuppFlags) != 0) {
        return -EINVAL;
    }

    SyscallCtx ctx{params};

    return ctx.ReadString(uptr_filename, [this, ctx, uptr_statbuf, dfd, flag] (const std::string &u_filename) {
        std::string filename{u_filename};
        return ctx.NestedWrite(uptr_statbuf, sizeof(struct stat), [this, ctx, filename, dfd, flag] (void *statbuf) {
            Queue([ctx, statbuf, filename, dfd, flag] () mutable {
                struct stat st{};

                if ((!filename.empty() && filename.starts_with("/")) || dfd == AT_FDCWD) {
#ifdef DEBUG_NEWFSTATAT
                    std::cout << "newfstatat(" << std::dec << dfd << ", \"" << filename << "\", " << std::hex
                              << "statbuf, " << flag << std::dec << ")\n";
#endif
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
                    FsStat::Stat(*file, st);
                } else {
                    FileDescriptor fd{ctx.GetProcess().get_file_descriptor(dfd)};
                    if (!fd.Valid()) {
                        ctx.ReturnWhenNotRunning(-EBADF);
                        return;
                    }
                    if (filename.empty()) {
                        if ((flag & AT_EMPTY_PATH) != AT_EMPTY_PATH) {
                            ctx.ReturnWhenNotRunning(-EINVAL);
                            return;
                        }
                        if (!fd.stat(st)) {
                            ctx.ReturnWhenNotRunning(-EBADF);
                            return;
                        }
                        memcpy(statbuf, &st, sizeof(st));
                        ctx.ReturnWhenNotRunning(0);
                        return;
                    }
                    // TODO
                    std::cout << "newfstatat(" << std::dec << dfd << ", \"" << filename << "\", " << std::hex
                              << "statbuf, " << flag << std::dec << ")\n";
                    asm("ud2");
                }
                memcpy(statbuf, &st, sizeof(st));
                ctx.ReturnWhenNotRunning(0);
            });
            return resolve_return_value::AsyncReturn();
        });
    });
}