//
// Created by sigsegv on 6/18/22.
//

#include <errno.h>
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

//#define SYSCALL_DEBUG

#ifdef SYSCALL_DEBUG
#include <iostream>
#endif

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
            SyscallAdditionalParams additionalParams{(int64_t) intr.r8(), (int64_t) intr.r9()};
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
#ifdef SYSCALL_DEBUG
                std::cout << "Syscall fast returns\n";
#endif
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
    Futex futex{*this};
    Access access{*this};
    OpenAt openat{*this};
    Read read{*this};
    Close close{*this};
    Pread64 pread64{*this};
    Gettimeofday gettimeofday{*this};
    Ioctl ioctl{*this};
public:
    SyscallHandlerImpl() : SyscallHandler() {
    }
};

static SyscallHandlerImpl syscallHandler{};

SyscallHandler &SyscallHandler::Instance() {
    return syscallHandler;
}
