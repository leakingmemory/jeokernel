//
// Created by sigsegv on 11/15/22.
//

#include "Prctl.h"
#include <iostream>
#include <errno.h>
#include <sys/prctl.h>
#include <sys/capability.h>

int64_t Prctl::Call(int64_t option, int64_t arg2, int64_t arg3, int64_t arg4, SyscallAdditionalParams &handler) {
    switch (option) {
        case PR_CAPBSET_READ:
            switch (arg2) {
                case CAP_MAC_OVERRIDE:
                    return 1;
                case CAP_MAC_ADMIN:
                    return 1;
                case CAP_SYSLOG:
                    return 1;
                case CAP_BLOCK_SUSPEND:
                    return 0;
                case CAP_AUDIT_READ:
                    return 0;
                case CAP_PERFMON:
                    return 0;
                case CAP_CHECKPOINT_RESTORE:
                    return 0;
            }
            std::cout << "prctl(PR_CAPBSET_READ, " << std::hex << arg2 << ", " << arg3 << ", " << arg4 << std::dec << "...)\n";
            return -EINVAL;
    }
    std::cout << "prctl(" << std::hex << option << ", " << arg2 << ", " << arg3 << ", " << arg4 << std::dec << "...)\n";
    return -EOPNOTSUPP;
}