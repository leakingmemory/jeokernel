//
// Created by sigsegv on 2/22/23.
//

#include "SchedGetaffinity.h"
#include <iostream>
#include <errno.h>

int64_t SchedGetaffinity::Call(int64_t pid, int64_t len_64, int64_t uptr_mask, int64_t, SyscallAdditionalParams &params) {
    size_t len = (size_t) (((uint64_t) len_64) & 0xffffffff);
    std::cout << "sched_getaffinity(" << pid << ", " << len << ", " << uptr_mask << ")\n";
    return -EOPNOTSUPP;
}