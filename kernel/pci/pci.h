//
// Created by sigsegv on 5/20/21.
//

#ifndef JEOKERNEL_PCI_H
#define JEOKERNEL_PCI_H

#ifndef UNIT_TESTING
#include <devices/devices.h>
#include <acpi/pci_irq_rt.h>
#include <core/LocalApic.h>
#include "../IOApic.h"
#include <klogger.h>
#endif

#include <optional>
#include <tuple>
#include <vector>
#include <functional>
#include <thread>

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
    PciDeviceInformation(PciDeviceInformation &&) = delete;
    PciDeviceInformation & operator = (PciDeviceInformation &&) = delete;
    PciDeviceInformation & operator = (const PciDeviceInformation &cp) = default;

    virtual PciDeviceInformation *GetPciInformation() override;

    PciBaseAddressRegister readBaseAddressRegister(uint8_t index);
    uint32_t readStatusRegister();
    PciRegisterF readRegF();
};

class pci_irq {
private:
    pci &bus;
    std::vector<std::function<bool ()>> handlers;
    uint8_t irq;
    hw_spinlock lock;
    unsigned int missCount;
    unsigned int missCountWarn;
public:
    pci_irq(pci &bus, uint8_t irq) : bus(bus), handlers(), irq(irq), lock(), missCount(0), missCountWarn(1) {}
    pci_irq(pci &bus, const IRQLink &link, int index);
    bool invoke(Interrupt &intr);
    void interrupt(Interrupt &intr);
    void add_handler(const std::function<bool ()> &h);

    uint8_t Irq() {
        return irq;
    }
};

class pci : public Bus {
private:
    uint8_t bus;
    uint8_t br_bus;
    uint8_t br_slot;
    uint8_t br_func;

    std::vector<PciIRQRouting> irqr;
    std::vector<std::tuple<std::string,IRQLink>> SourceMap;
    std::vector<pci_irq*> irqs;
    IOApic *ioapic;
    LocalApic *lapic;
public:
    pci(uint16_t bus, uint16_t br_bus, uint16_t br_slot, uint16_t br_func);
    ~pci() override;
    virtual void ProbeDevices() override;
    std::optional<PciDeviceInformation> probeDevice(uint8_t addr, uint8_t func=0);
    pci_irq *GetIrq(uint8_t irqn);
    virtual void InstallIrqHandler(const IRQLink &link);
private:
    const PciIRQRouting *GetRouting(const PciDeviceInformation &deviceInformation, uint8_t pin_03);
    const IRQLink *GetLink(const std::string &name);
public:
    virtual std::optional<uint8_t> GetIrqNumber(const PciDeviceInformation &deviceInformation, uint8_t pin_03);
    virtual bool AddIrqHandler(uint8_t irq, std::function<bool ()> h);

    IOApic &Ioapic() {
        return *ioapic;
    }
    LocalApic &Lapic() {
        return *lapic;
    }
    bool IsPci() override {
        return true;
    }
    pci *GetPci() override {
        return this;
    }
private:
    void ReadIrqRouting(void *acpi_handle);
};

uint32_t read_pci_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint32_t read8_pci_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void write_pci_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);
void write8_pci_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t value);
void init_pci();
void detect_root_pcis();

#endif //JEOKERNEL_PCI_H
