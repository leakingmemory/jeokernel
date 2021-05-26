//
// Created by sigsegv on 26.05.2021.
//

#ifndef JEOKERNEL_PCI_BRIDGE_H
#define JEOKERNEL_PCI_BRIDGE_H

#include <devices/drivers.h>
#include "pci.h"

class pci_bridge : public Device {
private:
    PciDeviceInformation pciDeviceInformation;
public:
    pci_bridge(Bus &bus, const PciDeviceInformation &deviceInformation) : Device("pcibridge", &bus), pciDeviceInformation(deviceInformation) {}
    void init() override;
};

class pci_bridge_driver : public Driver {
public:
    pci_bridge_driver() : Driver() {}
    virtual Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_PCI_BRIDGE_H
