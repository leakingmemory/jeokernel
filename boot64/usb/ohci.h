//
// Created by sigsegv on 28.05.2021.
//

#ifndef JEOKERNEL_OHCI_H
#define JEOKERNEL_OHCI_H

#include <devices/drivers.h>
#include <core/vmem.h>
#include "../pci/pci.h"

#define OHCI_CTRL_IR     0x0100

#define OHCI_CTRL_HCFS_MASK 0x00C0
#define OHCI_CTRL_HCFS_RESET       0
#define OHCI_CTRL_HCFS_RESUME      (1 << 6)
#define OHCI_CTRL_HCFS_OPERATIONAL (2 << 6)
#define OHCI_CTRL_HCFS_SUSPEND     (3 << 6)


struct ohci_registers {
    uint32_t HcRevision;
    uint32_t HcControl;
    uint32_t HcCommandStatus;
    uint32_t HcInterruptStatus;
    uint32_t HcInterruptEnable;
    uint32_t HcInterruptDisable;
    uint32_t HcHCCA;
    uint32_t HcPeriodCurrentED;
    uint32_t HcControlHeadED;
    uint32_t HcControlCurrentED;
    uint32_t HcBulkHeadED;
    uint32_t HcBulkCurrentED;
    uint32_t HcDoneHead;
    uint32_t HcFmInterval;
    uint32_t HcFmRemaining;
    uint32_t HcFmNumber;
    uint32_t HcPeriodicStart;
    uint32_t HcLSThreshold;
    uint32_t HcRhDescriptorA;
    uint32_t HcRhDescriptorB;
    uint32_t HcRhStatus;
    uint32_t PortStatus[1];
} __attribute__((__packed__));
static_assert(sizeof(ohci_registers) == 0x58);

class ohci : public Device {
private:
    PciDeviceInformation pciDeviceInformation;
    std::unique_ptr<vmem> mapped_registers_vm;
    ohci_registers *ohciRegisters;
public:
    ohci(Bus &bus, PciDeviceInformation &deviceInformation) : Device("ohci", &bus), pciDeviceInformation(deviceInformation), mapped_registers_vm(), ohciRegisters(nullptr) {}
    void init() override;
};

class ohci_driver : public Driver {
public:
    ohci_driver() : Driver() {}
    virtual Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_OHCI_H
