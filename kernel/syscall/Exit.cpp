//
// Created by sigsegv on 6/18/22.
//

#include <core/scheduler.h>
#include "Exit.h"

int64_t Exit::Call(int64_t returnValue, int64_t, int64_t, int64_t, SyscallAdditionalParams &additionalParams) {
    auto *scheduler = get_scheduler();
    scheduler->user_exit(returnValue);
    additionalParams.DoContextSwitch(true);
    return 0;
}