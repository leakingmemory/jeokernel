//
// Created by sigsegv on 8/31/23.
//

#include "Kill.h"
#include <exec/procthread.h>
#include <errno.h>

int64_t Kill::Call(int64_t ipid, int64_t isig, int64_t, int64_t, SyscallAdditionalParams &params) {
    auto *scheduler = params.Scheduler();
    uid_t euid;
    pid_t our_pid;
    {
        auto *pt = params.CurrentThread();
        euid = pt->geteuid();
        our_pid = pt->getpid();
    }
    std::shared_ptr<Process> process{};
    auto sig = (int) isig;
    auto pid = (pid_t) ipid;
    if (pid <= 0) {
        return -EOPNOTSUPP;
    }
    scheduler->all_tasks([pid, &process](task &t) {
        auto *pt = t.get_resource<ProcThread>();
        if (pt != nullptr && pt->getpid() == pid) {
            process = pt->GetProcess();
        }
    });
    if (!process) {
        return -ESRCH;
    }
    if (our_pid != pid && euid != 0 && euid != process->getuid()) {
        return -EPERM;
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