//
// Created by sigsegv on 28.05.2021.
//

#ifndef JEOKERNEL_OHCI_H
#define JEOKERNEL_OHCI_H

#include <devices/drivers.h>
#include <core/vmem.h>
#include "../pci/pci.h"
#include "Phys32Page.h"
#include "usb_hcd.h"
#include "StructPool.h"
#include "usb_transfer.h"
#include <kernelconfig.h>

#define OHCI_CTRL_CBSR_1_1  0x0000
#define OHCI_CTRL_CBSR_1_2  0x0001
#define OHCI_CTRL_CBSR_1_3  0x0002
#define OHCI_CTRL_CBSR_1_4  0x0003
#define OHCI_CTRL_CBSR_MASK 0x0003

#define OHCI_CTRL_PLE       0x0004
#define OHCI_CTRL_IE        0x0008
#define OHCI_CTRL_CLE       0x0010
#define OHCI_CTRL_BLE       0x0020

#define OHCI_CTRL_ENDPOINTS_MASK (OHCI_CTRL_PLE | OHCI_CTRL_IE | OHCI_CTRL_CLE | OHCI_CTRL_BLE)

#define OHCI_CTRL_IR        0x0100

#define OHCI_CTRL_HCFS_MASK 0x00C0
#define OHCI_CTRL_HCFS_RESET       0
#define OHCI_CTRL_HCFS_RESUME      (1 << 6)
#define OHCI_CTRL_HCFS_OPERATIONAL (2 << 6)
#define OHCI_CTRL_HCFS_SUSPEND     (3 << 6)

#define OHCI_CMD_HCR 1 // Host Controller Reset
#define OHCI_CMD_CLF 2 // Control List Filled
#define OHCI_CMD_BLF 4 // Bulk List Filled
#define OHCI_CMD_OCR 8 // Ownership Change Request

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

    ~ohci_endpoint_descriptor() {
        get_klogger() << "ohci endpoint destruction\n";
    }
};

struct ohci_transfer_descriptor {
    uint32_t TdControl;
    uint32_t CurrentBufferPointer;
    uint32_t NextTD;
    uint32_t BufferEnd;

    ohci_transfer_descriptor() : TdControl(0), CurrentBufferPointer(0), NextTD(0), BufferEnd(0) {
    }

#ifdef OHCI_DEBUGPRINT_TRANSFER
    ~ohci_transfer_descriptor() {
        {
            std::stringstream str{};
            str << std::hex << "Destroying transfer phys c=" << TdControl
                << " buf=" << CurrentBufferPointer << "/" << BufferEnd
                << " next=" << NextTD << "\n";
            get_klogger() << str.str().c_str();
        }
    }
#endif
};

class ohci_endpoint;

struct ohci_endpoint_descriptor_pointer {
    ohci_endpoint_descriptor *ed;
    ohci_endpoint *head;
    uint8_t index;
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

