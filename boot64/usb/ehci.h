//
// Created by sigsegv on 28.05.2021.
//

#ifndef JEOKERNEL_EHCI_H
#define JEOKERNEL_EHCI_H

#include <devices/drivers.h>
#include "../pci/pci.h"

class ehci : public Device {
private:
    PciDeviceInformation pciDeviceInformation;
public:
    ehci(Bus &bus, PciDeviceInformation &deviceInformation) : Device("ehci", &bus), pciDeviceInformation(deviceInformation) {}
    void init() override;
};

class ehci_driver : public Driver {
public:
    ehci_driver() : Driver() {}
    virtual Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_EHCI_H
