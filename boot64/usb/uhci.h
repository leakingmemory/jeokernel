//
// Created by sigsegv on 28.05.2021.
//

#ifndef JEOKERNEL_UHCI_H
#define JEOKERNEL_UHCI_H

#include <devices/drivers.h>
#include "../pci/pci.h"

class uhci : public Device {
private:
    PciDeviceInformation pciDeviceInformation;
    uint32_t iobase;
public:
    uhci(Bus &bus, PciDeviceInformation &deviceInformation) : Device("uhci", &bus), pciDeviceInformation(deviceInformation) {}
    void init() override;
};

class uhci_driver : public Driver {
public:
    uhci_driver() : Driver() {}
    virtual Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_UHCI_H
