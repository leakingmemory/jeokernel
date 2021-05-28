//
// Created by sigsegv on 28.05.2021.
//

#ifndef JEOKERNEL_XHCI_H
#define JEOKERNEL_XHCI_H

#include <devices/drivers.h>
#include "../pci/pci.h"

class xhci : public Device {
private:
    PciDeviceInformation pciDeviceInformation;
public:
    xhci(Bus &bus, PciDeviceInformation &deviceInformation) : Device("xhci", &bus), pciDeviceInformation(deviceInformation) {}
    void init() override;
};

class xhci_driver : public Driver {
public:
    xhci_driver() : Driver() {}
    virtual Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_XHCI_H
