//
// Created by sigsegv on 10/7/22.
//

#ifndef JEOKERNEL_GETCWD_H
#define JEOKERNEL_GETCWD_H

#include "SyscallHandler.h"
#include "SyscallCtx.h"

class ProcThread;

class Getcwd : public Syscall {
public:
    Getcwd(SyscallHandler &handler) : Syscall(handler, 79) {}
private:
    int DoGetcwd(ProcThread &, char *, intptr_t);
public:
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_GETCWD_H
