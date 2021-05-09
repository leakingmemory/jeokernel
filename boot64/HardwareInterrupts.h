//
// Created by sigsegv on 03.05.2021.
//

#ifndef JEOKERNEL_HARDWAREINTERRUPTS_H
#define JEOKERNEL_HARDWAREINTERRUPTS_H

#include <cstdint>
#include <tuple>
#include <vector>
#include <functional>
#include <interrupt_frame.h>
#include <core/LocalApic.h>

#define HW_INT_N 0x30

class HardwareInterrupts {
private:
    hw_spinlock _lock;
    uint64_t serial;
    std::vector<std::tuple<uint64_t,std::function<void (Interrupt &)>>> handlers[HW_INT_N];
    std::vector<std::tuple<uint64_t,std::function<void (Interrupt &)>>> finalizers[HW_INT_N];
public:
    HardwareInterrupts() : _lock(), serial(0), handlers(), finalizers() {}
    uint64_t add_handler(uint8_t intn, std::function<void (Interrupt &)> func);
    void remove_handler(uint64_t id);
    uint64_t add_finalizer(uint8_t intn, std::function<void (Interrupt &)> func);
    void remove_finalizer(uint64_t id);
    bool handle(Interrupt &interrupt);
};

void create_hw_interrupt_handler();
HardwareInterrupts &get_hw_interrupt_handler();

#endif //JEOKERNEL_HARDWAREINTERRUPTS_H
