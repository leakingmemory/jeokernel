//
// Created by sigsegv on 10/11/22.
//

#include "Fcntl.h"
#include <exec/procthread.h>
#include <errno.h>
#include <iostream>
#include <fcntl.h>

class Fcntl_Call : public referrer {
private:
    std::weak_ptr<Fcntl_Call> selfRef{};
    Fcntl_Call() : referrer("Fcntl_Call") {}
public:
    static std::shared_ptr<Fcntl_Call> Create();
    std::string GetReferrerIdentifier() override;
    intptr_t Call(ProcThread *, int fd, intptr_t cmd, intptr_t arg);
};

std::shared_ptr<Fcntl_Call> Fcntl_Call::Create() {
    std::shared_ptr<Fcntl_Call> fcntlCall{new Fcntl_Call()};
    std::weak_ptr<Fcntl_Call> weakPtr{fcntlCall};
    fcntlCall->selfRef = weakPtr;
    return fcntlCall;
}

std::string Fcntl_Call::GetReferrerIdentifier() {
    return "";
}

intptr_t Fcntl_Call::Call(ProcThread *process, int fd, intptr_t cmd, intptr_t arg) {
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    auto fdesc = process->get_file_descriptor(selfRef, fd);
    if (!fdesc) {
        return -EBADF;
    }
    if (cmd == F_GETFL) {
        return fdesc->get_open_flags();
    }
    std::cout << "fcntl("<<std::dec<<fd<<", 0x" << std::hex << cmd << ", 0x" << arg << std::dec << ")\n";
    return -EOPNOTSUPP;
}

int64_t Fcntl::Call(int64_t fd, int64_t cmd, int64_t arg, int64_t, SyscallAdditionalParams &params) {
    return Fcntl_Call::Create()->Call(params.CurrentThread(), (int) fd, cmd, arg);
}
