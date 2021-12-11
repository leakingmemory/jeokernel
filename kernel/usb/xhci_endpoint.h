//
// Created by sigsegv on 12/6/21.
//

#ifndef JEOKERNEL_XHCI_ENDPOINT_H
#define JEOKERNEL_XHCI_ENDPOINT_H

#include "usb_port_connection.h"

class xhci;
class xhci_device;
struct xhci_trb;

class xhci_endpoint_ring_container {
public:
    virtual xhci_trb *Trb(int index) = 0;
    virtual int LengthIncludingLink() = 0;
    virtual uint64_t RingPhysAddr() = 0;
};

class xhci_endpoint_ring_container_endpoint0 : public xhci_endpoint_ring_container {
private:
    std::shared_ptr<xhci_device> device;
public:
    xhci_endpoint_ring_container_endpoint0(std::shared_ptr<xhci_device> device) : device(device) { }
    xhci_trb *Trb(int index) override;
    int LengthIncludingLink() override;
    uint64_t RingPhysAddr() override;
};

class xhci_endpoint : public usb_endpoint {
    friend xhci;
private:
    xhci &xhciRef;
    std::shared_ptr<xhci_endpoint_ring_container> ringContainer;
    xhci_trb *barrier;
    uint32_t index;
    uint16_t streamId;
    uint8_t slot;
    uint8_t doorbellTarget; /* 1: ep0, 2: ep1 out, 3: ep1 in, ... */
    uint8_t cycle : 1;
public:
    xhci_endpoint(xhci &xhciRef, std::shared_ptr<xhci_endpoint_ring_container> ringContainer,  uint8_t slot, uint8_t doorbellTarget, uint16_t streamId) : xhciRef(xhciRef), ringContainer(ringContainer), barrier(nullptr), index(0), streamId(streamId), slot(slot), doorbellTarget(doorbellTarget), cycle(0) { }
    virtual ~xhci_endpoint();
    bool Addressing64bit() override;
    std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, void *data, uint32_t size, usb_transfer_direction direction, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction, std::function<void ()> doneCall, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_transfer> CreateTransferWithLock(bool commitTransaction, uint32_t size, usb_transfer_direction direction, std::function<void ()> doneCall, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_buffer> Alloc() override;
private:
    std::shared_ptr<usb_transfer> CreateTransferWithLock(bool commitTransaction, void *data, uint32_t size, usb_transfer_direction direction, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0);
    std::tuple<uint64_t,xhci_trb *> NextTransfer();
    void CommitCommand(xhci_trb *trb);
};


#endif //JEOKERNEL_XHCI_ENDPOINT_H