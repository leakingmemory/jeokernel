//
// Created by sigsegv on 6/19/22.
//

#include <core/scheduler.h>
#include <exec/procthread.h>
#include <errno.h>
#include "Write.h"

class Write_Call : public referrer {
private:
    std::weak_ptr<Write_Call> selfRef{};
    Write_Call() : referrer("Write_Call") {}
public:
    static std::shared_ptr<Write_Call> Create();
    std::string GetReferrerIdentifier() override;
    file_descriptor_result Call(tasklist *scheduler, task *current_task, ProcThread *, int fd, uintptr_t ptr, intptr_t len);
};

std::shared_ptr<Write_Call> Write_Call::Create() {
    std::shared_ptr<Write_Call> writeCall{new Write_Call()};
    std::weak_ptr<Write_Call> weakPtr{writeCall};
    writeCall->selfRef = weakPtr;
    return writeCall;
}

std::string Write_Call::GetReferrerIdentifier() {
    return "";
}

file_descriptor_result Write_Call::Call(tasklist *scheduler, task *current_task, ProcThread *process, int fd, uintptr_t ptr, intptr_t len) {
    std::shared_ptr<class referrer> selfRef = this->selfRef.lock();
    auto desc = process->get_file_descriptor(selfRef, fd);
    if (!desc) {
        return {.result = -EBADF, .async = false};
    }
    return desc->write(process, ptr, len, [scheduler, current_task] (intptr_t result) {
        scheduler->when_not_running(*current_task, [current_task, result] () {
            current_task->get_cpu_state().rax = (uint64_t) result;
            current_task->set_blocked(false);
        });
    });
}

int64_t Write::Call(int64_t fd, int64_t ptr, int64_t len, int64_t, SyscallAdditionalParams &additionalParams) {
    auto *scheduler = additionalParams.Scheduler();
    task *current_task = additionalParams.CurrentTask();
    auto *process = additionalParams.CurrentThread();
    auto result = Write_Call::Create()->Call(scheduler, current_task, process, (int) fd, ptr, len);
    if (result.async) {
        current_task->set_blocked(true);
        additionalParams.DoContextSwitch(true);
        return 0;
    } else {
        return result.result;
    }
}