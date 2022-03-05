//
// Created by sigsegv on 12/6/21.
//

#ifndef JEOKERNEL_XHCI_ENDPOINT_H
#define JEOKERNEL_XHCI_ENDPOINT_H

#include "usb_port_connection.h"
#include "xhci_transfer.h"
#include "Phys32Page.h"

class xhci;
class xhci_device;
struct xhci_trb;
class xhci_port_enumeration_addressing;

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

struct xhci_endpoint_data;

class xhci_endpoint_ring_container_endpoints : public xhci_endpoint_ring_container {
private:
    Phys32Page page;
    xhci_endpoint_data *data;
public:
    xhci_endpoint_ring_container_endpoints();
    ~xhci_endpoint_ring_container_endpoints();
    xhci_trb *Trb(int index) override;
    int LengthIncludingLink() override;
    uint64_t RingPhysAddr() override;
};

class xhci_input_context_container;

class xhci_endpoint : public usb_endpoint {
    friend xhci;
    friend xhci_port_enumeration_addressing;
private:
    xhci &xhciRef;
    std::shared_ptr<xhci_endpoint_ring_container> ringContainer;
    std::shared_ptr<xhci_transfer> *transferRing;
    std::shared_ptr<xhci_input_context_container> inputctx_container;
    xhci_trb *barrier;
    usb_endpoint_direction dir;
    usb_endpoint_type endpointType;
    uint32_t index;
    uint16_t streamId;
    uint8_t slot;
    uint8_t endpoint;
    uint8_t doorbellTarget; /* 1: ep0, 2: ep1 out, 3: ep1 in, ... */
    uint8_t cycle : 1;
public:
    xhci_endpoint(xhci &xhciRef, std::shared_ptr<xhci_endpoint_ring_container> ringContainer,  std::shared_ptr<xhci_input_context_container> inputctx_container, uint8_t slot, uint8_t endpoint, usb_endpoint_direction dir, usb_endpoint_type endpoint_type, uint16_t streamId);
    virtual ~xhci_endpoint();
    bool Addressing64bit() override;
    std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, void *data, uint32_t size, usb_transfer_direction direction, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_transfer> CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction, std::function<void ()> doneCall, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_transfer> CreateTransferWithLock(bool commitTransaction, const void *data, uint32_t size, usb_transfer_direction direction, std::function<void ()> doneCall, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    std::shared_ptr<usb_transfer> CreateTransferWithLock(bool commitTransaction, uint32_t size, usb_transfer_direction direction, std::function<void ()> doneCall, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0) override;
    bool WaitForEndpointState(uint8_t state);
    bool ResetEndpoint();
    bool StopEndpoint();
    bool SetDequeuePtr(uint64_t ptr);
    bool ReconfigureEndpoint(uint64_t ptr);
    bool ClearStall() override;
private:
    std::shared_ptr<usb_transfer> CreateTransferWithLock(bool commitTransaction, const void *data, uint32_t size, usb_transfer_direction direction, bool bufferRounding = false, uint16_t delayInterrupt = TRANSFER_NO_INTERRUPT, int8_t dataToggle = 0);
    std::tuple<uint32_t,uint64_t,xhci_trb *> NextTransfer();
    void CommitCommand(xhci_trb *trb);
    void TransferEvent(uint64_t trbaddr, uint32_t transferLength, uint8_t completionCode);
};


#endif //JEOKERNEL_XHCI_ENDPOINT_H
