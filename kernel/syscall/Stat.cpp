//
// Created by sigsegv on 8/10/23.
//

#include <sys/stat.h>
#include <exec/procthread.h>
#include <errno.h>
#include <exec/files.h>
#include <resource/referrer.h>
#include <resource/reference.h>
#include "Stat.h"
#include "SyscallCtx.h"

class Stat_Call : public referrer {
private:
    std::weak_ptr<Stat_Call> selfRef{};
    Stat_Call() : referrer("Stat_Call") {}
public:
    static std::shared_ptr<Stat_Call> Create();
    std::string GetReferrerIdentifier() override;
    void Call(SyscallCtx &ctx, struct stat *stbuf, const std::string &filename);
};

std::shared_ptr<Stat_Call> Stat_Call::Create() {
    std::shared_ptr<Stat_Call> statCall{new Stat_Call()};
    std::weak_ptr<Stat_Call> weakPtr{statCall};
    statCall->selfRef = weakPtr;
    return statCall;
}

std::string Stat_Call::GetReferrerIdentifier() {
    return "";
}

void Stat_Call::Call(SyscallCtx &ctx, struct stat *stbuf, const std::string &filename) {
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
    struct stat st;
    FsStat::Stat(*file, st);
    memcpy(stbuf, &st, sizeof(st));
    ctx.ReturnWhenNotRunning(0);
}

int64_t Stat::Call(int64_t uptr_filename, int64_t uptr_statbuf, int64_t, int64_t, SyscallAdditionalParams &handler) {
    SyscallCtx ctx{handler, "Stat"};
    auto task_id = handler.TaskId();

    return ctx.ReadString(uptr_filename, [this, ctx, uptr_statbuf, task_id] (const std::string &inFilename) {
        std::string filename{inFilename};
        return ctx.NestedWrite(uptr_statbuf, sizeof(struct stat), [this, ctx, filename, task_id] (void *ptr) {
            auto *stbuf = (struct stat *) ptr;
            Queue(task_id, [ctx, filename, stbuf] () mutable {
                Stat_Call::Create()->Call(ctx, stbuf, filename);
            });
            return ctx.Async();
        });
    });
}