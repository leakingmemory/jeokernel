//
// Created by sigsegv on 11/4/22.
//

#include "Clone.h"
#include "SyscallCtx.h"
#include "exec/usermem.h"
#include <iostream>
#include <errno.h>
#include <exec/procthread.h>

class TaskCloner : public SyscallInterruptFrameVisitor {
private:
    std::weak_ptr<TaskCloner> this_ref;
    hw_spinlock mtx;
    std::shared_ptr<Process> clonedProcess;
    uintptr_t uptr_child_tid;
    std::function<void (bool fault)> completed;
    bool isCompleted;
    bool isFaulted;
private:
    TaskCloner(std::shared_ptr<Process> clonedProcess, uintptr_t uptr_clild_tid) : this_ref(), mtx(), clonedProcess(clonedProcess), uptr_child_tid(uptr_clild_tid), completed(), isCompleted(false) {}
public:
    static std::shared_ptr<TaskCloner> Create(std::shared_ptr<Process> clonedProcess, uintptr_t uptr_clild_tid) {
        std::shared_ptr<TaskCloner> obj{new TaskCloner(clonedProcess, uptr_clild_tid)};
        std::weak_ptr<TaskCloner> weak{obj};
        obj->this_ref = weak;
        return obj;
    }
    static void SpawnFromInterrupt(ProcThread *, const x86_fpu_state &fpuState, InterruptStackFrame &cpuState, const InterruptCpuFrame &cpuFrame);
    void VisitInterruptFrame(Interrupt &intr) override;
    void Complete(bool fault);
    bool CallbackIfAsyncOrCompletedNow(std::function<void (bool fault)> completed);
    bool IsFaulted() const {
        return isFaulted;
    }
};

void TaskCloner::SpawnFromInterrupt(ProcThread *process, const x86_fpu_state &fpuState, InterruptStackFrame &cpuState, const InterruptCpuFrame &cpuFrame) {
    std::vector<task_resource *> resources{};
    auto *scheduler = get_scheduler();
    resources.push_back(process);
    cpuState.rax = 0;
    auto pid = scheduler->new_task(
            fpuState,
            cpuState,
            cpuFrame,
            resources);
    std::cout << "Started task " << pid << "\n";
    scheduler->set_name(pid, process->GetProcess()->GetCmdline());
}

void TaskCloner::VisitInterruptFrame(Interrupt &intr) {
    auto *process = new ProcThread(clonedProcess);
    if (uptr_child_tid != 0) {
        auto u_child_tid = uptr_child_tid;
        x86_fpu_state fpu = intr.get_fpu_state();
        InterruptStackFrame stackFrame = intr.get_cpu_state();
        InterruptCpuFrame cpuFrame = intr.get_cpu_frame();
        std::shared_ptr<TaskCloner> ref_self = this_ref.lock();
        auto resolve = clonedProcess->resolve_read(*process, uptr_child_tid, sizeof(pid_t), false, [ref_self, process] (intptr_t res) {
            bool fault = res != 0;
            if (fault) {
                delete process;
            }
            ref_self->Complete(fault);
        }, [process, u_child_tid, fpu, stackFrame, cpuFrame] (bool success, bool async, const std::function<void (intptr_t)> &asyncReturn) mutable {
            if (!success) {
                return resolve_return_value::Return(-EFAULT);
            }
            if (!process->resolve_write(u_child_tid, sizeof(pid_t))) {
                return resolve_return_value::Return(-EFAULT);
            }
            UserMemory userMem{*process, u_child_tid, sizeof(pid_t), true};
            *((pid_t *) userMem.Pointer()) = process->getpid();
            SpawnFromInterrupt(process, fpu, stackFrame, cpuFrame);
            return resolve_return_value::Return(0);
        });
        if (resolve.hasValue) {
            bool fault = resolve.result != 0;
            if (fault) {
                delete process;
            }
            Complete(fault);
        }
        return;
    }
    InterruptStackFrame stackFrame = intr.get_cpu_state();
    SpawnFromInterrupt(process, intr.get_fpu_state(), stackFrame, intr.get_cpu_frame());
}

void TaskCloner::Complete(bool fault) {
    std::unique_lock lock{mtx};
    isFaulted = fault;
    isCompleted = true;
    if (completed) {
        auto func = completed;
        completed = {};
        lock.release();
        func(fault);
        return;
    }
}

bool TaskCloner::CallbackIfAsyncOrCompletedNow(std::function<void (bool fault)> completed) {
    std::lock_guard lock{mtx};
    if (!isCompleted) {
        this->completed = completed;
        return false;
    }
    return true;
}

int64_t Clone::Call(int64_t flags, int64_t new_stackp, int64_t uptr_parent_tidptr, int64_t uptr_child_tidptr, SyscallAdditionalParams &params) {
    auto tls = params.Param5();
    std::cout << "clone(" << std::hex << flags << ", " << new_stackp << ", " << uptr_parent_tidptr << ", " << uptr_child_tidptr
    << ", " << tls << std::dec << ")\n";
    if ((flags & CloneSupportedFlags) != flags) {
        return -EOPNOTSUPP;
    }
    SyscallCtx ctx{params};
    std::shared_ptr<Process> clonedProcess = ctx.GetProcess().Clone();
    std::shared_ptr<TaskCloner> taskCloner = TaskCloner::Create(clonedProcess, flags & CLONE_CHILD_SETTID ? uptr_child_tidptr : 0);
    params.Accept(*taskCloner);
    auto childPid = clonedProcess->getpid();
    bool completed = taskCloner->CallbackIfAsyncOrCompletedNow([ctx, childPid] (bool faulted) {
        if (faulted) {
            ctx.AsyncCtx()->returnAsync(-EFAULT);
            return;
        }
        ctx.AsyncCtx()->returnAsync(childPid);
    });
    if (completed) {
        if (taskCloner->IsFaulted()) {
            return -EFAULT;
        }
        return childPid;
    } else {
        ctx.AsyncCtx()->async();
        return 0;
    }
}