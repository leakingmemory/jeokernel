//
// Created by sigsegv on 28.05.2021.
//

#include <sstream>
#include <klogger.h>
#include "ehci.h"

Device *ehci_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    if (
            deviceInformation.device_class == 0x0C /* serial bus */ &&
            deviceInformation.device_subclass == 3 /* USB */ &&
            deviceInformation.prog_if == 0x20 /* ehci */
    ) {
        return new ehci(bus, *(deviceInformation.GetPciInformation()));
    } else {
        return nullptr;
    }
}

void ehci::init() {
    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": USB2 controller\n";
        get_klogger() << msg.str().c_str();
    }
}
