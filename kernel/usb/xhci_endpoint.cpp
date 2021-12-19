//
// Created by sigsegv on 12/6/21.
//

#include "xhci_transfer.h"
#include "xhci_endpoint.h"
#include "xhci.h"
#include "UsbBuffer32.h"

xhci_trb *xhci_endpoint_ring_container_endpoint0::Trb(int index) {
    return &(device->SlotData()->Endpoint0Ring.ring[index]);
}

int xhci_endpoint_ring_container_endpoint0::LengthIncludingLink() {
    return device->SlotData()->Endpoint0Ring.LengthIncludingLink();
}

uint64_t xhci_endpoint_ring_container_endpoint0::RingPhysAddr() {
    return device->Endpoint0RingPhys();
}

xhci_endpoint::~xhci_endpoint() {
    {
        critical_section cli{};
        std::lock_guard lock{xhciRef.xhcilock};
        xhciRef.resources->DCBAA()->contexts[slot]->UnsetEndpoint(endpoint, this);
    }
    auto length = ringContainer->LengthIncludingLink();
    for (int i = 0; i < length; i++) {
        transferRing[i].std::shared_ptr<xhci_transfer>::~shared_ptr();
    }
    free(transferRing);
    transferRing = nullptr;
}

bool xhci_endpoint::Addressing64bit() {
    return false;
}

std::shared_ptr<usb_transfer>
xhci_endpoint::CreateTransfer(bool commitTransaction, void *data, uint32_t size, usb_transfer_direction direction,
                              bool bufferRounding, uint16_t delayInterrupt, int8_t dataToggle) {
    critical_section cli{};
    std::lock_guard lock{xhciRef.xhcilock};
    auto transfer = CreateTransferWithLock(commitTransaction, data, size, direction, bufferRounding, delayInterrupt, dataToggle);
    return transfer;
}

std::shared_ptr<usb_transfer>
xhci_endpoint::CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction,
                              bool bufferRounding, uint16_t delayInterrupt, int8_t dataToggle) {
    return CreateTransfer(commitTransaction, size, direction,
                          [] () {}, bufferRounding, delayInterrupt,
                          dataToggle);
}

std::shared_ptr<usb_transfer>
xhci_endpoint::CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction,
                              std::function<void()> doneCall, bool bufferRounding, uint16_t delayInterrupt,
                              int8_t dataToggle) {
    critical_section cli{};
    std::lock_guard lock{xhciRef.xhcilock};
    auto transfer = CreateTransferWithLock(commitTransaction, size, direction, doneCall, bufferRounding, delayInterrupt, dataToggle);
    return transfer;
}

std::shared_ptr<usb_transfer>
xhci_endpoint::CreateTransferWithLock(bool commitTransaction, uint32_t size, usb_transfer_direction direction,
                                      std::function<void()> doneCall, bool bufferRounding, uint16_t delayInterrupt,
                                      int8_t dataToggle) {
    auto transfer = CreateTransferWithLock(commitTransaction, nullptr, size, direction, bufferRounding, delayInterrupt, dataToggle);
    transfer->SetDoneCall(doneCall);
    return transfer;
}

std::shared_ptr<usb_buffer> xhci_endpoint::Alloc() {
    return {};
}

class xhci_setup_packet_buffer : public usb_buffer {
private:
    uint8_t buf[8];
public:
    xhci_setup_packet_buffer(void *data) : buf() {
        memcpy(&(buf[0]), data, 8);
    }
    void *Pointer() override {
        return (void *) &(buf[0]);
    }
    uint64_t Addr() override {
        return 0;
    }
    size_t Size() override {
        return 8;
    }
};

