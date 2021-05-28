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
    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": USB controller\n";
        get_klogger() << msg.str().c_str();
    }
}
