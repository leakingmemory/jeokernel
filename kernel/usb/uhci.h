//
// Created by sigsegv on 28.05.2021.
//

#ifndef JEOKERNEL_UHCI_H
#define JEOKERNEL_UHCI_H

#include <devices/drivers.h>
#include "../pci/pci.h"
#include "Phys32Page.h"
#include "StructPool.h"
#include "usb_hcd.h"
#include "usb_transfer.h"

#define UHCI_TRANSFER_BUFFER_SIZE 64

class uhci_endpoint;

struct uhci_queue_head {
    uint32_t head;
    uint32_t element;
    union {
        uint64_t unused;
        uhci_endpoint *NextEndpoint;
    };
};
static_assert(sizeof(uhci_queue_head) == 16);

struct uhci_transfer_descriptor {
    uint32_t next;
    uint16_t ActLen;
    uint16_t Status;
    uint8_t PID;
    uint8_t DeviceAddress : 7;
    uint8_t Endpoint : 4;
    uint8_t DataToggle : 1;
    uint8_t Reserved : 1;
    uint16_t MaxLen : 11;
    uint32_t BufferPointer;

    uint64_t unused1;
    uint64_t unused2;
} __attribute__((__packed__));
static_assert(sizeof(uhci_transfer_descriptor) == 32);

union uhci_qh_or_td {
    uhci_queue_head qh;
    uhci_transfer_descriptor td;
};
static_assert(sizeof(uhci_qh_or_td) == 32);

class uhci;

class uhci_buffer : public usb_buffer {
private:
    std::shared_ptr<StructPoolPointer<usb_byte_buffer<UHCI_TRANSFER_BUFFER_SIZE>,uint32_t>> bufferPtr;
public:
    explicit uhci_buffer(uhci &uhci);
    void *Pointer() override {
        return bufferPtr->Pointer();
    }
    uint64_t Addr() override {
        return bufferPtr->Phys();
    }
    size_t Size() override {
        return UHCI_TRANSFER_BUFFER_SIZE;
    }
};

class uhci_transfer : public usb_transfer {
    friend uhci_endpoint;
private:
    std::shared_ptr<StructPoolPointer<uhci_qh_or_td,uint32_t>> td;
    std::shared_ptr<usb_buffer> buffer;
    std::shared_ptr<uhci_transfer> next;
public:
    explicit uhci_transfer(uhci &uhciRef);
    std::shared_ptr<usb_buffer> Buffer() override {
        return buffer;
    }
    uhci_transfer_descriptor &TD() {
        return td->Pointer()->td;
    }
    usb_transfer_status GetStatus() override;
};

class uhci_endpoint : public usb_endpoint {
private:
    uhci &uhciRef;
    usb_speed speed;
    usb_endpoint_type endpointType;
    int pollingRateMs;
    uint32_t maxPacketSize;
    uint8_t functionAddr;
    uint8_t endpointNum;
    std::shared_ptr<StructPoolPointer<uhci_qh_or_td,uint32_t>> qh;
    std::shared_ptr<uhci_transfer> pending;
    std::shared_ptr<uhci_transfer> active;
public:
    uhci_endpoint(uhci &uhciRef, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_speed speed, usb_endpoint_type endpointType, int pollingRateMs = 0);
    ~uhci_endpoint() override;
    bool Addressing64bit() override {
        return false;
    }
private:
    std::shared_ptr<usb_transfer> CreateTransferWithLock(bool commitTransaction, std::shared_ptr<usb_buffer> buffer, uint32_t size, usb_transfer_direction direction, int8_t dataToggle, std::function<void (uhci_transfer &transfer)> &applyFunc);
    std::shared_ptr<usb_transfer> CreateTransferWithoutLock(bool commitTransaction, std::shared_ptr<usb_buffer> buffer, uint32_t size, usb_transfer_direction direction, int8_t dataToggle, std::function<void (uhci_transfer &transfer)> &applyFunc);
public:
    std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, void *data, uint32_t size, usb_transfer_direction direction, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction, std::function<void ()> doneCall, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_transfer> CreateTransferWithLock(bool commitTransaction, uint32_t size, usb_transfer_direction direction, std::function<void ()> doneCall, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_buffer> Alloc() override;
    void IntWithLock();
};

struct uhci_endpoint_cleanup {
    std::shared_ptr<StructPoolPointer<uhci_qh_or_td,uint32_t>> qh;
    std::shared_ptr<uhci_transfer> transfers;
};

class uhci : public usb_hcd {
    friend uhci_endpoint;
    friend uhci_buffer;
    friend uhci_transfer;
private:
    PciDeviceInformation pciDeviceInformation;
    Phys32Page FramesPhys;
    uint32_t *Frames;
    StructPool<StructPoolAllocator<Phys32Page,uhci_qh_or_td>> qhtdPool;
    std::shared_ptr<StructPoolPointer<uhci_qh_or_td,uint32_t>> qh;
    StructPool<StructPoolAllocator<Phys32Page,usb_byte_buffer<UHCI_TRANSFER_BUFFER_SIZE>>> bufPool;
    std::vector<uhci_endpoint *> watchList;
    std::vector<std::shared_ptr<uhci_endpoint_cleanup>> delayedDestruction;
    hw_spinlock uhcilock;
    uint32_t iobase;
public:
    uhci(Bus &bus, PciDeviceInformation &deviceInformation) :
        usb_hcd("uhci", bus), pciDeviceInformation(deviceInformation),
        FramesPhys(4096), Frames((uint32_t *) FramesPhys.Pointer()),
        qhtdPool(), qh(), bufPool(), watchList(), delayedDestruction(), uhcilock() {}
    void init() override;
    void dumpregs() override;
    int GetNumberOfPorts() override;
    uint32_t GetPortStatus(int port) override;
    void SwitchPortOff(int port) override;
    void SwitchPortOn(int port) override;
    void EnablePort(int port) override;
    void DisablePort(int port) override;
    void ResetPort(int port) override;
    usb_speed PortSpeed(int port) override;
    void ClearStatusChange(int port, uint32_t statuses) override;
    std::shared_ptr<usb_endpoint> CreateControlEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) override;
    std::shared_ptr<usb_endpoint> CreateInterruptEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed, int pollingIntervalMs) override;
    size_t TransferBufferSize() override {
        return UHCI_TRANSFER_BUFFER_SIZE;
    }
    hw_spinlock &HcdSpinlock() override {
        return uhcilock;
    }
private:
    bool irq();
    void usbint();
};

class uhci_driver : public Driver {
public:
    uhci_driver() : Driver() {}
    virtual Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_UHCI_H
