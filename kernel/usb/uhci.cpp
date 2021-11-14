//
// Created by sigsegv on 28.05.2021.
//

#include <sstream>
#include <klogger.h>
#include <cpuio.h>
#include "uhci.h"

#define REG_LEGACY_SUPPORT 0xC0
#define REG_COMMAND        0
#define REG_STATUS         0x02
#define REG_INTR           0x04
#define REG_FRAME_NUMBER   0x06
#define REG_FRAME_BASEADDR 0x08
#define REG_SOFMOD         0x0C
#define REG_PORTSC1        0x10
#define REG_PORTSC2        0x12

#define COMMAND_RUN_STOP   1

#define UHCI_POINTER_TERMINATE 1
#define UHCI_POINTER_QH        2

Device *uhci_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    if (
            deviceInformation.device_class == 0x0C /* serial bus */ &&
            deviceInformation.device_subclass == 3 /* USB */ &&
            deviceInformation.prog_if == 0x00 /* uhci */
    ) {
        return new uhci(bus, *(deviceInformation.GetPciInformation()));
    } else {
        return nullptr;
    }
}

void uhci::init() {
    {
        PciBaseAddressRegister bar4 = pciDeviceInformation.readBaseAddressRegister(4);
        if (!bar4.is_io()) {
            get_klogger() << "USB uhci controller is invalid (invalid BAR4, not ioport)\n";
            return;
        }
        iobase = bar4.addr32();
    }
    outportw(iobase + REG_LEGACY_SUPPORT, 0x8F00); // Clear legacy support bit 0x8000, and clear status trap flags
    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": USB controller iobase 0x" << std::hex << iobase << "\n";
        get_klogger() << msg.str().c_str();
    }

    outportw(iobase + REG_INTR, 0); // Disable intr

    PciRegisterF regF{pciDeviceInformation.readRegF()};
    {
        auto *pci = GetBus()->GetPci();
        std::optional<uint8_t> irq = pci->GetIrqNumber(pciDeviceInformation, regF.InterruptPin - 1);
        {
            std::stringstream msg;
            msg << DeviceType() << (unsigned int) DeviceId() << ": INT pin " << (unsigned int) regF.InterruptPin
                << " PIC INT#" << (unsigned int) regF.InterruptLine << " ACPI INT# " << (irq ? *irq : 0) << "\n";
            get_klogger() << msg.str().c_str();
        }
        if (irq) {
            pci->AddIrqHandler(*irq,[this]() {
                return this->irq();
            });
        }
    }

    qh = qhtdPool.Alloc();
    qh->Pointer()->qh.head = UHCI_POINTER_TERMINATE;
    qh->Pointer()->qh.element = UHCI_POINTER_TERMINATE;
    for (int i = 0; i < 1024; i++) {
        Frames[i] = UHCI_POINTER_QH | qh->Phys();
    }

    outportw(iobase + REG_FRAME_NUMBER, 0);
    outportl(iobase + REG_FRAME_BASEADDR, FramesPhys.PhysAddr());

    outportb(iobase + REG_SOFMOD, 0x40);

    outportw(iobase + REG_STATUS, 0xFFFF); // Clear

    outportw(iobase + REG_COMMAND, COMMAND_RUN_STOP);
}

bool uhci::irq() {
    get_klogger() << "UHCI interrupt\n";
    return false;
}
