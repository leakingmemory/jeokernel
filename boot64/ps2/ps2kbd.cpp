//
// Created by sigsegv on 6/6/21.
//

#include <sstream>
#include <klogger.h>
#include <core/cpu_mpfp.h>
#include <core/LocalApic.h>
#include "ps2kbd.h"
#include "../HardwareInterrupts.h"

void ps2kbd::init() {
    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": Init\n";
        get_klogger() << msg.str().c_str();
    }
    ps2dev.EnableIrq();
    cpu_mpfp *mpfp = get_mpfp();
    uint8_t ioapic_intn{ps2dev.IrqNum()};
    {
        uint8_t isa_bus_id{0xFF};
        for (int i = 0; i < mpfp->get_num_bus(); i++) {
            const mp_bus_entry &bus = mpfp->get_bus(i);
            if (bus.bus_type[0] == 'I' && bus.bus_type[1] == 'S' && bus.bus_type[2] == 'A') {
                isa_bus_id = bus.bus_id;
            }
        }
        {
            std::stringstream msg;
            msg << DeviceType() << (unsigned int) DeviceId() << ": ISA Bus ID " << (unsigned int) isa_bus_id << "\n";
            get_klogger() << msg.str().c_str();
        }
        for (int i = 0; i < mpfp->get_num_ioapic_ints(); i++) {
            const mp_ioapic_interrupt_entry &ioapic_int = mpfp->get_ioapic_int(i);
            if (ioapic_int.source_bus_irq == ps2dev.IrqNum() && ioapic_int.source_bus_id == isa_bus_id) {
                ioapic_intn = ioapic_int.ioapic_intin;

                std::stringstream msg;
                msg << DeviceType() << (unsigned int) DeviceId() << ": IRQ bus "
                    << (unsigned int) ioapic_int.source_bus_id << " "
                    << (unsigned int) ioapic_int.source_bus_irq << " -> "
                    << (unsigned int) ioapic_int.ioapic_intin << " flags "
                    << (unsigned int) ioapic_int.interrupt_flags << "\n";
                get_klogger() << msg.str().c_str();
            }
        }
    }

    lapic = std::make_unique<LocalApic>(*mpfp);
    ioapic = std::make_unique<IOApic>(*mpfp);

    get_hw_interrupt_handler().add_handler(4, [this, ioapic_intn] (Interrupt &intr) {
        get_klogger() << "Keyboard interrupt! ";
        ps2dev.Bus().input();
        ioapic->send_eoi(ioapic_intn);
        lapic->eio();
        get_klogger() << "Keyboard proceed\n";
    });

    ioapic->enable(ioapic_intn, lapic->get_cpu_num(*mpfp));
    ioapic->send_eoi(ioapic_intn);
}
