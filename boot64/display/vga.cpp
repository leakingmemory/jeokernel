//
// Created by sigsegv on 5/25/21.
//

#include <klogger.h>
#include <sstream>
#include "vga.h"

Device *vga_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    if (deviceInformation.device_class == 0x03 && deviceInformation.device_subclass == 0 &&
        deviceInformation.GetPciInformation() != nullptr) {
        return new vga(&bus, deviceInformation);
    }
    return nullptr;
}

void vga::init() {
    std::stringstream msg{};
    msg << DeviceType() << (unsigned int) DeviceId() << ": vga compatible\n";
    get_klogger() << msg.str().c_str();
}
