//
// Created by sigsegv on 8/10/22.
//

#define KERNEL

#include <exec/procthread.h>
#include <exec/usermem.h>
#include <exec/files.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <resource/reference.h>
#include <resource/referrer.h>
#include <iostream>
#include "Statx.h"
#include "SyscallCtx.h"

//#define DEBUG_STATX

class Statx_Call : public referrer {
private:
    std::weak_ptr<Statx_Call> selfRef{};
    Statx_Call() : referrer("Statx_Call") {}
public:
    static std::shared_ptr<Statx_Call> Create();
    std::string GetReferrerIdentifier() override;
    void ResolveFromRoot(SyscallCtx &ctx, void *statbuf, struct statx &st, const std::string &filename);
};

std::shared_ptr<Statx_Call> Statx_Call::Create() {
    std::shared_ptr<Statx_Call> statx{new Statx_Call()};
    std::weak_ptr<Statx_Call> weakPtr{statx};
    statx->selfRef = weakPtr;
    return statx;
}

std::string Statx_Call::GetReferrerIdentifier() {
    return "";
}

void Statx_Call::ResolveFromRoot(SyscallCtx &ctx, void *statbuf, struct statx &st, const std::string &filename) {
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    auto fileResolve = ctx.GetProcess().ResolveFile(selfRef, filename);
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
    auto file = std::move(fileResolve.result);
    if (!file) {
        ctx.ReturnWhenNotRunning(-ENOENT);
        return;
    }
    FsStat::Stat(*file, st);
    memcpy(statbuf, &st, sizeof(st));
    ctx.ReturnWhenNotRunning(0);
}

int64_t Statx::Call(int64_t i_dfd, int64_t uptr_filename, int64_t flag, int64_t mask, SyscallAdditionalParams &params) {
    auto dfd = (int32_t) i_dfd;
    int64_t uptr_statbuf = params.Param5();
    if (uptr_filename == 0 || uptr_statbuf == 0) {
        return -EINVAL;
    }

    constexpr int suppFlags = AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW | AT_SYMLINK_FOLLOW | AT_NO_AUTOMOUNT;
    constexpr int nonsuppFlags = ~suppFlags;

    if ((flag & nonsuppFlags) != 0) {
        std::cout << "Statx: Unsuppoerted flags in 0x" << std::hex << flag << std::dec << "\n";
        return -EINVAL;
    }

    SyscallCtx ctx{params};

    auto task_id = get_scheduler()->get_current_task_id();

    return ctx.ReadString(uptr_filename, [this, ctx, task_id, uptr_statbuf, dfd, flag] (const std::string &u_filename) {
        std::string filename{u_filename};
        return ctx.NestedWrite(uptr_statbuf, sizeof(struct statx), [this, ctx, task_id, filename, dfd, flag] (void *statbuf) {
            Queue(task_id, [ctx, statbuf, filename, dfd, flag] () mutable {
                struct statx st{};

#ifdef DEBUG_STATX
                std::cout << "statx(" << std::dec << dfd << ", \"" << filename << "\", " << std::hex
                          << "statbuf, " << flag << std::dec << ")\n";
#endif
                if ((!filename.empty() && filename.starts_with("/")) || dfd == AT_FDCWD) {
                    Statx_Call::Create()->ResolveFromRoot(ctx, statbuf, st, filename);
                    return;
                } else {
                    std::shared_ptr<FileDescriptor> fd = ctx.GetProcess().get_file_descriptor(dfd);
                    if (!fd) {
                        ctx.ReturnWhenNotRunning(-EBADF);
                        return;
                    }
                    if (filename.empty()) {
                        if ((flag & AT_EMPTY_PATH) != AT_EMPTY_PATH) {
                            ctx.ReturnWhenNotRunning(-EINVAL);
                            return;
                        }
                        if (!fd->stat(st)) {
                            ctx.ReturnWhenNotRunning(-EBADF);
                            return;
                        }
                        memcpy(statbuf, &st, sizeof(st));
                        ctx.ReturnWhenNotRunning(0);
                        return;
                    }
                    // TODO
                    std::cout << "statx(" << std::dec << dfd << ", \"" << filename << "\", " << std::hex
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