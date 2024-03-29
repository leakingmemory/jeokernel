//
// Created by sigsegv on 6/18/22.
//

#include <errno.h>
#include <core/scheduler.h>
#include "interrupt_frame.h"
#include "SyscallHandler.h"
#include "Exit.h"
#include "Write.h"
#include "Geteuid.h"
#include "Getegid.h"
#include "Getuid.h"
#include "Getgid.h"
#include "Brk.h"
#include "ArchPrctl.h"
#include "SetTidAddress.h"
#include "SetRobustList.h"
#include "Uname.h"
#include "Writev.h"
#include "Mmap.h"
#include "Sigprocmask.h"
#include "PrLimit64.h"
#include "Readlink.h"
#include "Getrandom.h"
#include "ClockGettime.h"
#include "Mprotect.h"
#include "Newfstatat.h"
#include "ExitGroup.h"
#include "Rseq.h"
#include "Futex.h"
#include "Access.h"
#include "OpenAt.h"
#include "Read.h"
#include "Close.h"
#include "Pread64.h"
#include "Gettimeofday.h"
#include "Ioctl.h"
#include "Sysinfo.h"
#include "RtSigaction.h"
#include "Getcwd.h"
#include "Getpid.h"
#include "Getppid.h"
#include "Getpgrp.h"
#include "Dup.h"
#include "Fcntl.h"
#include "Dup2.h"
#include "Time.h"
#include "Pselect6.h"
#include "Setpgid.h"
#include "Clone.h"
#include "Wait4.h"
#include "Execve.h"
#include "Prctl.h"
#include "Getdents64.h"
#include "Statx.h"
#include "Chdir.h"
#include "Munmap.h"
#include "Getxattr.h"
#include "SchedGetaffinity.h"
#include "Fstat.h"
#include "Fadvise64.h"
#include "Lseek.h"
#include "Mkdir.h"
#include "Mount.h"
#include "Open.h"
#include "Getpgid.h"
#include "Stat.h"
#include "Fork.h"
#include "Gettid.h"
#include "Readv.h"
#include "Faccessat2.h"
#include "Membarrier.h"
#include "Nanosleep.h"
#include "Sigaltstack.h"
#include "Pipe.h"
#include "Sigreturn.h"
#include "Kill.h"

#include <exec/procthread.h>
#include <iostream>

//#define SYSCALL_DEBUG

#ifdef SYSCALL_DEBUG
#include <iostream>
#endif

SyscallAdditionalParams::SyscallAdditionalParams(int64_t param5, int64_t param6, const std::function<void(
        SyscallInterruptFrameVisitor &)> &accessInterruptFrame) : param5(param5), param6(param6), accessInterruptFrame(accessInterruptFrame), modifyCpuState(), scheduler(get_scheduler()), currentTask(nullptr), currentThread(nullptr), taskId(0), doContextSwitch(false) {
    currentTask = &(scheduler->get_current_task());
    currentThread = scheduler->get_resource<ProcThread>(*currentTask);
    taskId = scheduler->get_current_task_id();
}

Syscall::Syscall(SyscallHandler &handler, uint64_t number) : number(number) {
    handler.handlers.push_back(this);
}

