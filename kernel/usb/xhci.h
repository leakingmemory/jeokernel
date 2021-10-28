//
// Created by sigsegv on 28.05.2021.
//

#ifndef JEOKERNEL_XHCI_H
#define JEOKERNEL_XHCI_H

#ifndef UNIT_TESTING
#include <devices/drivers.h>
#include <core/vmem.h>
#include "../pci/pci.h"
#endif

#include <memory>

struct HostControllerPortRegisters {
    uint32_t portStatusControl;
    uint32_t portPowerManagementStatusControl;
    uint32_t portLinkControl;
    uint32_t portHardwareLPMControl;
} __attribute__((__packed__));

static_assert(sizeof(HostControllerPortRegisters) == 0x10);

struct xhci_operational_registers {
    uint32_t usbCommand;
    uint32_t usbStatus;
    uint32_t pageSize;
    uint64_t reserved1;
    uint32_t deviceNotificationControl;
    uint64_t commandRingControl;
    uint64_t reserved2;
    uint64_t reserved3;
    uint64_t deviceContextBaseAddressArrayPointer;
    uint32_t config;
    uint8_t reserved4[0x400-0x3C];
    HostControllerPortRegisters portRegisters[1];
} __attribute__((__packed__));

static_assert(sizeof(xhci_operational_registers) == 0x410);

union xhci_legsup_cap {
    uint32_t value;
    struct {
        uint16_t cap;
        uint8_t bios_owned;
        uint8_t os_owned;
    };

    uint8_t get_os_own() {
        uint32_t os_own = value >> 24;
        return (uint8_t) os_own;
    }
    void assert_os_own() {
        os_owned = get_os_own() | 1;
    }
    bool is_owned() {
        uint32_t semas = value >> 16;
        if ((semas & 1) == 0 && (semas & 0x100) != 0) {
            return true;
        } else {
            return false;
        }
    }
};

struct xhci_ext_cap {
    uint32_t *pointer;
    union {
        uint32_t value;
        struct {
            uint8_t cap_id;
            uint8_t next_ptr;
            uint16_t cap_specific;
        } __attribute__((__packed__));
    };

    explicit xhci_ext_cap(uint32_t *pointer) : pointer(pointer), value(*pointer) {}
    xhci_ext_cap(const xhci_ext_cap &cp) noexcept : pointer(cp.pointer), value(cp.value) {}
    xhci_ext_cap(const xhci_ext_cap &&cp) noexcept : pointer(cp.pointer), value(cp.value) {}
    xhci_ext_cap & operator = (const xhci_ext_cap &cp) noexcept {
        this->pointer = cp.pointer;
        this->value = cp.value;
        return *this;
    }
    xhci_ext_cap & operator = (const xhci_ext_cap &&cp) noexcept {
        this->pointer = cp.pointer;
        this->value = cp.value;
        return *this;
    }

    std::optional<xhci_ext_cap> next() {
        if (next_ptr != 0) {
            uint32_t *ptr = pointer;
            uint16_t off = next_ptr;
            ptr += off;
            return { xhci_ext_cap(ptr) };
        } else {
            return { };
        }
    }

    xhci_legsup_cap *legsup() {
        if (cap_id == 1) {
            return (xhci_legsup_cap *) (void *) pointer;
        } else {
            return nullptr;
        }
    }
};

union xhci_hccparams1 {
    uint32_t hccparams1;
    struct {
        bool ac64 : 1;
        bool bnc : 1;
        bool csz : 1;
        bool ppc : 1;
        bool pind : 1;
        bool lhrC : 1;
        bool ltc : 1;
        bool nss : 1;
        bool pae : 1;
        bool spc : 1;
        bool sec : 1;
        bool cfc : 1;
        uint8_t maxPSASize : 4;
        uint16_t xhciExtCapPtr;
    } __attribute__((__packed__));

    explicit xhci_hccparams1(uint32_t hccparams1) noexcept : hccparams1(hccparams1) {}
    xhci_hccparams1(const xhci_hccparams1 &cp) noexcept : hccparams1(cp.hccparams1) {}
    xhci_hccparams1(const xhci_hccparams1 &&cp) noexcept : hccparams1(cp.hccparams1) {}
    xhci_hccparams1 & operator = (const xhci_hccparams1 &cp) noexcept {
        this->hccparams1 = cp.hccparams1;
        return *this;
    }
    xhci_hccparams1 & operator = (const xhci_hccparams1 &&cp) noexcept {
        this->hccparams1 = cp.hccparams1;
        return *this;
    }
};

struct xhci_capabilities {
    uint8_t caplength;
    uint8_t reserved;
    uint16_t hciversion;
    uint32_t hcsparams1;
    uint32_t hcsparams2;
    uint32_t hcsparams3;
    uint32_t hccparams1;
    uint32_t dboff;
    uint32_t rtsoff;
    uint32_t hccparams2;

    xhci_operational_registers *opregs() {
        uint8_t *ptr = (uint8_t *) (void *) this;
        ptr += caplength;
        ptr += 0x20;
        return (xhci_operational_registers *) (void *) ptr;
    }

    std::optional<xhci_ext_cap> extcap() {
        xhci_hccparams1 par(hccparams1);
        if (par.xhciExtCapPtr != 0) {
            uint32_t *ptr = (uint32_t *) (void *) this;
            uint32_t off = par.xhciExtCapPtr;
            ptr += off;
            return { xhci_ext_cap(ptr) };
        } else {
            return { };
        }
    }
} __attribute__((__packed__));

static_assert(sizeof(xhci_capabilities) == 0x20);

class xhci : public Device {
private:
    PciDeviceInformation pciDeviceInformation;
    std::unique_ptr<vmem> mapped_registers_vm;
    xhci_capabilities *capabilities;
    xhci_operational_registers *opregs;
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
