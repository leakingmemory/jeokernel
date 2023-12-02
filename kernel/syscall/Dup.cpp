//
// Created by sigsegv on 10/10/22.
//

#include "Dup.h"
#include <exec/procthread.h>
#include <errno.h>
#include <resource/referrer.h>
#include <resource/reference.h>

class Dup_Call : public referrer {
private:
    std::weak_ptr<Dup_Call> selfRef{};
    Dup_Call() : referrer("Dup_Call") {}
public:
    static std::shared_ptr<Dup_Call> Create();
    std::string GetReferrerIdentifier() override;
    int Call(ProcThread *process, int oldfd);
};

std::shared_ptr<Dup_Call> Dup_Call::Create() {
    std::shared_ptr<Dup_Call> dupCall{new Dup_Call()};
    std::weak_ptr<Dup_Call> weakPtr{dupCall};
    dupCall->selfRef = weakPtr;
    return dupCall;
}

std::string Dup_Call::GetReferrerIdentifier() {
    return "";
}

int Dup_Call::Call(ProcThread *process, int oldfd) {
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    auto old = process->get_file_descriptor(selfRef, oldfd);
    if (!old) {
        return -EBADF;
    }
    auto newf = process->create_file_descriptor(selfRef, old->get_open_flags(), old->GetHandler(selfRef)->clone(selfRef));
    if (!newf) {
        return -EMFILE;
    }
    return newf->FD();
}

int64_t Dup::Call(int64_t oldfd, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    auto *process = params.CurrentThread();
    return Dup_Call::Create()->Call(process, (int) oldfd);
}