SyscallResult SyscallHandler::Call(Interrupt &intr) {
    uint64_t number = intr.rax();
#ifdef SYSCALL_DEBUG
    std::cout << "syscall " << std::dec << number << "(0x" << std::hex << intr.rdi() << ", 0x" << intr.rsi()
              << ", 0x" << intr.rdx() << ", 0x" << intr.r10() << ", 0x" << intr.r8() << ", 0x" << intr.r9()
              << std::dec << ")\n";
#endif
    for (auto *handler : handlers) {
        if (handler->number == number) {
            SyscallAdditionalParams additionalParams{(int64_t) intr.r8(), (int64_t) intr.r9(), [&intr] (SyscallInterruptFrameVisitor &visitor) {
                visitor.VisitInterruptFrame(intr);
            }};
            additionalParams.CurrentThread()->SetSyscallNumber((int) number);
            intr.get_cpu_state().rax = 0;
            uint64_t result = (uint64_t) handler->Call((int64_t) intr.rdi(), (int64_t) intr.rsi(), (int64_t) intr.rdx(), (int64_t) intr.r10(), additionalParams);
            if (result != 0) {
                intr.get_cpu_state().rax = result;
            }
            if (additionalParams.DoModifyCpuState()) {
                additionalParams.ModifyCpuState(intr);
            }
            if (additionalParams.DoContextSwitch()) {
#ifdef SYSCALL_DEBUG
                std::cout << "Syscall returns with context switch\n";
#endif
                return SyscallResult::CONTEXT_SWITCH;
            } else {
#ifdef DEBUG_SYSCALL_PFAULT_ASYNC_BUGS
                auto *scheduler = get_scheduler();
                task *current_task = &(scheduler->get_current_task());
                auto *process = scheduler->get_resource<ProcThread>(*current_task);
                if (process->IsThreadFaulted()) {
                    std::cerr << "Error: Syscall " << std::dec << number << " returns with unresolved fault\n";
                    current_task->set_blocked(true);
                    return SyscallResult::CONTEXT_SWITCH;
                }
#endif
#ifdef SYSCALL_DEBUG
                std::cout << "Syscall fast returns\n";
#endif
                if (callctx::HandleSignalInFastReturn(intr)) {
                    return SyscallResult::CONTEXT_SWITCH;
                }
                return SyscallResult::FAST_RETURN;
            }
        }
    }
    intr.get_cpu_state().rax = (uint64_t) (-ENOSYS);
    return SyscallResult::NOT_A_SYSCALL;
}

class SyscallHandlerImpl : public SyscallHandler {
private:
    Exit exit{*this};
    Write write{*this};
    Geteuid geteuid{*this};
    Getegid getegid{*this};
    Getuid getuid{*this};
    Getgid getgid{*this};
    Brk brk{*this};
    ArchPrctl archPrctl{*this};
    SetTidAddress setTidAddress{*this};
    SetRobustList setRobustList{*this};
    Uname uname{*this};
    Writev writev{*this};
    Mmap mmap{*this};
    Sigprocmask sigprocmask{*this};
    PrLimit64 prLimit64{*this};
    Readlink readlink{*this};
    Getrandom getrandom{*this};
    ClockGettime clockGettime{*this};
    Mprotect mprotect{*this};
    Newfstatat newfstatat{*this};
    ExitGroup exitGroup{*this};
    Rseq rseq{*this};
    SysFutex futex{*this};
    Access access{*this};
    OpenAt openat{*this};
    Read read{*this};
    Close close{*this};
    Pread64 pread64{*this};
    Gettimeofday gettimeofday{*this};
    Ioctl ioctl{*this};
    Sysinfo sysinfo{*this};
    RtSigaction rtSigaction{*this};
    Getcwd getcwd{*this};
    Getpid getpid{*this};
    Getppid getppid{*this};
    Getpgrp getpgrp{*this};
    Dup dup{*this};
    Fcntl fcntl{*this};
    Dup2 dup2{*this};
    Time time{*this};
    Pselect6 pselect6{*this};
    Setpgid setpgid{*this};
    Clone clone{*this};
    Wait4 wait4{*this};
    Execve execve{*this};
    Prctl prctl{*this};
    Getdents64 getdents64{*this};
    Statx statx{*this};
    Chdir chdir{*this};
    Munmap munmap{*this};
    Getxattr getxattr{*this};
    SchedGetaffinity getaffinity{*this};
    Fstat fstat{*this};
    Fadvise64 fadvise64{*this};
    Lseek lseek{*this};
    Mkdir mkdir{*this};
    Mount mount{*this};
    Open open{*this};
    Getpgid getpgid{*this};
    Stat stat{*this};
    Fork fork{*this};
    Gettid gettid{*this};
    Readv readv{*this};
    Faccessat2 faccessat2{*this};
    Membarrier membarrier{*this};
    Nanosleep nanosleep{*this};
    Sigaltstack sigaltstack{*this};
    Pipe pipe{*this};
    Sigreturn sigreturn{*this};
    Kill kill{*this};
public:
    SyscallHandlerImpl() : SyscallHandler() {
    }
};

static SyscallHandlerImpl syscallHandler{};

SyscallHandler &SyscallHandler::Instance() {
    return syscallHandler;
}
