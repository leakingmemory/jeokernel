//
// Created by sigsegv on 28.05.2021.
//

#ifndef JEOKERNEL_OHCI_H
#define JEOKERNEL_OHCI_H

#include <devices/drivers.h>
#include <core/vmem.h>
#include "../pci/pci.h"
#include "Phys32Page.h"

#define OHCI_CTRL_CBSR_1_1  0x0000
#define OHCI_CTRL_CBSR_1_2  0x0001
#define OHCI_CTRL_CBSR_1_3  0x0002
#define OHCI_CTRL_CBSR_1_4  0x0003
#define OHCI_CTRL_CBSR_MASK 0x0003

#define OHCI_CTRL_PLE       0x0004
#define OHCI_CTRL_IE        0x0008
#define OHCI_CTRL_CLE       0x0010
#define OHCI_CTRL_BLE       0x0020

#define OHCI_CTRL_IR        0x0100

#define OHCI_CTRL_HCFS_MASK 0x00C0
#define OHCI_CTRL_HCFS_RESET       0
#define OHCI_CTRL_HCFS_RESUME      (1 << 6)
#define OHCI_CTRL_HCFS_OPERATIONAL (2 << 6)
#define OHCI_CTRL_HCFS_SUSPEND     (3 << 6)

#define OHCI_CMD_OCR 8

#define OHCI_INT_SCHEDULING_OVERRUN             0x01
#define OHCI_INT_SCHEDULING_WRITE_DONE_HEAD     0x02
#define OHCI_INT_SCHEDULING_START_OF_FRAME      0x04
#define OHCI_INT_SCHEDULING_RESUME_DETECTED     0x08
#define OHCI_INT_SCHEDULING_UNRECOVERABLE_ERR   0x10
#define OHCI_INT_SCHEDULING_FRAME_NUM_OVERFLOW  0x20
#define OHCI_INT_SCHEDULING_ROOT_HUB_STATUS_CH  0x40
#define OHCI_INT_OWNERSHIP_CHANGE 0x40000000
#define OHCI_INT_MASTER_INTERRUPT 0x80000000

#define OHCI_INT_MASK             0xC000007F

#define OHCI_HC_LPSC (1 << 16)

#define OHCI_INTERVAL_FIT        0x80000000
#define OHCI_INTERVAL_FSMPS_MASK 0x7FFF0000
#define OHCI_INTERVAL_FI_MASK    0x00003FFF

struct ohci_endpoint_descriptor {
    uint32_t HcControl;
    uint32_t TailP;
    uint32_t HeadP;
    uint32_t NextED;
};

struct ohci_endpoint_descriptor_pointer {
    ohci_endpoint_descriptor *ed;
    ohci_endpoint_descriptor_pointer *next;
};

struct ohci_hcca {
    uint32_t intr_ed[32];
    uint16_t HccaFrameNumber;
    uint16_t HccaPad1;
    uint32_t HccaDoneHead;
    uint8_t reserved[116];
};

static_assert(sizeof(ohci_hcca) == 252);

struct ohci_hcca_aligned {
    ohci_hcca hcca;
    uint32_t padding;
};

static_assert(sizeof(ohci_hcca_aligned) == 0x100);

struct ohci_hcca_with_eds {
    ohci_hcca_aligned hcca;

    ohci_endpoint_descriptor eds[0x3F];

    uint16_t RelativeAddr(void *ptr) {
        uint64_t base = (uint64_t) this;
        uint64_t addr = (uint64_t) ptr;
        addr -= base;
        return (uint16_t) addr;
    }
};

struct ohci_hcca_and_eds {
    ohci_hcca_with_eds hcca_with_eds;

    ohci_endpoint_descriptor hc_control_ed;

    ohci_endpoint_descriptor hc_bulk_ed;
};

static_assert(sizeof(ohci_hcca_with_eds) == 0x4F0);

class OhciHcca {
private:
    Phys32Page page;
    ohci_hcca_and_eds *hcca;
    ohci_endpoint_descriptor_pointer hcca_ed_ptrs[0x1F];
public:
    OhciHcca();
    ohci_hcca &Hcca() {
        return hcca->hcca_with_eds.hcca.hcca;
    }
    ohci_endpoint_descriptor &ControlED() {
        return hcca->hc_control_ed;
    }
    ohci_endpoint_descriptor &BulkED() {
        return hcca->hc_bulk_ed;
    }
    uint32_t Addr() {
        return page.PhysAddr();
    }
    uint32_t Addr(void *ptr) {
        return page.PhysAddr() + hcca->hcca_with_eds.RelativeAddr(ptr);
    }
};

struct ohci_descriptor_a {
    union {
        uint32_t value;
        struct {
            uint8_t no_ports;
            bool psm : 1;
            bool nps : 1;
            bool dt : 1;
            bool ocpm : 1;
            bool nocp : 1;
            uint8_t reserved1 : 3;
            uint8_t reserved2;
            uint8_t potpgt;
        } __attribute__((__packed__));
    } __attribute__((__packed__));

    ohci_descriptor_a(uint32_t value) : value(value) {}
    ohci_descriptor_a &operator =(const uint32_t &value) {
        this->value = value;
        return *this;
    }
    ohci_descriptor_a &operator =(uint32_t &&value) {
        this->value = value;
        return *this;
    }
} __attribute__((__packed__));

struct ohci_port_status {
    union {
        uint32_t value;
        struct {
            bool CurrentConnectStatus : 1;
            bool pes : 1;
            bool pss : 1;
            bool poci : 1;
            bool prs : 1;
            uint8_t reserved1 : 3;
            bool pps : 1;
            bool lsda : 1;
            uint8_t reserved2 : 6;
            bool csc : 1;
            bool pesc : 1;
            bool pssc : 1;
            bool ocic : 1;
            bool prsc : 1;
            uint8_t reserved3 : 3;
            uint8_t reserved4;
        } __attribute__((__packed__));
    } __attribute__((__packed__));
} __attribute__((__packed__));

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
    OhciHcca hcca;
    uint16_t power_on_port_delay_ms;
    uint8_t no_ports;
public:
    ohci(Bus &bus, PciDeviceInformation &deviceInformation) :
        Device("ohci", &bus), pciDeviceInformation(deviceInformation), mapped_registers_vm(),
        ohciRegisters(nullptr), hcca() {}
    void init() override;

    constexpr uint8_t desca_ndp(uint32_t descA) const {
        return (uint8_t) (descA & 0xFF);
    }
};

class ohci_driver : public Driver {
public:
    ohci_driver() : Driver() {}
    virtual Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_OHCI_H
