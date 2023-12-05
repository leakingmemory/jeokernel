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
    const char *name;
public:
    SyscallCtxAsync(SyscallAdditionalParams &params, const char *name);
    void async() override;
    void returnAsync(intptr_t value) override;
};

SyscallCtxAsync::SyscallCtxAsync(SyscallAdditionalParams &params, const char *name) :
    params(&params),
    scheduler(params.Scheduler()),
    current_task(params.CurrentTask()),
    process(params.CurrentThread()),
    name(name)
{
}

void SyscallCtxAsync::async() {
    current_task->set_blocked(name, true);
    params->DoContextSwitch(true);
}

void SyscallCtxAsync::returnAsync(intptr_t value) {
    task *t = this->current_task;
    auto *scheduler = this->scheduler;
    auto procthread = t->get_resource<ProcThread>();
    procthread->ClearAborterFunc();
    scheduler->when_not_running(*t, [scheduler, procthread, t, value] () {
        if (procthread->IsKilled()) {
            t->set_end(true);
            return;
        }
        auto signal = procthread != nullptr ? procthread->GetAndClearSigpending() : -1;
        if (signal > 0) {
            auto optSigaction = procthread->GetSigaction(signal);
            if (optSigaction) {
                t->get_cpu_state().rax = (uint64_t) value;
                callctx_async::HandleSignalInWhenNotRunning(scheduler, t, procthread, *optSigaction, signal);
            } else {
                t->set_end(true);
            }
            return;
        }
        t->get_cpu_state().rax = (uint64_t) value;
        t->set_blocked("asyncret", false);
    });
}

SyscallCtx::SyscallCtx(SyscallAdditionalParams &params, const char *name) : callctx(std::make_shared<SyscallCtxAsync>(params, name)){}