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

Syscall::Syscall(SyscallHandler &handler, uint64_t number) : number(number) {
    handler.handlers.push_back(this);
}

SyscallResult SyscallHandler::Call(Interrupt &intr) {
    uint64_t number = intr.rax();
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
            return additionalParams.DoContextSwitch() ? SyscallResult::CONTEXT_SWITCH : SyscallResult::FAST_RETURN;
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
public:
    SyscallHandlerImpl() : SyscallHandler() {
    }
};

static SyscallHandlerImpl syscallHandler{};

SyscallHandler &SyscallHandler::Instance() {
    return syscallHandler;
}
