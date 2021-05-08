//
// Created by sigsegv on 03.05.2021.
//

#include <klogger.h>
#include <concurrency/critical_section.h>
#include <mutex>
#include "HardwareInterrupts.h"

uint64_t HardwareInterrupts::add_handler(uint8_t intn, std::function<void(Interrupt &)> func) {
    critical_section cli{};
    std::lock_guard lock{_lock};

    uint64_t serial = this->serial++;
    std::tuple<uint64_t,std::function<void(Interrupt &)>> record = std::make_tuple<uint64_t,std::function<void (Interrupt &)>>(serial,func);
    handlers[intn].push_back(record);
    return serial;
}

void HardwareInterrupts::remove_handler(uint64_t id) {
    critical_section cli{};
    std::lock_guard lock{_lock};

    for (int i = 0; i < HW_INT_N; i++) {
        auto iterator = handlers[i].begin();
        while (iterator != handlers[i].end()) {
            if (std::get<0>(*iterator) == id) {
                handlers[i].erase(iterator);
                return;
            }
            ++iterator;
        }
    }
}

uint64_t HardwareInterrupts::add_finalizer(uint8_t intn, std::function<void(Interrupt &)> func) {
    critical_section cli{};
    std::lock_guard lock{_lock};

    uint64_t serial = this->serial++;
    std::tuple<uint64_t,std::function<void(Interrupt &)>> record = std::make_tuple<uint64_t,std::function<void (Interrupt &)>>(serial,func);
    finalizers[intn].push_back(record);
    return serial;
}

void HardwareInterrupts::remove_finalizer(uint64_t id) {
    critical_section cli{};
    std::lock_guard lock{_lock};

    for (int i = 0; i < HW_INT_N; i++) {
        auto iterator = finalizers[i].begin();
        while (iterator != finalizers[i].end()) {
            if (std::get<0>(*iterator) == id) {
                finalizers[i].erase(iterator);
                return;
            }
            ++iterator;
        }
    }
}

bool HardwareInterrupts::handle(Interrupt &interrupt) {
    std::vector<std::function<void (Interrupt &)>> call_order{};
    bool handled = true;

    {
        std::lock_guard lock{_lock};

        for (const std::tuple<uint64_t, std::function<void(Interrupt &)>> &handler : handlers[
                interrupt.interrupt_vector() - 0x20]) {
            std::function<void(Interrupt &)> callable = std::get<1>(handler);
            call_order.push_back(callable);
            handled = true;
        }
        for (const std::tuple<uint64_t, std::function<void(Interrupt &)>> &handler : finalizers[
                interrupt.interrupt_vector() - 0x20]) {
            std::function<void(Interrupt &)> callable = std::get<1>(handler);
            call_order.push_back(callable);
            handled = true;
        }
    }

    for (std::function<void(Interrupt &)> &callable : call_order) {
        callable(interrupt);
    }

    return handled;
}

static HardwareInterrupts *hw_interrupts = nullptr;

void create_hw_interrupt_handler() {
    hw_interrupts = new HardwareInterrupts;
}

HardwareInterrupts &get_hw_interrupt_handler() {
    return *hw_interrupts;
}
