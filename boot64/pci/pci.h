//
// Created by sigsegv on 5/20/21.
//

#ifndef JEOKERNEL_PCI_H
#define JEOKERNEL_PCI_H

#ifndef UNIT_TESTING
#include <devices/devices.h>
#endif

#include <optional>

//#define USE_APIC_FOR_PCI_BUSES

struct PciBaseAddressRegister {
    PciDeviceInformation &dev_info;
    uint32_t value;
    uint8_t offset;

    bool is_memory() {
        return (value & 1) == 0;
    }
    bool is_io() {
        return (value & 1) != 0;
    }

    bool is_32bit() {
        return (value & 6) == 0;
    }
    bool is_64bit() {
        return (value & 6) == (2 << 1);
    }

    /**
     * If prefetchable, read has no side effects. You can use write trhough instead of no cache.
     * @return True if reads has no side effects.
     */
    bool is_prefetchable() {
        return ((value & 8) != 0);
    }

    uint32_t addr32() {
        if (is_memory()) {
            return value & 0xFFFFFFF0;
        } else {
            return value & 0xFFFFFFFC;
        }
    }

    uint64_t addr64();

    uint32_t read32();
    void write32(uint32_t value);

    uint32_t memory_size();
};

struct PciRegisterF {
    union {
        uint32_t value;
        struct {
            uint8_t InterruptLine; // PIC INT#
            uint8_t InterruptPin; // A/B/C/D
            uint8_t MinGrant;
            uint8_t MaxLatency;
        };
    };

    PciRegisterF(uint32_t value) : value(value) {}
};

struct PciDeviceInformation : public DeviceInformation {
    uint8_t bus;
    uint8_t func : 3;
    uint8_t slot : 5;
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

    virtual PciDeviceInformation *GetPciInformation() override;

    PciBaseAddressRegister readBaseAddressRegister(uint8_t index);
    uint32_t readStatusRegister();
    PciRegisterF readRegF();
};

class pci : public Bus {
private:
    uint8_t bus;
public:
    pci(uint16_t bus) : Bus("pci"), bus(bus) {
    }
    virtual void ProbeDevices() override;
    std::optional<PciDeviceInformation> probeDevice(uint8_t addr, uint8_t func=0);
};

uint32_t read_pci_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void write_pci_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);
void write8_pci_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t value);
void detect_root_pcis();

#endif //JEOKERNEL_PCI_H
