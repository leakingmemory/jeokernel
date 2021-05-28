//
// Created by sigsegv on 26.05.2021.
//

#include <sstream>
#include <klogger.h>
#include "pci_bridge.h"

Device *pci_bridge_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    if (
            deviceInformation.device_class == 6/*bridge*/ &&
            deviceInformation.device_subclass == 4/*pci-pci*/ &&
            deviceInformation.GetPciInformation() != nullptr
    ) {
        PciDeviceInformation &pciDeviceInformation = *(deviceInformation.GetPciInformation());
        if (pciDeviceInformation.header_type == 1 /* pci-pci header*/) {
            return new pci_bridge(bus, pciDeviceInformation);
        }
    }
    return nullptr;
}

void pci_bridge::init() {
    uint32_t reg_06 = read_pci_config(pciDeviceInformation.bus, pciDeviceInformation.slot, pciDeviceInformation.func, 0x18);
    uint8_t primary_bus = (uint8_t) (reg_06 & 0x000000FF);
    reg_06 = reg_06 >> 8;
    uint8_t secondary_bus = (uint8_t) (reg_06 & 0x000000FF);
    reg_06 = reg_06 >> 8;
    uint8_t subordinate_bus = (uint8_t) (reg_06 & 0x000000FF);
    std::stringstream msg;
    msg << DeviceType() << (unsigned int) DeviceId() << ": " << (unsigned int) primary_bus << " -> " << (unsigned int) secondary_bus << " - " << (unsigned int) subordinate_bus << "\n";
    get_klogger() << msg.str().c_str();
#ifndef USE_APIC_FOR_PCI_BUSES
    for (uint16_t bus = secondary_bus; bus <= subordinate_bus; bus++) {
        pci *pcibus = new pci(bus);
        devices().add(*pcibus);
        std::stringstream msg{};
        msg << pcibus->DeviceType() << (unsigned int) pcibus->DeviceId() << ": sub bus\n";
        get_klogger() << msg.str().c_str();
        pcibus->ProbeDevices();
    }
#endif
}
