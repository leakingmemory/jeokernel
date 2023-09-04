//
// Created by sigsegv on 10/4/22.
//

#include "SyscallCtx.h"
#include <core/scheduler.h>
#include <exec/procthread.h>
#include <exec/usermem.h>

class SyscallCtxAsync : public callctx_async{
private:
    SyscallAdditionalParams *params;
    tasklist *scheduler;
    task *current_task;
    ProcThread *process;
public:
    SyscallCtxAsync(SyscallAdditionalParams &params);
    void async() override;
    void returnAsync(intptr_t value) override;
};

SyscallCtxAsync::SyscallCtxAsync(SyscallAdditionalParams &params) :
    params(&params),
    scheduler(get_scheduler()),
    current_task(&(scheduler->get_current_task())),
    process(current_task->get_resource<ProcThread>())
{
}

void SyscallCtxAsync::async() {
    current_task->set_blocked(true);
    params->DoContextSwitch(true);
}

void SyscallCtxAsync::returnAsync(intptr_t value) {
    task *t = this->current_task;
    auto *scheduler = this->scheduler;
    auto procthread = t->get_resource<ProcThread>();
    procthread->ClearAborterFunc();
    scheduler->when_not_running(*t, [scheduler, procthread, t, value] () {
        auto signal = procthread != nullptr ? procthread->GetAndClearSigpending() : -1;
        if (signal > 0) {
            auto optSigaction = procthread->GetSigaction(signal);
            if (optSigaction) {
                t->get_cpu_state().rax = (uint64_t) value;
                callctx_async::HandleSignalInWhenNotRunning(scheduler, t, procthread, *optSigaction, signal);
            } else {
                scheduler->evict_task_with_lock(*t);
            }
            return;
        }
        t->get_cpu_state().rax = (uint64_t) value;
        t->set_blocked(false);
    });
}

SyscallCtx::SyscallCtx(SyscallAdditionalParams &params) : callctx(std::make_shared<SyscallCtxAsync>(params)){}