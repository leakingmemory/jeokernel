//
// Created by sigsegv on 8/22/23.
//

#include "Pipe.h"
#include <exec/pipe.h>
#include <exec/procthread.h>
#include <fcntl.h>
#include <resource/reference.h>
#include <resource/referrer.h>
#include "SyscallCtx.h"

class Pipe_Call : public referrer {
private:
    std::weak_ptr<Pipe_Call> selfRef{};
    Pipe_Call() : referrer("Pipe_Call") {}
public:
    static std::shared_ptr<Pipe_Call> Create();
    std::string GetReferrerIdentifier() override;
    resolve_return_value Call(const SyscallCtx &ctx, int *fdsptr);
};

std::shared_ptr<Pipe_Call> Pipe_Call::Create() {
    std::shared_ptr<Pipe_Call> pipeCall{new Pipe_Call()};
    std::weak_ptr<Pipe_Call> weakPtr{pipeCall};
    pipeCall->selfRef = weakPtr;
    return pipeCall;
}

std::string Pipe_Call::GetReferrerIdentifier() {
    return "";
}

resolve_return_value Pipe_Call::Call(const SyscallCtx &ctx, int *fdsptr) {
    int &fd1 = fdsptr[0];
    int &fd2 = fdsptr[1];
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    reference<FileDescriptorHandler> pipe1{};
    reference<FileDescriptorHandler> pipe2{};
    PipeDescriptorHandler::CreateHandlerPair(selfRef, pipe1, pipe2);
    auto fdesc1 = ctx.GetProcess().create_file_descriptor(O_RDWR, pipe1);
    auto fdesc2 = ctx.GetProcess().create_file_descriptor(O_RDWR, pipe2);
    fd1 = fdesc1->FD();
    fd2 = fdesc2->FD();
    return resolve_return_value::Return(0);
}

int64_t Pipe::Call(int64_t uptr_fds, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};
    return ctx.Write(uptr_fds, sizeof(int) * 2, [ctx] (void *fdsptr) {
        return Pipe_Call::Create()->Call(ctx, (int *) fdsptr);
    });
}