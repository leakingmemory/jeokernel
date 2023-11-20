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
    file_descriptor_result Call(tasklist *scheduler, task *current_task, int fd, uintptr_t user_iov_ptr, int iovcnt);
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

file_descriptor_result Writev_Call::Call(tasklist *scheduler, task *current_task, int fd, uintptr_t user_iov_ptr, int iovcnt) {
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    auto *process = scheduler->get_resource<ProcThread>(*current_task);
    auto desc = process->get_file_descriptor(selfRef, fd);
    if (!desc) {
        return {.result = -EBADF, .async = false};
    }
    return desc->writev(process, user_iov_ptr, iovcnt, [scheduler, current_task] (intptr_t result) {
        scheduler->when_not_running(*current_task, [current_task, result] () {
            current_task->get_cpu_state().rax = (uint64_t) result;
            current_task->set_blocked(false);
        });
    });
}

int64_t Writev::Call(int64_t fd, int64_t user_iov_ptr, int64_t iovcnt, int64_t, SyscallAdditionalParams &params) {
    auto *scheduler = get_scheduler();
    task *current_task = &(scheduler->get_current_task());
    auto result = Writev_Call::Create()->Call(scheduler, current_task, (int) fd, user_iov_ptr, (int) iovcnt);
    if (result.async) {
        current_task->set_blocked(true);
        params.DoContextSwitch(true);
        return 0;
    } else {
        return result.result;
    }
}