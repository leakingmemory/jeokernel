//
// Created by sigsegv on 7/25/22.
//

#include <core/scheduler.h>
#include <exec/procthread.h>
#include <errno.h>
#include "Writev.h"

class Writev_Call : public referrer {
private:
    std::weak_ptr<Writev_Call> selfRef{};
    Writev_Call() : referrer("Write_Call") {}
public:
    static std::shared_ptr<Writev_Call> Create();
    std::string GetReferrerIdentifier() override;
    file_descriptor_result Call(tasklist *scheduler, task *current_task, ProcThread *, int fd, uintptr_t user_iov_ptr, int iovcnt);
};

std::shared_ptr<Writev_Call> Writev_Call::Create() {
    std::shared_ptr<Writev_Call> writeCall{new Writev_Call()};
    std::weak_ptr<Writev_Call> weakPtr{writeCall};
    writeCall->selfRef = weakPtr;
    return writeCall;
}

std::string Writev_Call::GetReferrerIdentifier() {
    return "";
}

file_descriptor_result Writev_Call::Call(tasklist *scheduler, task *current_task, ProcThread *process, int fd, uintptr_t user_iov_ptr, int iovcnt) {
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    auto desc = process->get_file_descriptor(selfRef, fd);
    if (!desc) {
        return {.result = -EBADF, .async = false};
    }
    return desc->writev(process, user_iov_ptr, iovcnt, [scheduler, current_task] (intptr_t result) {
        scheduler->when_not_running(*current_task, [current_task, result] () {
            current_task->get_cpu_state().rax = (uint64_t) result;
            current_task->set_blocked("writvret", false);
        });
    });
}

int64_t Writev::Call(int64_t fd, int64_t user_iov_ptr, int64_t iovcnt, int64_t, SyscallAdditionalParams &params) {
    auto *scheduler = params.Scheduler();
    task *current_task = params.CurrentTask();
    auto *process = params.CurrentThread();
    auto result = Writev_Call::Create()->Call(scheduler, current_task, process, (int) fd, user_iov_ptr, (int) iovcnt);
    if (result.async) {
        current_task->set_blocked("writev", true);
        params.DoContextSwitch(true);
        return 0;
    } else {
        return result.result;
    }
}