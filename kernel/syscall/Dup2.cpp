//
// Created by sigsegv on 10/11/22.
//

#include "Dup2.h"
#include <exec/procthread.h>
#include <errno.h>
#include <resource/referrer.h>
#include <resource/reference.h>

class Dup2_Call : public referrer {
private:
    std::weak_ptr<Dup2_Call> selfRef{};
    Dup2_Call() : referrer("Dup_Call") {}
public:
    static std::shared_ptr<Dup2_Call> Create();
    std::string GetReferrerIdentifier() override;
    int Call(ProcThread *process, int oldfd, int newfd);
};

std::shared_ptr<Dup2_Call> Dup2_Call::Create() {
    std::shared_ptr<Dup2_Call> dupCall{new Dup2_Call()};
    std::weak_ptr<Dup2_Call> weakPtr{dupCall};
    dupCall->selfRef = weakPtr;
    return dupCall;
}

std::string Dup2_Call::GetReferrerIdentifier() {
    return "";
}

int Dup2_Call::Call(ProcThread *process, int oldfd, int newfd) {
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    auto old = process->get_file_descriptor(selfRef, oldfd);
    if (!old) {
        return -EBADF;
    }
    auto newf = process->create_file_descriptor(selfRef, old->get_open_flags(), old->GetHandler(selfRef)->clone(selfRef), newfd);
    return newf->FD();
}

int64_t Dup2::Call(int64_t oldfd, int64_t newfd, int64_t, int64_t, SyscallAdditionalParams &params) {
    auto *process = params.CurrentThread();
    return Dup2_Call::Create()->Call(process, (int) oldfd, (int) newfd);
}