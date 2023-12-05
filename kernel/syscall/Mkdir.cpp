//
// Created by sigsegv on 3/6/23.
//

#include "Mkdir.h"
#include "SyscallCtx.h"
#include <errno.h>
#include <iostream>
#include <resource/referrer.h>
#include <exec/procthread.h>

class Mkdir_Call : public referrer {
private:
    std::weak_ptr<Mkdir_Call> selfRef{};
    Mkdir_Call() : referrer("Mkdir_Call") {}
public:
    static std::shared_ptr<Mkdir_Call> Create();
    std::string GetReferrerIdentifier() override;
    resolve_return_value Call(const SyscallCtx &ctx, const std::string &pathname, const std::string &dirname, int32_t mode);
};

std::shared_ptr<Mkdir_Call> Mkdir_Call::Create() {
    std::shared_ptr<Mkdir_Call> mkdirCall{new Mkdir_Call()};
    std::weak_ptr<Mkdir_Call> weakPtr{mkdirCall};
    mkdirCall->selfRef = weakPtr;
    return mkdirCall;
}

std::string Mkdir_Call::GetReferrerIdentifier() {
    return "";
}

resolve_return_value Mkdir_Call::Call(const SyscallCtx &i_ctx, const std::string &i_pathname, const std::string &i_dirname, int32_t mode) {
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    int32_t task_id;
    {
        auto *scheduler = get_scheduler();
        task_id = scheduler->get_current_task_id();
    }
    SyscallCtx ctx{i_ctx};
    std::string pathname{i_pathname};
    std::string dirname{i_dirname};
    if (pathname.empty()) {
        pathname = ".";
    }
    ctx.GetProcess().QueueBlocking(task_id, [selfRef, ctx, pathname, dirname, mode] () mutable {
        auto fileResolve = ctx.GetProcess().ResolveFile(selfRef, pathname);
        if (fileResolve.status != kfile_status::SUCCESS) {
            switch (fileResolve.status) {
                case kfile_status::IO_ERROR:
                    ctx.ReturnWhenNotRunning(-EIO);
                    return;
                case kfile_status::NOT_DIRECTORY:
                    ctx.ReturnWhenNotRunning(-ENOTDIR);
                    return;
                default:
                    ctx.ReturnWhenNotRunning(-EIO);
                    return;
            }
        }
        if (!fileResolve.result) {
            ctx.ReturnWhenNotRunning(-ENOENT);
            return;
        }
        auto dir = reference_dynamic_cast<kdirectory>(std::move(fileResolve.result));
        if (!dir) {
            ctx.ReturnWhenNotRunning(-ENOTDIR);
            return;
        }
        auto result = dir->CreateDirectory(selfRef, dirname, mode);
        if (result.status != kfile_status::SUCCESS) {
            switch (result.status) {
                case kfile_status::SUCCESS:
                case kfile_status::IO_ERROR:
                case kfile_status::TOO_MANY_LINKS:
                    ctx.ReturnWhenNotRunning(-EIO);
                    return;
                case kfile_status::NO_AVAIL_INODES:
                case kfile_status::NO_AVAIL_BLOCKS:
                    ctx.ReturnWhenNotRunning(-ENOSPC);
                    return;
                case kfile_status::EXISTS:
                    ctx.ReturnWhenNotRunning(-EEXIST);
                    return;
                case kfile_status::NOT_DIRECTORY:
                    ctx.ReturnWhenNotRunning(-ENOTDIR);
                    return;
            }
        }
        if (!result.result) {
            ctx.ReturnWhenNotRunning(-EIO);
            return;
        }
        ctx.ReturnWhenNotRunning(0);
    });
    return ctx.Async();
}

int64_t Mkdir::Call(int64_t uptr_pathname, int64_t u_mode, int64_t, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params, "Mkdir"};
    int32_t mode = (int32_t) u_mode;
    return ctx.ReadString(uptr_pathname, [ctx, mode] (const std::string &i_pathname) {
        std::cout << "mkdir(" << i_pathname << ", " << std::oct << mode << std::dec << ")\n";
        if (i_pathname.empty()) {
            return ctx.Return(-EINVAL);
        }
        std::string pathname{i_pathname};
        while (pathname.ends_with('/')) {
            pathname.resize(pathname.size() - 1);
        }
        if (pathname.empty()) {
            return ctx.Return(-EEXIST);
        }
        std::string dirname{};
        auto slashpos = pathname.find_last_of('/');
        if (slashpos < pathname.size()) {
            dirname = pathname.substr(slashpos + 1);
            pathname.resize(slashpos);
            while (pathname.ends_with('/')) {
                pathname.resize(pathname.size() - 1);
            }
            if (pathname.empty()) {
                pathname = "/";
            }
        } else {
            dirname = pathname;
            pathname = "";
        }
        return Mkdir_Call::Create()->Call(ctx, pathname, dirname, mode);
    });
}