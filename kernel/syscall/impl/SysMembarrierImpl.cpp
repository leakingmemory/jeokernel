//
// Created by sigsegv on 8/13/23.
//

#include "SysMembarrierImpl.h"
#include <errno.h>
#include <iostream>

enum class MembarrierCmd {
    QUERY = 0,
    GLOBAL = (1 << 0),
    GLOBAL_EXPEDITED = (1 << 1),
    REGISTER_GLOBAL_EXPEDITED = (1 << 2),
    PRIVATE_EXPEDITED = (1 << 3),
    REGISTER_PRIVATE_EXPEDITED = (1 << 4),
    PRIVATE_EXPEDITED_SYNC_CORE = (1 << 5),
    REGISTER_PRIVATE_EXPEDITED_SYNC_CORE = (1 << 6),
    PRIVATE_EXPEDITED_RSEQ = (1 << 7),
    REGISTER_PRIVATE_EXPEDITED_RSEQ = (1 << 8)
};

enum class MembarrierFlags {
    FLAG_CPU = 1
};

constexpr int supportedCmd = ((int)MembarrierCmd::QUERY | (int)MembarrierCmd::REGISTER_PRIVATE_EXPEDITED);
constexpr int notSupportedCmd = ~supportedCmd;
constexpr int advertiseAdditionalCmds = ((int)MembarrierCmd::GLOBAL | (int)MembarrierCmd::GLOBAL_EXPEDITED |
        (int)MembarrierCmd::REGISTER_GLOBAL_EXPEDITED | (int)MembarrierCmd::PRIVATE_EXPEDITED |
        (int)MembarrierCmd::REGISTER_PRIVATE_EXPEDITED | (int)MembarrierCmd::PRIVATE_EXPEDITED_RSEQ |
        (int)MembarrierCmd::REGISTER_PRIVATE_EXPEDITED_RSEQ | (int)MembarrierCmd::PRIVATE_EXPEDITED_SYNC_CORE |
        (int)MembarrierCmd::REGISTER_PRIVATE_EXPEDITED_SYNC_CORE);
constexpr int advertiseCmds = supportedCmd | advertiseAdditionalCmds;

int SysMembarrierImpl::DoMembarrier(ProcThread &process, int cmd, unsigned int flags, int cpu_id) {
    if ((cmd & notSupportedCmd) != 0) {
        std::cerr << "membarrier: unsupported cmd=" << cmd << "\n";
        return -EOPNOTSUPP;
    }
    if (cmd != (int)MembarrierCmd::QUERY) {
        return 0;
    } else {
        return advertiseCmds;
    }
}