//
// Created by sigsegv on 6/18/22.
//

#include <errno.h>
#include "interrupt_frame.h"
#include "SyscallHandler.h"
#include "Exit.h"

Syscall::Syscall(SyscallHandler &handler, uint64_t number) : number(number) {
    handler.handlers.push_back(this);
}

SyscallResult SyscallHandler::Call(Interrupt &intr) {
    uint64_t number = intr.rax();
    for (auto *handler : handlers) {
        if (handler->number == number) {
            SyscallAdditionalParams additionalParams{(int64_t) intr.r8(), (int64_t) intr.r9()};
            uint64_t result = (uint64_t) handler->Call((int64_t) intr.rdi(), (int64_t) intr.rsi(), (int64_t) intr.rdx(), (int64_t) intr.r10(), additionalParams);
            intr.get_cpu_state().rax = result;
            return additionalParams.DoContextSwitch() ? SyscallResult::CONTEXT_SWITCH : SyscallResult::FAST_RETURN;
        }
    }
    intr.get_cpu_state().rax = (uint64_t) (-ENOSYS);
    return SyscallResult::NOT_A_SYSCALL;
}

class SyscallHandlerImpl : public SyscallHandler {
private:
    Exit exit{*this};
public:
    SyscallHandlerImpl() : SyscallHandler() {
    }
};

static SyscallHandlerImpl syscallHandler{};

SyscallHandler &SyscallHandler::Instance() {
    return syscallHandler;
}
