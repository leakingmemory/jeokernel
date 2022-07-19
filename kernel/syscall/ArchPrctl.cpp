//
// Created by sigsegv on 7/17/22.
//

#include <interrupt_frame.h>
#include <sys/prctl.h>
#include <errno.h>
#include <iostream>
#include "ArchPrctl.h"

int64_t ArchPrctl::Call(int64_t ctl, int64_t addr, int64_t, int64_t, SyscallAdditionalParams &additionalParams) {
    switch (ctl) {
        case ARCH_SET_FS:
            additionalParams.ModifyCpuState([addr] (Interrupt &intr) { intr.get_cpu_state().fsbase = addr; });
            return 0;
        case ARCH_SET_GS:
            std::cerr << "arch_prctl: set gsbase not impl\n";
            return -EOPNOTSUPP;
        case ARCH_GET_FS:
            std::cerr << "arch_prctl: get fsbase not impl\n";
            return -EOPNOTSUPP;
        case ARCH_GET_GS:
            std::cerr << "arch_prctl: get gsbase not impl\n";
            return -EOPNOTSUPP;
        default:
            std::cerr << "arch_prctl: invalid arch_prctl " << ctl << "\n";
            return -EOPNOTSUPP;
    }
}