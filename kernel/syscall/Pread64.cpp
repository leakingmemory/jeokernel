//
// Created by sigsegv on 9/15/22.
//

#include "Pread64.h"
#include <exec/procthread.h>
#include <errno.h>
#include "SyscallCtx.h"

class Pread64_Call : public referrer {
private:
    std::weak_ptr<Pread64_Call> selfRef{};
    reference<FileDescriptor> desc;
    Pread64_Call() : referrer("Pread64_Call") {}
public:
    static std::shared_ptr<Pread64_Call> Create();
    std::string GetReferrerIdentifier() override;
    intptr_t Call(SyscallAdditionalParams &additionalParams, int fd, intptr_t uptr_buf, intptr_t count, intptr_t pos);
};

std::shared_ptr<Pread64_Call> Pread64_Call::Create() {
    std::shared_ptr<Pread64_Call> pread64Call{new Pread64_Call()};
    std::weak_ptr<Pread64_Call> weakPtr{pread64Call};
    pread64Call->selfRef = weakPtr;
    return pread64Call;
}

std::string Pread64_Call::GetReferrerIdentifier() {
    return "";
}

intptr_t
Pread64_Call::Call(SyscallAdditionalParams &additionalParams, int fd, intptr_t uptr_buf, intptr_t count, intptr_t pos) {
    std::shared_ptr<Pread64_Call> selfRef = this->selfRef.lock();
    std::shared_ptr<class referrer> referrer = selfRef;
    SyscallCtx ctx{additionalParams, "Pread64"};
    auto &process = ctx.GetProcess();
    desc = process.get_file_descriptor(referrer, fd);
    if (!desc) {
        return -EBADF;
    }
    auto task_id = additionalParams.TaskId();
    return ctx.Write(uptr_buf, count, [selfRef, &process, ctx, task_id, count, pos] (void *ptr) mutable {
        process.QueueBlocking(task_id, [selfRef, ctx, ptr, count, pos] () mutable {
            std::shared_ptr<SyscallCtx> shctx = std::make_shared<SyscallCtx>(ctx);
            auto result = selfRef->desc->read(shctx, ptr, count, pos);
            if (result.hasValue) {
                ctx.ReturnWhenNotRunning(result.result);
            }
        });
        return resolve_return_value::AsyncReturn();
    });
}

int64_t Pread64::Call(int64_t fd, int64_t uptr_buf, int64_t count, int64_t pos, SyscallAdditionalParams &additionalParams) {
    return Pread64_Call::Create()->Call(additionalParams, fd, uptr_buf, count, pos);
}