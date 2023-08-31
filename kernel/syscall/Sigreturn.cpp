//
// Created by sigsegv on 8/30/23.
//

#include "Sigreturn.h"
#include <signal.h>
#include <interrupt_frame.h>

int64_t Sigreturn::Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &params) {
    params.ModifyCpuState([] (Interrupt &intr) {
        intr.get_cpu_frame().rip = SIGTRAMP_ADDR;
    });
    return 0;
}