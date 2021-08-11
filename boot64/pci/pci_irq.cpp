//
// Created by sigsegv on 8/10/21.
//

#include <sstream>
#include <klogger.h>
#include "pci.h"
#include "../HardwareInterrupts.h"

pci_irq::pci_irq(pci &bus, const IRQLink &link, int index) : pci_irq(bus, (link.Interrupts.data())[index]) {
    get_hw_interrupt_handler().add_handler(irq + 3, [this] (Interrupt &intr) {
        interrupt(intr);
        this->bus.Ioapic().send_eoi(irq);
        this->bus.Lapic().eio();
    });
    bus.Ioapic().enable(irq, this->bus.Lapic().get_cpu_num(this->bus.Mpfp()),
                        true, false, 0, link.Polarity, link.Triggering);
    bus.Ioapic().send_eoi(irq);
}

bool pci_irq::invoke() {
    {
        std::stringstream msg{};
        msg << "IRQ " << irq << "\n";
        get_klogger() << msg.str().c_str();
    }
    bool hit{false};
    for (auto &h : handlers) {
        if (h()) {
            hit = true;
        }
    }
    return hit;
}

void pci_irq::interrupt(Interrupt &intr) {
    std::stringstream msg{};
    msg << "PCI IRQ " << irq << "\n";
    get_klogger() << msg.str().c_str();


}