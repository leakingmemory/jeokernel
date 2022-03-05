//
// Created by sigsegv on 28.05.2021.
//

#ifndef JEOKERNEL_EHCI_H
#define JEOKERNEL_EHCI_H

#include <devices/drivers.h>
#include <core/vmem.h>
#include "../pci/pci.h"
#include "Phys32Page.h"
#include "StructPool.h"
#include "usb_hcd.h"
#include "usb_transfer.h"

struct ehci_regs {
    uint32_t usbCommand;
    uint32_t usbStatus;
    uint32_t usbInterruptEnable;
    uint32_t frameIndex;
    uint32_t ctrlDsSegment;
    uint32_t periodicListBase;
    uint32_t asyncListAddr;
    uint32_t reserved[9];
    uint32_t configFlag;
    uint32_t portStatusControl[1];
};
static_assert(sizeof(ehci_regs) == 0x48);

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
    void write8(uint8_t offset, uint8_t value) {
        write8_pci_config(pciDeviceInformation.bus, pciDeviceInformation.slot, pciDeviceInformation.func, this->offset + offset, value);
    }
};

#define EHCI_LEGACY_BIOS  0x00010000
#define EHCI_LEGACY_OWNED 0x01000000

class ehci_endpoint;

#define EHCI_TRANSFER_BUFFER_SIZE 4096

#define EHCI_POINTER_TERMINATE 1
#define EHCI_POINTER_QH        2

#define EHCI_QH_ENDPOINT_SPEED_FULL 0
#define EHCI_QH_ENDPOINT_SPEED_LOW  1
#define EHCI_QH_ENDPOINT_SPEED_HIGH 2

struct ehci_qtd {
    uint32_t next_qtd;
    uint32_t alternate_qtd;
    uint8_t status;
    uint8_t pid : 2;
    uint8_t error_count : 2;
    uint8_t current_page : 3;
    bool interrup_on_cmp : 1;
    uint16_t total_bytes : 15;
    bool data_toggle : 1;
    uint32_t buffer_pages[5];
} __attribute__ ((__packed__));
static_assert(sizeof(ehci_qtd) == 32);

struct ehci_qh {
    uint32_t HorizLink;
    uint8_t DeviceAddress : 7;
    bool InactivateOnNext : 1;
    uint8_t EndpointNumber : 4;
    uint8_t EndpointSpeed : 2;
    uint8_t DataToggleControl : 1;
    bool HeadOfReclamationList : 1;
    uint16_t MaxPacketLength : 11;
    bool ControlEndpointFlag : 1;
    uint8_t NakCountReload : 4;
    uint8_t InterruptScheduleMask;
    uint8_t SplitCompletionMask;
    uint8_t HubAddress : 7;
    uint8_t PortNumber : 7;
    uint8_t HighBandwidthPipeMultiplier : 2;
    uint32_t CurrentQTd;
    uint32_t NextQTd;
    uint32_t AlternateQTd;
    union {
        uint32_t Overlay[6];
        uint8_t Status;
    };
    union {
        struct {
            uint64_t Padding1;
            uint64_t Padding2;
        };
        ehci_endpoint *NextEndpoint;
    };
} __attribute__ ((__packed__));
static_assert(sizeof(ehci_qh) == 64);

union ehci_qh_or_qtd {
    ehci_qh qh;
    ehci_qtd qtd;
};

class ehci;
class ehci_endpoint;

class ehci_transfer : public usb_transfer {
    friend ehci_endpoint;
private:
    std::shared_ptr<StructPoolPointer<ehci_qh_or_qtd,uint32_t>> td;
    std::shared_ptr<usb_buffer> buffer;
    std::shared_ptr<ehci_transfer> next;
public:
    explicit ehci_transfer(ehci &ehciRef);
    std::shared_ptr<usb_buffer> Buffer() override {
        return buffer;
    }
    ehci_qtd &qTD() {
        return td->Pointer()->qtd;
    }
    usb_transfer_status GetStatus() override;
};

