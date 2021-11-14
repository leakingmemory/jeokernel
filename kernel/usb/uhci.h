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

#define UHCI_TRANSFER_BUFFER_SIZE 64

struct uhci_queue_head {
    uint32_t head;
    uint32_t element;
    uint32_t unused1;
    uint32_t unused2;
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
} __attribute__((__packed__));
static_assert(sizeof(uhci_transfer_descriptor) == 16);

union uhci_qh_or_td {
    uhci_queue_head qh;
    uhci_transfer_descriptor td;
};
static_assert(sizeof(uhci_qh_or_td) == 16);

class uhci : public usb_hcd {
private:
    PciDeviceInformation pciDeviceInformation;
    Phys32Page FramesPhys;
    uint32_t *Frames;
    StructPool<StructPoolAllocator<Phys32Page,uhci_qh_or_td>> qhtdPool;
    std::shared_ptr<StructPoolPointer<uhci_qh_or_td,uint32_t>> qh;
    hw_spinlock uhcilock;
    uint32_t iobase;
public:
    uhci(Bus &bus, PciDeviceInformation &deviceInformation) :
        usb_hcd("uhci", bus), pciDeviceInformation(deviceInformation),
        FramesPhys(4096), Frames((uint32_t *) FramesPhys.Pointer()),
        qhtdPool(), qh(), uhcilock() {}
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
};

class uhci_driver : public Driver {
public:
    uhci_driver() : Driver() {}
    virtual Device *probe(Bus &bus, DeviceInformation &deviceInformation) override;
};


#endif //JEOKERNEL_UHCI_H
