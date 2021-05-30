//
// Created by sigsegv on 28.05.2021.
//

#ifndef JEOKERNEL_EHCI_H
#define JEOKERNEL_EHCI_H

#include <devices/drivers.h>
#include <core/vmem.h>
#include "../pci/pci.h"

struct ehci_regs {
    uint32_t usbCommand;
    uint32_t usbStatus;
    uint32_t usbInterruptEnable;
    uint32_t frameIndex;
    uint32_t ctrlDsSegment;
    uint32_t periodicListBase;
    uint64_t asyncListAddr;
    uint32_t configFlag;
    uint32_t portStatusControl[1];
};

struct ehci_capabilities {
    uint8_t caplength;
    uint8_t reserved1;
    uint16_t hciversion;
    uint32_t hcsparams;
    uint32_t hccparams;
    uint32_t hcsp_portroute;

    ehci_regs *opregs() {
        uint8_t *ptr = (uint8_t *) (void *) this;
        ptr += caplength;
        return (ehci_regs *) (void *) ptr;
    }
};

struct EECP {
    PciDeviceInformation &pciDeviceInformation;
    union {
        uint32_t value;
        struct {
            uint8_t capability_id;
            uint8_t next_eecp;
            uint16_t data;
        } __attribute__((__packed__));
    } __attribute__((__packed__));
    uint8_t offset;

    EECP(PciDeviceInformation &pciDeviceInformation, uint8_t offset) : pciDeviceInformation(pciDeviceInformation), offset(offset) {
        value = read_pci_config(pciDeviceInformation.bus, pciDeviceInformation.slot, pciDeviceInformation.func, offset);
    }
    EECP(const EECP &cp) : pciDeviceInformation(cp.pciDeviceInformation), value(cp.value), offset(cp.offset) {
    }
    EECP(EECP &&cp) : pciDeviceInformation(cp.pciDeviceInformation), value(cp.value), offset(cp.offset) {
    }
    /*
     * Sounds trivial, but since I use a reference to pciDeviceInformation
     */
    EECP &operator = (const EECP &cp) = delete;
    EECP &operator = (EECP &&cp) = delete;

    bool hasNext() {
        return (next_eecp != 0);
    }
    std::unique_ptr<EECP> next() {
        if (hasNext()) {
            return std::make_unique<EECP>(pciDeviceInformation, next_eecp);
        } else {
            return std::unique_ptr<EECP>();
        }
    }

    EECP &refresh() {
        value = read_pci_config(pciDeviceInformation.bus, pciDeviceInformation.slot, pciDeviceInformation.func, offset);
        return *this;
    }
    void write(uint32_t value) {
        write_pci_config(pciDeviceInformation.bus, pciDeviceInformation.slot, pciDeviceInformation.func, offset, value);
    }
};

#define EHCI_LEGACY_BIOS  0x00010000
#define EHCI_LEGACY_OWNED 0x01000000

class ehci : public Device {
private:
    PciDeviceInformation pciDeviceInformation;
    std::unique_ptr<vmem> mapped_registers_vm;
    ehci_capabilities *caps;
    ehci_regs *regs;
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
