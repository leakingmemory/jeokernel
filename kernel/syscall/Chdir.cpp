//
// Created by sigsegv on 12/5/22.
//

#include "Chdir.h"
#include "SyscallCtx.h"
#include <exec/procthread.h>
#include <errno.h>
#include <resource/referrer.h>
#include <resource/reference.h>

class Chdir_Call : public referrer {
private:
    std::weak_ptr<Chdir_Call> selfRef{};
    Chdir_Call() : referrer("Chdir_Call") {}
public:
    static std::shared_ptr<Chdir_Call> Create();
    std::string GetReferrerIdentifier() override;
    int Call(const std::shared_ptr<Process> &proc, const std::string &filename);
};

std::shared_ptr<Chdir_Call> Chdir_Call::Create() {
    std::shared_ptr<Chdir_Call> chdirCall{new Chdir_Call()};
    std::weak_ptr<Chdir_Call> weakPtr{chdirCall};
    chdirCall->selfRef = weakPtr;
    return chdirCall;
}

std::string Chdir_Call::GetReferrerIdentifier() {
    return "";
}

int Chdir_Call::Call(const std::shared_ptr<Process> &proc, const std::string &filename) {
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    auto file = proc->ResolveFile(selfRef, filename);
    switch (file.status) {
        case kfile_status::SUCCESS:
            break;
        case kfile_status::IO_ERROR:
            return -EIO;
        case kfile_status::NOT_DIRECTORY:
            return -ENOTDIR;
        case kfile_status::TOO_MANY_LINKS:
            return -ELOOP;
        case kfile_status::NO_AVAIL_INODES:
        case kfile_status::NO_AVAIL_BLOCKS:
        case kfile_status::EXISTS:
            return -EIO;
    }
    if (!file.result) {
        return -ENOENT;
    }
    reference<kdirectory> dir = reference_dynamic_cast<kdirectory>(std::move(file.result));
    if (!dir) {
        return -ENOTDIR;
    }
    proc->SetCwd(file.result);
    return 0;
}

int64_t Chdir::Call(int64_t uptr_filename, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params, "Chdir"};
    auto task_id = params.TaskId();
    return ctx.ReadString(uptr_filename, [this, ctx, task_id] (const std::string &r_filename) {
        std::string filename{r_filename};
        Queue(task_id, [this, ctx, filename] () {
            auto res = Chdir_Call::Create()->Call(ctx.GetProcess().GetProcess(), filename);
            ctx.ReturnWhenNotRunning(res);
        });
        return ctx.Async();
    });
}