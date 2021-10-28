//
// Created by sigsegv on 28.05.2021.
//

#include <sstream>
#include <klogger.h>
#include <cpuio.h>
#include "uhci.h"

#define REG_LEGACY_SUPPORT 0xC0

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
}
