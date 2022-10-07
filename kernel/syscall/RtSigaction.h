//
// Created by sigsegv on 10/6/22.
//

#ifndef JEOKERNEL_RTSIGACTION_H
#define JEOKERNEL_RTSIGACTION_H

#include "SyscallHandler.h"

class ProcThread;
struct sigaction;

class RtSigaction : Syscall {
public:
    RtSigaction(SyscallHandler &handler) : Syscall(handler, 13) {}
    int DoRtSigaction(ProcThread &, int signal, const sigaction *act, sigaction *oact);
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_RTSIGACTION_H
