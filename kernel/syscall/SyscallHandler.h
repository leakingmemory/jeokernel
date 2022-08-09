//
// Created by sigsegv on 6/18/22.
//

#ifndef JEOKERNEL_SYSCALLHANDLER_H
#define JEOKERNEL_SYSCALLHANDLER_H

#include <cstdint>
#include <vector>
#include <functional>

class Interrupt;

class SyscallAdditionalParams {
private:
    int64_t param5, param6;
    std::function<void (Interrupt &)> modifyCpuState;
    bool doContextSwitch;
public:
    SyscallAdditionalParams(int64_t param5, int64_t param6) : param5(param5), param6(param6), modifyCpuState(), doContextSwitch(false) {
    }
    int64_t Param5() const {
        return param5;
    }
    int64_t Param6() const {
        return param6;
    }
    bool DoContextSwitch() const {
        return doContextSwitch;
    }
    void DoContextSwitch(bool doContextSwitch) {
        this->doContextSwitch = doContextSwitch;
    }
    bool DoModifyCpuState() const noexcept {
        if (modifyCpuState) {
            return true;
        } else {
            return false;
        }
    }
    void ModifyCpuState(Interrupt &intr) {
        modifyCpuState(intr);
    }
    void ModifyCpuState(const std::function<void (Interrupt &)> &func) {
        modifyCpuState = func;
    }
};

class SyscallHandler;

class Syscall {
    friend SyscallHandler;
private:
    uint64_t number;
public:
    Syscall(SyscallHandler &handler, uint64_t number);
    virtual int64_t Call(int64_t, int64_t, int64_t, int64_t, SyscallAdditionalParams &) = 0;
};

class SyscallHandlerImpl;
class Interrupt;

enum class SyscallResult {
    FAST_RETURN,
    CONTEXT_SWITCH,
    NOT_A_SYSCALL
};

class SyscallHandler {
    friend Syscall;
    friend SyscallHandlerImpl;
private:
    std::vector<Syscall *> handlers;
    SyscallHandler() : handlers() {
    }
public:
    SyscallResult Call(Interrupt &);
    static SyscallHandler &Instance();
};

#endif //JEOKERNEL_SYSCALLHANDLER_H
