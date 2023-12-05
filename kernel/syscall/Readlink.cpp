//
// Created by sigsegv on 8/2/22.
//

#include <iostream>
#include <exec/procthread.h>
#include <exec/usermem.h>
#include <errno.h>
#include <resource/referrer.h>
#include <resource/reference.h>
#include "Readlink.h"
#include "SyscallCtx.h"

class Readlink_Call : public referrer {
private:
    std::weak_ptr<Readlink_Call> selfRef{};
    Readlink_Call() : referrer("Readlink_Call") {}
public:
    static std::shared_ptr<Readlink_Call> Create();
    std::string GetReferrerIdentifier() override;
    std::string Call(ProcThread &proc, const std::string &path, int &errno);
};

std::shared_ptr<Readlink_Call> Readlink_Call::Create() {
    std::shared_ptr<Readlink_Call> readl{new Readlink_Call()};
    std::weak_ptr<Readlink_Call> weakPtr{readl};
    readl->selfRef = weakPtr;
    return readl;
}

std::string Readlink_Call::GetReferrerIdentifier() {
    return "";
}

std::string Readlink_Call::Call(ProcThread &proc, const std::string &path, int &errno) {
    auto fileResolve = proc.ResolveFile(selfRef.lock(), path);
    auto file = std::move(fileResolve.result);
    if (fileResolve.status != kfile_status::SUCCESS) {
        switch (fileResolve.status) {
            case kfile_status::IO_ERROR:
                errno = -EIO;
                return {};
            case kfile_status::NOT_DIRECTORY:
                errno = -ENOTDIR;
                return {};
            default:
                errno = -EIO;
                return {};
        }
    }
    if (!file) {
        errno = -ENOENT;
        return {};
    }
    ksymlink *symlink = dynamic_cast<ksymlink *>(&(*file));
    if (symlink == nullptr) {
        errno = -EINVAL;
        return {};
    }
    errno = 0;
    return symlink->GetLink();
}

int64_t Readlink::Call(int64_t uptr_path, int64_t uptr_buf, int64_t bufsize, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params, "Readlink"};
    auto task_id = params.TaskId();
    return ctx.ReadString(uptr_path, [this, ctx, task_id, uptr_buf, bufsize] (const std::string &u_path) {
        std::string path{u_path};
        return ctx.NestedWrite(uptr_buf, bufsize, [this, ctx, task_id, path, bufsize] (void *ptr) {
            char *buf = (char *) ptr;

            Queue(task_id, [this, ctx, path, buf, bufsize] () {
                int errno;
                std::string link = Readlink_Call::Create()->Call(ctx.GetProcess(), path, errno);
                if (errno != 0) {
                    ctx.ReturnWhenNotRunning(errno);
                    return;
                }
                auto size = link.size();
                if (size > bufsize) {
                    size = bufsize;
                }
                memcpy(buf, link.c_str(), size);
                ctx.ReturnWhenNotRunning(size);
            });

            return resolve_return_value::AsyncReturn();
        });
    });
}