class ehci_endpoint : public usb_endpoint {
private:
    ehci &ehciRef;
    std::shared_ptr<StructPoolPointer<ehci_qh_or_qtd,uint32_t>> head;
    std::shared_ptr<StructPoolPointer<ehci_qh_or_qtd,uint32_t>> qh;
    std::shared_ptr<ehci_transfer> pending;
    std::shared_ptr<ehci_transfer> active;
    usb_endpoint_type endpointType;
public:
    ehci_endpoint(ehci &ehciRef, const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_speed speed, usb_endpoint_type endpointType, int pollingRateMs = 0);
    ~ehci_endpoint() override;
private:
    std::shared_ptr<usb_transfer> CreateTransferWithLock(bool commitTransaction, std::shared_ptr<usb_buffer> buffer, uint32_t size, usb_transfer_direction direction, int8_t dataToggle, std::function<void (ehci_transfer &transfer)> &applyFunc);
    std::shared_ptr<usb_transfer> CreateTransferWithoutLock(bool commitTransaction, std::shared_ptr<usb_buffer> buffer, uint32_t size, usb_transfer_direction direction, int8_t dataToggle, std::function<void (ehci_transfer &transfer)> &applyFunc);
public:
    bool Addressing64bit() override;
    std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, void *data, uint32_t size, usb_transfer_direction direction, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction, std::function<void ()> doneCall, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_transfer> CreateTransferWithLock(bool commitTransaction, const void *data, uint32_t size, usb_transfer_direction direction, std::function<void ()> doneCall, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_transfer> CreateTransferWithLock(bool commitTransaction, uint32_t size, usb_transfer_direction direction, std::function<void ()> doneCall, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    bool ClearStall() override;
    void IntWithLock();
};

struct ehci_endpoint_cleanup {
    std::shared_ptr<StructPoolPointer<ehci_qh_or_qtd,uint32_t>> qh;
    std::shared_ptr<ehci_transfer> transfers;
};

#define EHCI_BUFFER_SHORT   64

template <int n> class ehci_buffer : public usb_buffer {
private:
    std::shared_ptr<StructPoolPointer<usb_byte_buffer<n>, uint32_t>> buffer;
public:
    ehci_buffer(std::shared_ptr<StructPoolPointer<usb_byte_buffer<n>, uint32_t>> buffer) : buffer(buffer) {
    }
    void *Pointer() override {
        return buffer->Pointer();
    }
    uint64_t Addr() override{
        return buffer->Phys();
    }
    size_t Size() override {
        return n;
    }
};

class ehci : public usb_hcd {
    friend ehci_endpoint;
    friend ehci_transfer;
private:
    PciDeviceInformation pciDeviceInformation;
    std::unique_ptr<vmem> mapped_registers_vm;
    ehci_capabilities *caps;
    ehci_regs *regs;
    Phys32Page FramesPhys;
    uint32_t *Frames;
    StructPool<StructPoolAllocator<Phys32Page,ehci_qh_or_qtd>> qhtdPool;
    std::shared_ptr<StructPoolPointer<ehci_qh_or_qtd,uint32_t>> qh;
    std::shared_ptr<StructPoolPointer<ehci_qh_or_qtd,uint32_t>> intqhroots[0x20];
    std::shared_ptr<StructPoolPointer<ehci_qh_or_qtd,uint32_t>> intqhs[0x1F];
    unsigned int intcycle;
    std::vector<std::shared_ptr<ehci_endpoint_cleanup>> delayedDestruction;
    std::vector<std::shared_ptr<ehci_endpoint_cleanup>> nextForDestruction;
    std::vector<ehci_endpoint *> watchList;
    StructPool<StructPoolAllocator<Phys32Page,usb_byte_buffer<EHCI_BUFFER_SHORT>>> bufPool;
    hw_spinlock ehcilock;
    uint8_t numPorts;
    bool portPower;
public:
    ehci(Bus &bus, PciDeviceInformation &deviceInformation) : usb_hcd("ehci", bus), pciDeviceInformation(deviceInformation),
                                                              FramesPhys(4096), Frames((uint32_t *) FramesPhys.Pointer()), qhtdPool(),
                                                              qh(), intqhroots(), intqhs(), intcycle(0), watchList(), bufPool(),
                                                              delayedDestruction(), nextForDestruction(), ehcilock(), numPorts(0),
                                                              portPower(false) {}
    void init() override;
    void dumpregs() override;
    int GetNumberOfPorts() override;
    usb_speed HubSpeed() override {
        return HIGH;
    }
    uint32_t GetPortStatus(int port) override;
    void SwitchPortOff(int port) override;
    void SwitchPortOn(int port) override;
    void EnablePort(int port) override;
    void DisablePort(int port) override;
    void ResetPort(int port) override;
    usb_speed PortSpeed(int port) override;
    void ClearStatusChange(int port, uint32_t statuses) override;
    std::shared_ptr<usb_endpoint> CreateControlEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) override;
    std::shared_ptr<usb_endpoint> CreateInterruptEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed, int pollingIntervalMs) override;
    std::shared_ptr<usb_endpoint> CreateBulkEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) override;
    size_t TransferBufferSize() override {
        return EHCI_TRANSFER_BUFFER_SIZE;
    }
    std::shared_ptr<usb_buffer> Alloc(size_t size);
    hw_spinlock &HcdSpinlock() override {
        return ehcilock;
    }
    bool PortResetEnablesPort() override {
        return false;
    }
private:
    bool irq();
    void frameListRollover();
    void usbint();
    void asyncAdvance();
};

class ehci_driver : public Driver {
public:
    ehci_driver() : Driver() {}
    virtual Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_EHCI_H
