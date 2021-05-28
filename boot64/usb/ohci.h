//
// Created by sigsegv on 28.05.2021.
//

#ifndef JEOKERNEL_OHCI_H
#define JEOKERNEL_OHCI_H

#include <devices/drivers.h>
#include "../pci/pci.h"

class ohci : public Device {
private:
    PciDeviceInformation pciDeviceInformation;
public:
    ohci(Bus &bus, PciDeviceInformation &deviceInformation) : Device("ohci", &bus), pciDeviceInformation(deviceInformation) {}
    void init() override;
};

class ohci_driver : public Driver {
public:
    ohci_driver() : Driver() {}
    virtual Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_OHCI_H