std::shared_ptr<usb_transfer> xhci_endpoint::CreateTransferWithLock(bool commitTransaction, void *data, uint32_t size,
                                                                    usb_transfer_direction direction,
                                                                    bool bufferRounding, uint16_t delayInterrupt,
                                                                    int8_t dataToggle) {
    if (direction == usb_transfer_direction::SETUP) {
        if (size != 8) {
            return {};
        }
        std::shared_ptr<usb_buffer> setup_buffer{new xhci_setup_packet_buffer(data)};
        uint32_t index{0};
        uint64_t phaddr{0};
        xhci_trb *trb{nullptr};
        {
            auto transfer = NextTransfer();
            index = std::get<0>(transfer);
            phaddr = std::get<1>(transfer);
            trb = std::get<2>(transfer);
            if (trb == nullptr) {
                return {};
            }
        }
        usb_control_request *setup;
        {
            void *setup_data = (void *) trb;
            memcpy(setup_data, data, 8);
            setup = (usb_control_request *) setup_data;
        }
        trb->Data.TransferLength = 8;
        trb->Data.InterrupterTarget = 0;
        trb->Command |= XHCI_TRB_SETUP | XHCI_TRB_IMMEDIATE_DATA | XHCI_TRB_INTERRUPT_ON_COMPLETE;
        if (setup->wLength == 0) {
            trb->Setup.TransferType = XHCI_SETUP_TRT_NO_DATA;
        } else {
            if ((setup->bmRequestType & 0x80) == 0) {
                trb->Setup.TransferType = XHCI_SETUP_TRT_OUT;
            } else {
                trb->Setup.TransferType = XHCI_SETUP_TRT_IN;
            }
        }
        CommitCommand(trb);
        std::shared_ptr<usb_transfer> transfer{};
        {
            std::shared_ptr<xhci_transfer> xhciTransfer{new xhci_transfer(setup_buffer, phaddr)};
            transferRing[index] = xhciTransfer;
            transfer = xhciTransfer;
        }
        return transfer;
    }
    uint32_t index{0};
    uint64_t phaddr{0};
    xhci_trb *trb{nullptr};
    {
        auto transfer = NextTransfer();
        index = std::get<0>(transfer);
        phaddr = std::get<1>(transfer);
        trb = std::get<2>(transfer);
        if (trb == nullptr) {
            return {};
        }
    }
    std::shared_ptr<usb_buffer> buffer;
    if (size > 0) {
        buffer = std::shared_ptr<usb_buffer>(new UsbBuffer32(size));
        if (direction == usb_transfer_direction::OUT) {
            memcpy(buffer->Pointer(), data, size);
        }
        trb->Data.DataPtr = buffer->Addr();
    }
    trb->Data.TransferLength = size;
    trb->Data.TDSize = 0;
    trb->Data.InterrupterTarget = 0;
    trb->Flags = direction == usb_transfer_direction::OUT ? XHCI_TRB_DIR_OUT : XHCI_TRB_DIR_IN;
    if (size == 0) {
        trb->Command |= XHCI_TRB_STATUS | XHCI_TRB_IMMEDIATE_DATA | XHCI_TRB_INTERRUPT_ON_COMPLETE;
    } else {
        trb->Command |= XHCI_TRB_DATA | XHCI_TRB_INTERRUPT_ON_COMPLETE;
    }
    CommitCommand(trb);
    std::shared_ptr<usb_transfer> transfer{};
    {
        std::shared_ptr<xhci_transfer> xhciTransfer{new xhci_transfer(buffer, phaddr)};
        transferRing[index] = xhciTransfer;
        transfer = xhciTransfer;
    }
    return transfer;
}

std::tuple<uint32_t, uint64_t, xhci_trb *> xhci_endpoint::NextTransfer() {
    uint32_t trb_index{index};
    xhci_trb *trb = ringContainer->Trb(index);
    if (barrier == nullptr) {
        barrier = trb;
    } else if (barrier == trb) {
        return std::make_tuple<uint32_t,uint64_t,xhci_trb *>(0, 0, nullptr);
    }
    uint64_t physaddr{ringContainer->RingPhysAddr()};
    physaddr += sizeof(*trb) * index;
    if (index == 0) {
        cycle = cycle != 0 ? 0 : 1;
    }
    ++index;
    if (index == (ringContainer->LengthIncludingLink() - 1)) {
        auto *link = ringContainer->Trb(index);
        uint16_t cmd{link->Command};
        cmd &= 0xFFFE;
        cmd |= cycle;
        link->Command = cmd;
        index = 0;
    }
    trb->Data.DataPtr = 0;
    trb->Data.TransferLength = 0;
    trb->Data.TDSize = 0;
    trb->Data.InterrupterTarget = 0;
    trb->Command = trb->Command & 1;
    trb->EnableSlot.SlotType = 0;
    return std::make_tuple<uint32_t,uint64_t,xhci_trb *>(trb_index, physaddr, trb);
}

void xhci_endpoint::CommitCommand(xhci_trb *trb) {
    trb->Command = (trb->Command & 0xFFFE) | (cycle & 1);
    uint32_t doorbell{streamId};
    doorbell = doorbell << 16;
    doorbell |= doorbellTarget;
    xhciRef.doorbellregs->doorbells[slot] = doorbell; /* Ring the bell */
}

void xhci_endpoint::TransferEvent(uint64_t trbaddr, uint32_t transferLength, uint8_t completionCode) {
    uint64_t trboffset{trbaddr - ringContainer->RingPhysAddr()};
    uint64_t trbindex{trboffset / sizeof(xhci_trb)};
    if (trbindex < ringContainer->LengthIncludingLink()) {
        unsigned int nextindex{1};
        nextindex += trbindex;
        if (nextindex == (ringContainer->LengthIncludingLink() - 1)) {
            ++nextindex;
        }
        if (nextindex == ringContainer->LengthIncludingLink()) {
            nextindex = 0;
        }
        std::shared_ptr<xhci_transfer> transfer = transferRing[trbindex];
        if (transfer) {
            transfer->SetStatus(completionCode);
        } else {
            std::stringstream str{};
            str << "XHCI endpoint transfer event trb=" << std::hex << trbaddr << " index " << std::dec << trbindex
                << " barrier " << nextindex << "\n";
            get_klogger() << str.str().c_str();
        }
    }
}

xhci_endpoint::xhci_endpoint(xhci &xhciRef, std::shared_ptr<xhci_endpoint_ring_container> ringContainer, uint8_t slot,
                             uint8_t endpoint, uint16_t streamId) :
        xhciRef(xhciRef), ringContainer(ringContainer), transferRing(nullptr), barrier(nullptr), index(0),
        streamId(streamId), slot(slot), endpoint(endpoint), doorbellTarget(endpoint + 1), cycle(0) {
    auto length = ringContainer->LengthIncludingLink();
    transferRing = (std::shared_ptr<xhci_transfer> *) malloc(sizeof(*transferRing) * length);
    for (int i = 0; i < length; i++) {
        new (&(transferRing[i])) std::shared_ptr<xhci_transfer>();
    }
    xhciRef.resources->DCBAA()->contexts[slot]->SetEndpoint(endpoint, this);
}
