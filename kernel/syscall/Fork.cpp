//
// Created by sigsegv on 8/11/23.
//

#include "Fork.h"

int64_t Fork::Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    return DoClone(0, 0, 0, 0, 0, params);
}
