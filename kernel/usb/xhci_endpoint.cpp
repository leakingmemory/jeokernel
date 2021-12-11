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

std::shared_ptr<usb_transfer> xhci_endpoint::CreateTransferWithLock(bool commitTransaction, void *data, uint32_t size,
                                                                    usb_transfer_direction direction,
                                                                    bool bufferRounding, uint16_t delayInterrupt,
                                                                    int8_t dataToggle) {
    if (direction == usb_transfer_direction::SETUP) {
        if (size != 8) {
            return {};
        }
        uint64_t phaddr{0};
        xhci_trb *trb{nullptr};
        {
            auto transfer = NextTransfer();
            phaddr = std::get<0>(transfer);
            trb = std::get<1>(transfer);
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
        std::shared_ptr<usb_transfer> transfer{new xhci_transfer({}, phaddr)};
        return transfer;
    }
    uint64_t phaddr{0};
    xhci_trb *trb{nullptr};
    {
        auto transfer = NextTransfer();
        phaddr = std::get<0>(transfer);
        trb = std::get<1>(transfer);
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
    std::shared_ptr<usb_transfer> transfer{new xhci_transfer(buffer, phaddr)};
    return transfer;
}

std::tuple<uint64_t, xhci_trb *> xhci_endpoint::NextTransfer() {
    xhci_trb *trb = ringContainer->Trb(index);
    if (barrier == nullptr) {
        barrier = trb;
    } else if (barrier == trb) {
        return std::make_tuple<uint64_t,xhci_trb *>(0, nullptr);
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
    return std::make_tuple<uint64_t,xhci_trb *>(physaddr, trb);
}

void xhci_endpoint::CommitCommand(xhci_trb *trb) {
    trb->Command = (trb->Command & 0xFFFE) | (cycle & 1);
    uint32_t doorbell{streamId};
    doorbell = doorbell << 16;
    doorbell |= doorbellTarget;
    xhciRef.doorbellregs->doorbells[slot] = doorbell; /* Ring the bell */
}