    ohci_endpoint_descriptor eds[0x1F];

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

static_assert(sizeof(ohci_hcca_with_eds) == 0x2F0);

class OhciHcca {
private:
    Phys32Page page;
    ohci_hcca_and_eds *hcca;
    ohci_endpoint_descriptor_pointer hcca_ed_ptrs[0x1F];
    ohci_endpoint_descriptor_pointer hcca_root_ptrs[0x20];
    uint16_t cycle;
public:
    OhciHcca();
    ohci_hcca &Hcca() {
        return hcca->hcca_with_eds.hcca.hcca;
    }
    ohci_endpoint_descriptor_pointer &GetInterruptEDHead(int pollingRateMs) {
        if (pollingRateMs < 2) {
            return hcca_ed_ptrs[0x1E]; // 1ms
        } else if (pollingRateMs < 3) {
            return hcca_ed_ptrs[0x1C + (cycle++ & 1)]; // 2ms
        } else if (pollingRateMs < 5) {
            return hcca_ed_ptrs[0x18 + (cycle++ & 3)]; // 4ms
        } else if (pollingRateMs < 9) {
            return hcca_ed_ptrs[0x10 + (cycle++ & 7)]; // 8ms
        } else if (pollingRateMs < 17) {
            return hcca_ed_ptrs[cycle++ & 0xF]; // 16ms
        } else {
            return hcca_root_ptrs[cycle++ & 0x1F]; // 32ms
        }
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

struct ohci_descriptor_b {
    union {
        uint32_t value;
        struct {
            uint16_t dr;
            uint16_t ppcm;
        } __attribute__((__packed__));
    } __attribute__((__packed__));

    ohci_descriptor_b(uint32_t value) : value(value) {}
    ohci_descriptor_b &operator =(const uint32_t &value) {
        this->value = value;
        return *this;
    }
    ohci_descriptor_b &operator =(uint32_t &&value) {
        this->value = value;
        return *this;
    }
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

#define OHCI_SHORT_TRANSFER_BUFSIZE 64
#define OHCI_TRANSFER_BUFSIZE       1024

class ohci;

template <int n> class ohci_buffer : public usb_buffer {
private:
    std::shared_ptr<StructPoolPointer<usb_byte_buffer<n>,uint32_t>> bufferPtr;
public:
    explicit ohci_buffer(std::shared_ptr<StructPoolPointer<usb_byte_buffer<n>,uint32_t>> bufferPtr) : bufferPtr(bufferPtr) {}
    void *Pointer() override {
        return bufferPtr->Pointer();
    }
    uint64_t Addr() override {
        return bufferPtr->Phys();
    }
    size_t Size() override {
        return n;
    }
};

class ohci_transfer : public usb_transfer {
    friend ohci;
    friend ohci_endpoint;
private:
    std::shared_ptr<StructPoolPointer<ohci_transfer_descriptor,uint32_t>> transferPtr;
    std::shared_ptr<usb_buffer> buffer;
    std::shared_ptr<ohci_transfer> next;
    ohci_endpoint &endpoint;
    bool waitCancelled, waitCancelledAndWaitedCycle;
public:
    ohci_transfer(ohci &ohci, ohci_endpoint &endpoint);
    ~ohci_transfer() override;
    ohci_transfer_descriptor *TD() {
        return transferPtr->Pointer();
    }
    uint32_t PhysAddr() {
        return transferPtr->Phys();
    }
    std::shared_ptr<usb_buffer> Buffer() {
        return buffer;
    }
    void SetDone() override;
    usb_transfer_status GetStatus() override;
};

struct ohci_endpoint_cleanup {
    ohci &ohciRef;
    std::shared_ptr<StructPoolPointer<ohci_endpoint_descriptor,uint32_t>> ed;
    std::shared_ptr<ohci_transfer> transfers;

    ohci_endpoint_cleanup(
            ohci &ohciRef,
            std::shared_ptr<StructPoolPointer<ohci_endpoint_descriptor,uint32_t>> ed,
            std::shared_ptr<ohci_transfer> transfers
            ) : ohciRef(ohciRef), ed(ed), transfers(transfers) {}
    ~ohci_endpoint_cleanup();
};

class ohci_endpoint : public usb_endpoint {
    friend ohci_transfer;
private:
    ohci &ohciRef;
    std::shared_ptr<StructPoolPointer<ohci_endpoint_descriptor,uint32_t>> edPtr;
    std::shared_ptr<ohci_transfer> head;
    std::shared_ptr<ohci_transfer> tail;
    usb_endpoint_type endpointType;
    ohci_endpoint *next;
    ohci_endpoint_descriptor_pointer *int_ed_chain;
public:
    ohci_endpoint(ohci &ohci, usb_endpoint_type endpointType);
    ohci_endpoint(ohci &ohci, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed, usb_endpoint_type endpointType, int pollingRateMs = 0);
    virtual ~ohci_endpoint();
    void SetNext(ohci_endpoint *endpoint);
    void SetSkip(bool skip = true);
    uint32_t Phys();
    bool Addressing64bit() override {
        return false;
    }
private:
    std::shared_ptr<usb_transfer> CreateTransfer(std::shared_ptr<usb_buffer> buffer, uint32_t size, usb_transfer_direction direction, bool bufferRounding, uint16_t delayInterrupt, int8_t dataToggle, std::function<void (ohci_transfer &transfer)> &applyFunc);
    std::shared_ptr<usb_transfer> CreateTransferWithLock(std::shared_ptr<usb_buffer> buffer, uint32_t size, usb_transfer_direction direction, bool bufferRounding, uint16_t delayInterrupt, int8_t dataToggle, std::function<void (ohci_transfer &transfer)> &applyFunc);
public:
    std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, void *data, uint32_t size, usb_transfer_direction direction, bool bufferRounding, uint16_t delayInterrupt, int8_t dataToggle) override;
    std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, void *data, uint32_t size, usb_transfer_direction direction, std::function<void ()> doneCall, bool bufferRounding, uint16_t delayInterrupt, int8_t dataToggle) override;
    std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction, bool bufferRounding, uint16_t delayInterrupt, int8_t dataToggle) override;
    std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction, std::function<void ()> doneCall, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_transfer> CreateTransferWithLock(bool commitTransaction, const void *data, uint32_t size, usb_transfer_direction direction, std::function<void ()> doneCall, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_transfer> CreateTransferWithLock(bool commitTransaction, uint32_t size, usb_transfer_direction direction, std::function<void ()> doneCall, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_buffer> Alloc(size_t size);
    bool ClearStall() override;
};

struct ohci_statistics : public statistics_object {
    std::shared_ptr<statistics_object> edPoolStats;
    uint32_t destroyEdsCount;
    std::shared_ptr<statistics_object> shortPoolStats;
    std::shared_ptr<statistics_object> bufPoolStats;
    std::shared_ptr<statistics_object> xPoolStats;
    uint32_t transfersInProgressCount;

    void Accept(statistics_visitor &visitor) override;
};

class ohci : public usb_hcd {
    friend ohci_endpoint;
    friend ohci_endpoint_cleanup;
    friend ohci_transfer;
private:
    PciDeviceInformation pciDeviceInformation;
    std::unique_ptr<vmem> mapped_registers_vm;
    ohci_registers *ohciRegisters;
    OhciHcca hcca;
    ohci_descriptor_b bvalue;
    uint16_t power_on_port_delay_ms;
    uint8_t no_ports;
    StructPool<StructPoolAllocator<Phys32Page,ohci_endpoint_descriptor>> edPool;
    std::vector<ohci_endpoint_cleanup> destroyEds;
    ohci_endpoint controlEndpointEnd;
    ohci_endpoint *controlHead;
    ohci_endpoint bulkEndpointEnd;
    ohci_endpoint *bulkHead;
    StructPool<StructPoolAllocator<Phys32Page,usb_byte_buffer<OHCI_SHORT_TRANSFER_BUFSIZE>>> shortBufPool;
    StructPool<StructPoolAllocator<Phys32Page,usb_byte_buffer<OHCI_TRANSFER_BUFSIZE>>> bufPool;
    StructPool<StructPoolAllocator<Phys32Page,ohci_transfer_descriptor>> xPool;
    std::vector<std::shared_ptr<usb_transfer>> transfersInProgress;
    bool StartOfFrameReceived;
    hw_spinlock ohcilock;
public:
    ohci(Bus &bus, PciDeviceInformation &deviceInformation) :
        usb_hcd("ohci", bus), pciDeviceInformation(deviceInformation),
        mapped_registers_vm(), ohciRegisters(nullptr), hcca(), bvalue(0), edPool(), destroyEds(),
        controlEndpointEnd(*this, usb_endpoint_type::CONTROL), controlHead(&controlEndpointEnd),
        bulkEndpointEnd(*this, usb_endpoint_type::BULK), bulkHead(&bulkEndpointEnd), shortBufPool(),
        bufPool(), xPool(), transfersInProgress(), StartOfFrameReceived(false), ohcilock() {}
    void init() override;
    void dumpregs() override;

    constexpr uint8_t desca_ndp(uint32_t descA) const {
        return (uint8_t) (descA & 0xFF);
    }
    bool PortPowerIndividuallyControlled(int portno) {
        uint16_t bit = 2 << portno;
        return (bvalue.ppcm & bit) != 0;
    }
private:
    bool ResetHostController();
    bool irq();
    void StartOfFrame();
    std::shared_ptr<usb_transfer> ExtractDoneQueueItem(uint32_t physaddr);
    void ProcessDoneQueue();
public:
    int GetNumberOfPorts() override {
        return no_ports;
    }
    usb_speed HubSpeed() override {
        return FULL;
    }
    uint32_t GetPortStatus(int port) override;
    void SwitchPortOff(int port) override;
    void SwitchPortOn(int port) override;
    void ClearStatusChange(int port, uint32_t statuses) override;
    std::shared_ptr<usb_endpoint> CreateControlEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) override;
    std::shared_ptr<usb_endpoint> CreateInterruptEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed, int pollingIntervalMs) override;
    std::shared_ptr<usb_endpoint> CreateBulkEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) override;
    size_t TransferBufferSize() override {
        return OHCI_TRANSFER_BUFSIZE;
    }
    std::shared_ptr<usb_buffer> Alloc(size_t size);
    hw_spinlock &HcdSpinlock() override {
        return ohcilock;
    }
    void EnablePort(int port) override;
    void DisablePort(int port) override;
    void ResetPort(int port) override;
    usb_speed PortSpeed(int port) override;

    std::shared_ptr<statistics_object> GetStatisticsObject() override;
};

class ohci_driver : public Driver {
public:
    ohci_driver() : Driver() {}
    virtual Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_OHCI_H
