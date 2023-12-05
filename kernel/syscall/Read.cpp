//
// Created by sigsegv on 9/14/22.
//

#include "Read.h"
#include <iostream>
#include <errno.h>
#include <exec/procthread.h>
#include "SyscallCtx.h"

class Read_Call : public referrer {
private:
    std::weak_ptr<Read_Call> selfRef{};
    reference<FileDescriptor> desc{};
    Read_Call() : referrer("Read_Call") {}
public:
    static std::shared_ptr<Read_Call> Create();
    std::string GetReferrerIdentifier() override;
    intptr_t Call(SyscallAdditionalParams &additionalParams, int fd, intptr_t uptr_buf, intptr_t count);
};

std::shared_ptr<Read_Call> Read_Call::Create() {
    std::shared_ptr<Read_Call> readCall{new Read_Call()};
    std::weak_ptr<Read_Call> weakPtr{readCall};
    readCall->selfRef = weakPtr;
    return readCall;
}

std::string Read_Call::GetReferrerIdentifier() {
    return "";
}

intptr_t Read_Call::Call(SyscallAdditionalParams &additionalParams, int fd, intptr_t uptr_buf, intptr_t count) {
    std::shared_ptr<Read_Call> selfRef = this->selfRef.lock();
    std::shared_ptr<class referrer> referrer = selfRef;
    SyscallCtx ctx{additionalParams, "Read"};
    auto &process = ctx.GetProcess();
    desc = process.get_file_descriptor(referrer, fd);
    if (!desc) {
        return -EBADF;
    }
    auto task_id = additionalParams.TaskId();
    return ctx.Write(uptr_buf, count, [selfRef, &process, ctx, task_id, count] (void *ptr) mutable {
        process.QueueBlocking(task_id, [selfRef, ctx, ptr, count] () mutable {
            std::shared_ptr<SyscallCtx> shctx = std::make_shared<SyscallCtx>(ctx);
            auto result = selfRef->desc->read(shctx, ptr, count);
            if (result.hasValue) {
                ctx.ReturnWhenNotRunning(result.result);
            }
        });
        return resolve_return_value::AsyncReturn();
    });
}

int64_t Read::Call(int64_t fd, int64_t uptr_buf, int64_t count, int64_t, SyscallAdditionalParams &additionalParams) {
    return Read_Call::Create()->Call(additionalParams, fd, uptr_buf, count);
}
