//
// Created by sigsegv on 03.05.2021.
//

#ifndef JEOKERNEL_CPUINTERRUPTS_H
#define JEOKERNEL_CPUINTERRUPTS_H

#include <cstdint>
#include <tuple>
#include <vector>
#include <functional>
#include <interrupt_frame.h>
#include <core/LocalApic.h>

#define CPU_INT_N 0x20

class CpuInterrupts {
private:
    hw_spinlock _lock;
    uint64_t serial;
    std::vector<std::tuple<uint64_t,std::function<void (Interrupt &)>>> handlers[CPU_INT_N];
public:
    CpuInterrupts() : _lock(), serial(0), handlers() {}
    uint64_t add_handler(uint8_t intn, std::function<void (Interrupt &)> func);
    void remove_handler(uint64_t id);
    bool handle(Interrupt &interrupt);
};

void create_cpu_interrupt_handler();
CpuInterrupts &get_cpu_interrupt_handler();

#endif //JEOKERNEL_CPUINTERRUPTS_H
