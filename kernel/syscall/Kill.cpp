//
// Created by sigsegv on 8/31/23.
//

#include "Kill.h"
#include <exec/procthread.h>
#include <errno.h>

int64_t Kill::Call(int64_t ipid, int64_t isig, int64_t, int64_t, SyscallAdditionalParams &) {
    std::shared_ptr<Process> process{};
    auto sig = (int) isig;
    {
        auto pid = (pid_t) ipid;
        if (pid <= 0) {
            return -EOPNOTSUPP;
        }
        auto *scheduler = get_scheduler();
        scheduler->all_tasks([pid, &process](task &t) {
            auto *pt = t.get_resource<ProcThread>();
            if (pt != nullptr && pt->getpid() == pid) {
                process = pt->GetProcess();
            }
        });
    }
    if (!process) {
        return -ESRCH;
    }
    int err = process->setsignal(sig);
    if (err > 0) {
        process->CallAbort();
    }
    if (err < 0) {
        return err;
    }
    return 0;
}