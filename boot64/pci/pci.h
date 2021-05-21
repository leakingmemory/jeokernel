//
// Created by sigsegv on 5/20/21.
//

#ifndef JEOKERNEL_PCI_H
#define JEOKERNEL_PCI_H

#include <devices/devices.h>
#include <optional>

struct PciDeviceInformation : public DeviceInformation {
    uint8_t revision_id;
    bool multifunction : 1;
    uint8_t header_type : 7;

    PciDeviceInformation() : DeviceInformation() {
    }

    PciDeviceInformation(const PciDeviceInformation &cp) : PciDeviceInformation() {
        this->operator =(cp);
    }
    PciDeviceInformation(PciDeviceInformation &&) = default;
    PciDeviceInformation & operator = (PciDeviceInformation &&) = default;
    PciDeviceInformation & operator = (const PciDeviceInformation &cp) = default;


    PciDeviceInformation *GetPciInformation() override;
};

class pci : public Bus {
private:
    uint16_t bus;
public:
    pci(uint16_t bus) : Bus("pci"), bus(bus) {
    }
    virtual void ProbeDevices() override;
    std::optional<PciDeviceInformation> probeDevice(uint8_t addr);
};

void detect_root_pcis();

#endif //JEOKERNEL_PCI_H
