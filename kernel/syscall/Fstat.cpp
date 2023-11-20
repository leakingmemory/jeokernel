//
// Created by sigsegv on 2/24/23.
//

#include "Fstat.h"
#include "SyscallCtx.h"
#include <sys/stat.h>
#include <exec/procthread.h>
#include <errno.h>

class Fstat_Call : public referrer {
private:
    std::weak_ptr<Fstat_Call> selfRef{};
    reference<FileDescriptor> fdesc{};
    Fstat_Call() : referrer("Fstat_Call") {}
public:
    static std::shared_ptr<Fstat_Call> Create();
    std::string GetReferrerIdentifier() override;
    intptr_t Call(SyscallAdditionalParams &params, int fd, intptr_t uptr_statbuf);
};

std::shared_ptr<Fstat_Call> Fstat_Call::Create() {
    std::shared_ptr<Fstat_Call> fstatCall{new Fstat_Call()};
    std::weak_ptr<Fstat_Call> weakPtr{fstatCall};
    fstatCall->selfRef = weakPtr;
    return fstatCall;
}

std::string Fstat_Call::GetReferrerIdentifier() {
    return "";
}

intptr_t Fstat_Call::Call(SyscallAdditionalParams &params, int fd, intptr_t uptr_statbuf) {
    std::shared_ptr<Fstat_Call> selfRef = this->selfRef.lock();
    std::shared_ptr<class referrer> referrer = selfRef;
    SyscallCtx ctx{params};
    fdesc = ctx.GetProcess().get_file_descriptor(referrer, fd);
    if (!fdesc) {
        return -EBADF;
    }
    return ctx.Write(uptr_statbuf, sizeof(struct stat), [selfRef, ctx] (void *statbuf) {
        selfRef->fdesc->stat(*((struct stat *) statbuf));
        return ctx.Return(0);
    });
}

int64_t Fstat::Call(int64_t i_fd, int64_t uptr_statbuf, int64_t, int64_t, SyscallAdditionalParams &params) {
    int fd = (int) i_fd;
    return Fstat_Call::Create()->Call(params, fd, uptr_statbuf);
}