//
// Created by sigsegv on 7/18/22.
//

#include <exec/procthread.h>
#include "SetRobustList.h"

int64_t SetRobustList::Call(int64_t addr, int64_t length, int64_t, int64_t, SyscallAdditionalParams &params) {
    params.CurrentThread()->SetRobustListHead((uintptr_t) addr);
    return 0;
}