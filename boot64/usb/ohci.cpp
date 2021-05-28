//
// Created by sigsegv on 28.05.2021.
//

#include <sstream>
#include <klogger.h>
#include "ohci.h"

Device *ohci_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    if (
            deviceInformation.device_class == 0x0C /* serial bus */ &&
            deviceInformation.device_subclass == 3 /* USB */ &&
            deviceInformation.prog_if == 0x10 /* ohci */
    ) {
        return new ohci(bus, *(deviceInformation.GetPciInformation()));
    } else {
        return nullptr;
    }
}

void ohci::init() {
    uint64_t addr{0};
    uint64_t size{0};
    {
        PciBaseAddressRegister bar4 = pciDeviceInformation.readBaseAddressRegister(0);
        if (!bar4.is_memory()) {
            get_klogger() << "USB ohci controller is invalid (invalid BAR0, not memory mapped)\n";
            return;
        }
        if (bar4.is_64bit()) {
            addr = bar4.addr64();
        } else if (bar4.is_32bit()) {
            addr = (uint32_t) bar4.addr32();
        } else {
            get_klogger() << "USB ohci controller is invalid (invalid BAR0, invalid record)\n";
            return;
        }
        size = bar4.memory_size();
    }
    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": USB controller, memory registers 0x" << std::hex << addr << " size " << std::dec << size << "\n";
        get_klogger() << msg.str().c_str();
    }
}
