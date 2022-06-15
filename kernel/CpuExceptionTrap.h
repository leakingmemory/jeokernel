//
// Created by sigsegv on 30.04.2021.
//

#ifndef JEOKERNEL_CPUEXCEPTIONTRAP_H
#define JEOKERNEL_CPUEXCEPTIONTRAP_H


class CpuExceptionTrap {
private:
    /* Careful with lifecycle, invalid outside exception stack frame */
    Interrupt &interrupt;
    const char *fault_text;
public:
    CpuExceptionTrap() = delete;
    CpuExceptionTrap(const char *fault_text, Interrupt &interrupt) : fault_text(fault_text), interrupt(interrupt) {
    }
    CpuExceptionTrap(CpuExceptionTrap &&) = delete;
    CpuExceptionTrap(CpuExceptionTrap &) = delete;
    CpuExceptionTrap &operator = (CpuExceptionTrap &&) = delete;
    CpuExceptionTrap &operator = (CpuExceptionTrap &) = delete;

    bool handle(bool errorCode) {
        if (errorCode ? (interrupt.error_code() & 4) != 0 : (interrupt.cs() & 3) == 3) {
            auto *scheduler = get_scheduler();
            std::string name{fault_text};
            if (scheduler->exception(name, interrupt)) {
                return true;
            }
        }
        interrupt.print_debug();
        wild_panic(fault_text);
        return false;
    }
};


#endif //JEOKERNEL_CPUEXCEPTIONTRAP_H
