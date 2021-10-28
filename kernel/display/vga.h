//
// Created by sigsegv on 5/25/21.
//

#ifndef JEOKERNEL_VGA_H
#define JEOKERNEL_VGA_H

#include <devices/devices.h>
#include <devices/drivers.h>
#include "../pci/pci.h"

class vga : public Device {
private:
    PciDeviceInformation pciDeviceInformation;
public:
    vga(Bus *bus, DeviceInformation &deviceInformation) : Device("vga", bus), pciDeviceInformation(*(deviceInformation.GetPciInformation())) {
    }
    void init() override;
};

class vga_driver : public Driver {
public:
    vga_driver() : Driver() {
    }
    virtual Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_VGA_H
