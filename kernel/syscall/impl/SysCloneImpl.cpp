//
// Created by sigsegv on 8/11/23.
//

#include "SysCloneImpl.h"
#include "../SyscallHandler.h"
#include "../SyscallCtx.h"
#include "exec/usermem.h"
#include <iostream>
#include <errno.h>
#include <exec/procthread.h>

//#define DEBUG_CLONE_CALL
//#define DEBUG_CLONE_TASK_STARTED

class TaskCloner : public SyscallInterruptFrameVisitor {
private:
    std::weak_ptr<TaskCloner> this_ref;
    hw_spinlock mtx;
    std::shared_ptr<Process> clonedProcess;
    uintptr_t uptr_child_tid;
    uintptr_t tls;
    uintptr_t stackPtr;
    std::function<void (bool fault)> completed;
    pid_t tid;
    bool isCompleted;
    bool isFaulted;
    bool setTls;
    bool setStackPtr;
    bool setTid;
    bool setChildTid;
    bool clearChildTid;
private:
    TaskCloner(std::shared_ptr<Process> clonedProcess, uintptr_t uptr_clild_tid, bool setChildTid, bool clearChildTid) : this_ref(), mtx(), clonedProcess(clonedProcess), uptr_child_tid(uptr_clild_tid), completed(), isCompleted(false), setTls(false), setStackPtr(false), setChildTid(setChildTid), clearChildTid(clearChildTid) {}
public:
    static std::shared_ptr<TaskCloner> Create(std::shared_ptr<Process> clonedProcess, uintptr_t uptr_clild_tid, bool setChildTid, bool clearChildTid) {
        std::shared_ptr<TaskCloner> obj{new TaskCloner(clonedProcess, uptr_clild_tid, setChildTid, clearChildTid)};
        std::weak_ptr<TaskCloner> weak{obj};
        obj->this_ref = weak;
        return obj;
    }
    static void SpawnFromInterrupt(ProcThread *, const x86_fpu_state &fpuState, InterruptStackFrame &cpuState, const InterruptCpuFrame &cpuFrame);
    void SetTid(pid_t tid) {
        this->tid = tid;
        this->setTid = true;
    }
    void SetTls(uintptr_t tls) {
        this->tls = tls;
        this->setTls = true;
    }
    void SetStackPtr(uintptr_t stackPtr) {
        this->stackPtr = stackPtr;
        this->setStackPtr = true;
    }
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
#ifdef DEBUG_CLONE_TASK_STARTED
    std::cout << "Started task " << pid << "\n";
#endif
    scheduler->set_name(pid, process->GetProcess()->GetCmdline());
}

void TaskCloner::VisitInterruptFrame(Interrupt &intr) {
    auto *process = new ProcThread(clonedProcess);
    process->SetTidAddress(uptr_child_tid);
    process->SetClearTidAddress(clearChildTid);
    if (setTid) {
        process->SetTid(tid);
    }
    if (setChildTid) {
        auto u_child_tid = uptr_child_tid;
        x86_fpu_state fpu = intr.get_fpu_state();
        InterruptStackFrame stackFrame = intr.get_cpu_state();
        InterruptCpuFrame cpuFrame = intr.get_cpu_frame();
        std::shared_ptr<TaskCloner> ref_self = this_ref.lock();
        if (setTls) {
            stackFrame.fsbase = tls;
        }
        if (setStackPtr) {
            cpuFrame.rsp = stackPtr;
        }
        process->SetFsBase(stackFrame.fsbase);
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
    InterruptCpuFrame cpuFrame = intr.get_cpu_frame();
    if (setTls) {
        stackFrame.fsbase = tls;
    }
    if (setStackPtr) {
        cpuFrame.rsp = stackPtr;
    }
    process->SetFsBase(stackFrame.fsbase);
    SpawnFromInterrupt(process, intr.get_fpu_state(), stackFrame, cpuFrame);
    Complete(false);
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

void SysCloneImpl::DoClone(std::shared_ptr<callctx_async> async, std::shared_ptr<Process> clonedProcess,
                           std::shared_ptr<TaskCloner> taskCloner) {
    auto childPid = clonedProcess->getpid();
    bool completed = taskCloner->CallbackIfAsyncOrCompletedNow([async, childPid] (bool faulted) {
        if (faulted) {
            async->returnAsync(-EFAULT);
            return;
        }
        async->returnAsync(childPid);
    });
    if (completed) {
        if (taskCloner->IsFaulted()) {
            async->returnAsync(-EFAULT);
            return;
        }
        async->returnAsync(childPid);
    }
}

int SysCloneImpl::DoClone(int64_t flags, int64_t new_stackp, int64_t uptr_parent_tidptr, int64_t uptr_child_tidptr, int64_t tls, SyscallAdditionalParams &params) {
#ifdef DEBUG_CLONE_CALL
    std::cout << "clone(" << std::hex << flags << ", " << new_stackp << ", " << uptr_parent_tidptr << ", " << uptr_child_tidptr
              << ", " << tls << std::dec << ")\n";
#endif
    if ((flags & CloneSupportedFlags) != flags) {
        std::cerr << "clone: unsupported flags 0x" << std::hex << flags << std::dec << "\n";
        return -EOPNOTSUPP;
    }
    SyscallCtx ctx{params, "SysClone"};
    std::shared_ptr<Process> clonedProcess{};
    if ((flags & ThreadFlags) == 0) {
        clonedProcess = ctx.GetProcess().Clone();
    } else {
        if ((flags & ThreadFlags) != ThreadFlags) {
            std::cerr << "clone: unsupported thread flags 0x" << std::hex << flags << std::dec << "\n";
            return -EOPNOTSUPP;
        }
        clonedProcess = ctx.GetProcess().GetProcess();
    }
    std::shared_ptr<TaskCloner> taskCloner = TaskCloner::Create(clonedProcess, uptr_child_tidptr, flags & CLONE_CHILD_SETTID, flags & CLONE_CHILD_CLEARTID);
    pid_t tid;
    if ((flags & ThreadFlags) == 0) {
        tid = clonedProcess->getpid();
    } else {
        tid = Process::AllocTid();
        taskCloner->SetTid(tid);
    }
    if ((flags & CLONE_SETTLS) != 0) {
        taskCloner->SetTls(tls);
    }
    if (new_stackp != 0) {
        taskCloner->SetStackPtr(new_stackp);
    }
    params.Accept(*taskCloner);
    if ((flags & CLONE_PARENT_SETTID) != 0) {
        return ctx.Write(uptr_parent_tidptr, sizeof(pid_t), [ctx, clonedProcess, taskCloner, tid] (void *parent_tidptr) {
            auto *parent_tid = (pid_t *) parent_tidptr;
            *parent_tid = tid;
            DoClone(ctx.AsyncCtx(), clonedProcess, taskCloner);
            return resolve_return_value::AsyncReturn();
        });
    }
    DoClone(ctx.AsyncCtx(), clonedProcess, taskCloner);
    ctx.AsyncCtx()->async();
    return 0;
}