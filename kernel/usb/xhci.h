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

struct xhci_trb {
    uint64_t DataPtr;
    uint32_t TransferLength : 17;
    uint8_t TDSize : 5;
    uint16_t InterrupterTarget : 10;
    uint16_t Command;
    uint16_t Reserved;
} __attribute__((__packed__));
static_assert(sizeof(xhci_trb) == 16);

#define XHCI_TRB_NORMAL             (1 << 10)
#define XHCI_TRB_SETUP              (2 << 10)
#define XHCI_TRB_DATA               (3 << 10)
#define XHCI_TRB_STATUS             (4 << 10)
#define XHCI_TRB_ISOCH              (5 << 10)
#define XHCI_TRB_LINK               (6 << 10)
#define XHCI_TRB_EVENT_DATA         (7 << 10)
#define XHCI_TRB_NOOP               (8 << 10)
#define XHCI_TRB_ENABLE_SLOT        (9 << 10)
#define XHCI_TRB_DISABLE_SLOT       (10 << 10)
#define XHCI_TRB_ADDRESS_DEVICE     (11 << 10)
#define XHCI_TRB_CONFIGURE_ENDPOINT (12 << 10)

#define XHCI_TRB_CYCLE  1

template <int n> struct xhci_trb_ring {
    static_assert(n > 2);
    xhci_trb ring[n];

    xhci_trb_ring(uint64_t physAddr) : ring() {
        ring[0].DataPtr = physAddr;
        ring[0].TransferLength = 0;
        ring[0].TDSize = 0;
        ring[0].InterrupterTarget = 0;
        ring[0].Command = XHCI_TRB_NORMAL | XHCI_TRB_CYCLE;
        ring[0].Reserved = 0;
        ring[n-1].DataPtr = physAddr;
        ring[n-1].TransferLength = 0;
        ring[n-1].TDSize = 0;
        ring[n-1].InterrupterTarget = 0;
        ring[n-1].Command = XHCI_TRB_LINK;
        ring[n-1].Reserved = 0;
    }
};

struct xhci_rings {
    xhci_trb_ring<16> CommandRing;

    xhci_rings(uint64_t physAddr) : CommandRing(physAddr) {}
};

struct xhci_device_context_entry;

struct xhci_dcbaa {
    uint64_t phys[256];
    xhci_device_context_entry *contexts[256];

    xhci_dcbaa() {
        for (int i = 0; i < 256; i++) {
            phys[i] = 0;
            contexts[i] = nullptr;
        }
    }
    ~xhci_dcbaa() {
        for (int i = 0; i < 256; i++) {
            if (contexts[i] != nullptr) {
                delete contexts[i];
            }
        }
    }
};

static_assert(sizeof(xhci_dcbaa) <= 4096);

class xhci_resources {
public:
    virtual ~xhci_resources() {}
    virtual uint64_t DCBAAPhys() const = 0;
    virtual xhci_dcbaa *DCBAA() const = 0;
    virtual uint64_t ScratchpadPhys() const = 0;
    virtual uint64_t CommandRingPhys() const = 0;
    virtual xhci_rings *Rings() const = 0;
};

class xhci : public Device {
private:
    PciDeviceInformation pciDeviceInformation;
    std::unique_ptr<vmem> mapped_registers_vm;
    xhci_capabilities *capabilities;
    xhci_operational_registers *opregs;
    std::unique_ptr<xhci_resources> resources;
    uint8_t numSlots;
public:
    xhci(Bus &bus, PciDeviceInformation &deviceInformation) : Device("xhci", &bus), pciDeviceInformation(deviceInformation), numSlots(0) {}
    void init() override;
    uint32_t Pagesize();
private:
    bool irq();
};

class xhci_driver : public Driver {
public:
    xhci_driver() : Driver() {}
    virtual Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_XHCI_H
