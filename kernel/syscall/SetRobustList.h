//
// Created by sigsegv on 7/18/22.
//

#ifndef JEOKERNEL_SETROBUSTLIST_H
#define JEOKERNEL_SETROBUSTLIST_H

#include "SyscallHandler.h"

class SetRobustList : public Syscall {
public:
    SetRobustList(SyscallHandler &handler) : Syscall(handler, 273) {}
    int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) override;
};


#endif //JEOKERNEL_SETROBUSTLIST_H
