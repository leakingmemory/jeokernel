//
// Created by sigsegv on 8/22/23.
//

#include "Pipe.h"
#include <exec/pipe.h>
#include <exec/procthread.h>
#include <fcntl.h>
#include "SyscallCtx.h"

int64_t Pipe::Call(int64_t uptr_fds, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    SyscallCtx ctx{params};
    return ctx.Write(uptr_fds, sizeof(int) * 2, [ctx] (void *fdsptr) {
        int &fd1 = ((int *) fdsptr)[0];
        int &fd2 = ((int *) fdsptr)[1];
        std::shared_ptr<PipeDescriptorHandler> pipe1{};
        std::shared_ptr<PipeDescriptorHandler> pipe2{};
        PipeDescriptorHandler::CreateHandlerPair(pipe1, pipe2);
        auto fdesc1 = ctx.GetProcess().create_file_descriptor(O_RDWR, pipe1);
        auto fdesc2 = ctx.GetProcess().create_file_descriptor(O_RDWR, pipe2);
        fd1 = fdesc1.FD();
        fd2 = fdesc2.FD();
        return resolve_return_value::Return(0);
    });
}