//
// Created by sigsegv on 8/10/21.
//

#include <sstream>
#include <klogger.h>
#include "pci.h"
#include "../HardwareInterrupts.h"

pci_irq::pci_irq(pci &bus, const IRQLink &link, int index) : pci_irq(bus, (link.Interrupts.data())[index]) {
    /* int pin number + 3 local apic ints below ioapic range */
    get_hw_interrupt_handler().add_handler(irq + 3, [this] (Interrupt &intr) {
        interrupt(intr);
        this->bus.Ioapic().send_eoi(irq);
        this->bus.Lapic().eio();
    });
    bus.Ioapic().enable(irq, this->bus.Lapic().get_cpu_num(this->bus.Mpfp()),
                        true, false, 0, link.Polarity, link.Triggering);
    bus.Ioapic().send_eoi(irq);
}

bool pci_irq::invoke(Interrupt &intr) {
    bool hit{false};
    {
        std::lock_guard _lock(lock);
        for (auto &h : handlers) {
            if (h()) {
                hit = true;
            }
        }
    }
    if (!hit) {
        std::stringstream msg{};
        msg << "IRQ " << irq << "\n";
        get_klogger() << msg.str().c_str();
    }
    return hit;
}

void pci_irq::interrupt(Interrupt &intr) {
    invoke(intr);
}

void pci_irq::add_handler(const std::function<bool()> &h) {
    std::lock_guard _guard(lock);
    handlers.push_back(h);
}