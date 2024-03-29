//
// Created by sigsegv on 6/18/22.
//

//#define EXIT_DEBUG

#include <core/scheduler.h>
#include "Exit.h"
#include <exec/procthread.h>
#ifdef EXIT_DEBUG
#include <iostream>
#endif

int64_t Exit::Call(int64_t returnValue, int64_t, int64_t, int64_t, SyscallAdditionalParams &additionalParams) {
    auto *scheduler = additionalParams.Scheduler();
    auto *process = additionalParams.CurrentThread();
#ifdef EXIT_DEBUG
    std::cout << "exit(" << returnValue << ") pid " << process->getpid() << " tid " << process->gettid() << "\n";
#endif
    process->SetExitCode(returnValue);
    scheduler->user_exit(returnValue);
    additionalParams.DoContextSwitch(true);
    return 0;
}