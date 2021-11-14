//
// Created by sigsegv on 28.05.2021.
//

#include <sstream>
#include <klogger.h>
#include <cpuio.h>
#include <delay.h>
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

#define UHCI_PORT_SUSPENDED         0x1000
#define UHCI_PORT_RESET             0x0200
#define UHCI_PORT_LOW_SPEED         0x0100
#define UHCI_PORT_RESUME_DETECT     0x0040
#define UHCI_PORT_LINE_STATUS_DM    0x0020
#define UHCI_PORT_LINE_STATUS_DP    0x0010
#define UHCI_PORT_ENABLED_CHANGE    0x0008
#define UHCI_PORT_ENABLED           0x0004
#define UHCI_PORT_CONNECTED_CHANGE  0x0002
#define UHCI_PORT_CONNECTED         0x0001

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

    Run();
}

bool uhci::irq() {
    get_klogger() << "UHCI interrupt\n";
    return false;
}

void uhci::dumpregs() {
}

int uhci::GetNumberOfPorts() {
    return 2;
}

uint32_t uhci::GetPortStatus(int port) {
    uint32_t reg = iobase + REG_PORTSC1 + (port * 2);
    uint32_t hcdStatus{USB_PORT_STATUS_PPS};

    uint16_t status = inportw(reg);

    if ((status & UHCI_PORT_SUSPENDED) != 0) {
        hcdStatus |= USB_PORT_STATUS_PSS;
    }
    if ((status & UHCI_PORT_RESET) != 0) {
        hcdStatus |= USB_PORT_STATUS_PRS;
    }
    if ((status & UHCI_PORT_LOW_SPEED) != 0) {
        hcdStatus |= USB_PORT_STATUS_LSDA;
    }
    if ((status & UHCI_PORT_RESUME_DETECT) != 0) {
        hcdStatus |= USB_PORT_STATUS_RESUME;
    }
    if ((status & UHCI_PORT_ENABLED_CHANGE) != 0) {
        hcdStatus |= USB_PORT_STATUS_PESC;
    }
    if ((status & UHCI_PORT_ENABLED) != 0) {
        hcdStatus |= USB_PORT_STATUS_PES;
    }
    if ((status & UHCI_PORT_CONNECTED_CHANGE) != 0) {
        hcdStatus |= USB_PORT_STATUS_CSC;
    }
    if ((status & UHCI_PORT_CONNECTED) != 0) {
        hcdStatus |= USB_PORT_STATUS_CCS;
    }
    return hcdStatus;
}

void uhci::SwitchPortOff(int port) {
}

void uhci::SwitchPortOn(int port) {
}

void uhci::ClearStatusChange(int port, uint32_t statuses) {
    uint32_t reg = iobase + REG_PORTSC1 + (port * 2);
    uint32_t value = inportw(reg);
    value &= ~(UHCI_PORT_CONNECTED_CHANGE | UHCI_PORT_ENABLED_CHANGE);
    if ((statuses & USB_PORT_STATUS_CSC) != 0) {
        value |= UHCI_PORT_CONNECTED_CHANGE;
    }
    if ((statuses & USB_PORT_STATUS_PESC) != 0) {
        value |= UHCI_PORT_ENABLED_CHANGE;
    }
    outportw(reg, value);
}

std::shared_ptr<usb_endpoint>
uhci::CreateControlEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum,
                            usb_endpoint_direction dir, usb_speed speed) {
    return std::shared_ptr<usb_endpoint>();
}

std::shared_ptr<usb_endpoint>
uhci::CreateInterruptEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum,
                              usb_endpoint_direction dir, usb_speed speed, int pollingIntervalMs) {
    return std::shared_ptr<usb_endpoint>();
}

void uhci::EnablePort(int port) {
    uint32_t reg = iobase + REG_PORTSC1 + (port * 2);
    outportw(reg, UHCI_PORT_ENABLED);
}

void uhci::DisablePort(int port) {
    uint32_t reg = iobase + REG_PORTSC1 + (port * 2);
    outportw(reg, 0);
}

void uhci::ResetPort(int port) {
    uint32_t reg = iobase + REG_PORTSC1 + (port * 2);
    {
        uint32_t value = inportw(reg);
        value &= ~(UHCI_PORT_CONNECTED_CHANGE | UHCI_PORT_ENABLED_CHANGE);
        value |= UHCI_PORT_RESET;
        outportw(reg, value);
    }
    delay_nano(50000000);
    {
        uint32_t value = inportw(reg);
        value &= ~(UHCI_PORT_CONNECTED_CHANGE | UHCI_PORT_ENABLED_CHANGE | UHCI_PORT_RESET);
        outportw(reg, value);
    }
}

usb_speed uhci::PortSpeed(int port) {
    uint32_t reg = iobase + REG_PORTSC1 + (port * 2);
    uint32_t value = inportw(reg);
    return (value & UHCI_PORT_LOW_SPEED) == 0 ? FULL : LOW;
}
