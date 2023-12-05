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
#include <resource/reference.h>
#include <resource/referrer.h>
#include <iostream>
#include "Newfstatat.h"
#include "SyscallCtx.h"

//#define DEBUG_NEWFSTATAT

class Newfstatat_Call : public referrer {
private:
    std::weak_ptr<Newfstatat_Call> selfRef{};
    Newfstatat_Call() : referrer("Newfstatat_Call") {}
public:
    static std::shared_ptr<Newfstatat_Call> Create();
    std::string GetReferrerIdentifier() override;
    void Call(SyscallCtx &ctx, void *statbuf, const std::string &filename, int dfd, unsigned int flag);
};

std::shared_ptr<Newfstatat_Call> Newfstatat_Call::Create() {
    std::shared_ptr<Newfstatat_Call> newfstatat{new Newfstatat_Call()};
    std::weak_ptr<Newfstatat_Call> weakPtr{newfstatat};
    newfstatat->selfRef = weakPtr;
    return newfstatat;
}

std::string Newfstatat_Call::GetReferrerIdentifier() {
    return "";
}

void Newfstatat_Call::Call(SyscallCtx &ctx, void *statbuf, const std::string &filename, int dfd, unsigned int flag) {
    struct stat st{};

#ifdef DEBUG_NEWFSTATAT
    std::cout << "newfstatat(" << std::dec << dfd << ", \"" << filename << "\", " << std::hex
                              << "statbuf, " << flag << std::dec << ")\n";
#endif
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    if ((!filename.empty() && filename.starts_with("/")) || dfd == AT_FDCWD) {
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
    } else {
        reference<FileDescriptor> fd = ctx.GetProcess().get_file_descriptor(selfRef, dfd);
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
        std::cout << "newfstatat(" << std::dec << dfd << ", \"" << filename << "\", " << std::hex
                  << "statbuf, " << flag << std::dec << ")\n";
        asm("ud2");
    }
    memcpy(statbuf, &st, sizeof(st));
    ctx.ReturnWhenNotRunning(0);
}

int64_t Newfstatat::Call(int64_t i_dfd, int64_t uptr_filename, int64_t uptr_statbuf, int64_t flag, SyscallAdditionalParams &params) {
    auto dfd = (int32_t) i_dfd;

    if (uptr_filename == 0) {
        return -EINVAL;
    }

    constexpr int suppFlags = AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW | AT_SYMLINK_FOLLOW | AT_NO_AUTOMOUNT;
    constexpr int nonsuppFlags = ~suppFlags;

    if ((flag & nonsuppFlags) != 0) {
        return -EINVAL;
    }

    SyscallCtx ctx{params, "Nfstatat"};

    auto task_id = params.TaskId();

    return ctx.ReadString(uptr_filename, [this, ctx, task_id, uptr_statbuf, dfd, flag] (const std::string &u_filename) {
        std::string filename{u_filename};
        return ctx.NestedWrite(uptr_statbuf, sizeof(struct stat), [this, ctx, task_id, filename, dfd, flag] (void *statbuf) {
            Queue(task_id, [ctx, statbuf, filename, dfd, flag] () mutable {
                Newfstatat_Call::Create()->Call(ctx, statbuf, filename, dfd, flag);
            });
            return resolve_return_value::AsyncReturn();
        });
    });
}