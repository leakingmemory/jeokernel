//
// Created by sigsegv on 03.05.2021.
//

#include <klogger.h>
#include <concurrency/critical_section.h>
#include <mutex>
#include "CpuInterrupts.h"

uint64_t CpuInterrupts::add_handler(uint8_t intn, std::function<void(Interrupt &)> func) {
    std::lock_guard lock{_lock};

    uint64_t serial = this->serial++;
    std::tuple<uint64_t,std::function<void(Interrupt &)>> record = std::make_tuple<uint64_t,std::function<void (Interrupt &)>>(serial,func);
    handlers[intn].push_back(record);
    return serial;
}

void CpuInterrupts::remove_handler(uint64_t id) {
    std::lock_guard lock{_lock};

    for (int i = 0; i < CPU_INT_N; i++) {
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

bool CpuInterrupts::handle(Interrupt &interrupt) {
    std::vector<std::function<void (Interrupt &)>> call_order{};
    bool handled = false;

    {
        std::lock_guard lock{_lock};

        for (const std::tuple<uint64_t, std::function<void(Interrupt &)>> &handler : handlers[
                interrupt.interrupt_vector()]) {
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

static CpuInterrupts *cpu_interrupts = nullptr;

void create_cpu_interrupt_handler() {
    cpu_interrupts = new CpuInterrupts;
}

CpuInterrupts &get_cpu_interrupt_handler() {
    return *cpu_interrupts;
}
