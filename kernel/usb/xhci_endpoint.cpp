//
// Created by sigsegv on 12/6/21.
//

#include "xhci_transfer.h"
#include "xhci_endpoint.h"
#include "xhci.h"
#include "UsbBuffer32.h"
#include "strings.h"

//#define XHCI_ENDPOINT_DUMP_TRB

struct xhci_endpoint_data {
    xhci_trb_ring<256> transferRing;
    xhci_endpoint_data(uint64_t physAddr) : transferRing(physAddr) {}
};
static_assert(sizeof(xhci_endpoint_data) <= 4096);

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
    if (endpoint != 0) {
        uint8_t endpointBit = ((endpoint << 1) + (dir == usb_endpoint_direction::IN ? 1 : 0));
        auto *inputctx = &(inputctx_container->InputContext()->context[0]);
        inputctx->dropContextFlags = 1 << endpointBit;
        auto evalContextCommand = xhciRef.EvaluateContext(inputctx_container->InputContextPhys(), slot);
        bool done{false};
        int timeout = 5;
        while (--timeout > 0) {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10ms);
            if (evalContextCommand->IsDone()) {
                done = true;
                break;
            }
        }
        if (!done) {
            std::stringstream str{};
            str <<  "Failed to deconfigure endpoint " << endpoint << "\n";
            get_klogger() << str.str().c_str();
        }
    }
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
    if (transfer) {
        transfer->SetDoneCall(doneCall);
    }
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
        } else {
            bzero(buffer->Pointer(), size);
        }
        trb->Data.DataPtr = buffer->Addr();
    }
    trb->Data.TransferLength = size;
    trb->Data.TDSize = 0;
    trb->Data.InterrupterTarget = 0;
    if (endpoint != 0) {
        trb->Flags = 0;
    } else {
        trb->Flags = direction == usb_transfer_direction::OUT ? XHCI_TRB_DIR_OUT : XHCI_TRB_DIR_IN;
    }
    if (size == 0) {
        trb->Command |= XHCI_TRB_STATUS | XHCI_TRB_INTERRUPT_ON_COMPLETE;
    } else {
        uint16_t flags{XHCI_TRB_INTERRUPT_ON_SHORT | XHCI_TRB_INTERRUPT_ON_COMPLETE};
        if (endpoint != 0) {
            flags |= XHCI_TRB_NORMAL;
        } else {
            flags |= XHCI_TRB_DATA;
        }
        trb->Command |= flags;
    }
    CommitCommand(trb);
    std::shared_ptr<usb_transfer> transfer{};
    {
        std::shared_ptr<xhci_transfer> xhciTransfer{new xhci_transfer(buffer, phaddr)};
        transferRing[index] = xhciTransfer;
        transfer = xhciTransfer;
    }
#ifdef XHCI_ENDPOINT_DUMP_TRB
    {
        uint8_t endpointIndex = ((endpoint << 1) - (dir == usb_endpoint_direction::OUT ? 1 : 0));
        xhciRef.resources->DCBAA()->contexts[slot]->DumpSlotContext();
        xhciRef.resources->DCBAA()->contexts[slot]->DumpEndpoint(endpointIndex, xhciRef.contextSize64);
    }
#endif
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

#ifdef XHCI_ENDPOINT_DUMP_TRB
    std::stringstream str{};
    const uint32_t *trb_r = (uint32_t *) trb;
    str << "TRB " << std::hex << trb_r[0] << " " << trb_r[1] << " " << trb_r[2] << " " << trb_r[3] << " doorbell " << doorbell << " slot " << slot << "\n";
    get_klogger() << str.str().c_str();
#endif
}

void xhci_endpoint::TransferEvent(uint64_t trbaddr, uint32_t transferLength, uint8_t completionCode) {
    uint64_t trboffset{trbaddr - ringContainer->RingPhysAddr()};
    uint64_t trbindex{trboffset / sizeof(xhci_trb)};
    if (trbindex < (ringContainer->LengthIncludingLink() - 1)) {
        std::shared_ptr<xhci_transfer> transfer = transferRing[trbindex];
        barrier = ringContainer->Trb(trbindex);
        if (transfer) {
            transfer->SetStatus(completionCode);
        } else {
            std::stringstream str{};
            str << "XHCI endpoint transfer event trb=" << std::hex << trbaddr << " index " << std::dec << trbindex
                << " code " << completionCode << "\n";
            get_klogger() << str.str().c_str();
        }
    } else {
        std::stringstream str{};
        str << "XHCI endpoint transfer event trb=" << std::hex << trbaddr << " index " << std::dec << trbindex
            << " code " << completionCode << "\n";
        get_klogger() << str.str().c_str();
    }
}

xhci_endpoint::xhci_endpoint(xhci &xhciRef, std::shared_ptr<xhci_endpoint_ring_container> ringContainer,
                             std::shared_ptr<xhci_input_context_container> inputctx_container, uint8_t slot,
                             uint8_t endpoint, usb_endpoint_direction dir, usb_endpoint_type endpointType,
                             uint16_t streamId) :
        xhciRef(xhciRef), ringContainer(ringContainer), transferRing(nullptr), inputctx_container(inputctx_container),
        barrier(nullptr), dir(dir), endpointType(endpointType), index(0), streamId(streamId), slot(slot),

        endpoint(endpoint), doorbellTarget(endpoint << 1), cycle(0) {
    if (dir != usb_endpoint_direction::OUT) {
        ++doorbellTarget;
    }
    auto length = ringContainer->LengthIncludingLink();
    transferRing = (std::shared_ptr<xhci_transfer> *) malloc(sizeof(*transferRing) * length);
    for (int i = 0; i < length; i++) {
        new (&(transferRing[i])) std::shared_ptr<xhci_transfer>();
    }
    xhciRef.resources->DCBAA()->contexts[slot]->SetEndpoint(endpoint, this);
}

bool xhci_endpoint::WaitForEndpointState(uint8_t state) {
    uint8_t endpointIndex = endpoint << ((uint8_t)1);
    if (dir != usb_endpoint_direction::OUT) {
        ++endpointIndex;
    }
    auto *slt = xhciRef.resources->DCBAA()->contexts[slot];
    int timeout = 5;
    while (slt->GetEndpointState(endpointIndex, xhciRef.contextSize64) != state) {
        --timeout;
        if (timeout == 0) {
            return false;
        }
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(10ms);
    }
    return true;
}

bool xhci_endpoint::ResetEndpoint() {
    uint8_t endpointIndex = endpoint << ((uint8_t)1);
    if (dir != usb_endpoint_direction::OUT) {
        ++endpointIndex;
    }

    auto resetEndpointCommand = xhciRef.ResetEndpoint(slot, endpointIndex);
    bool done{false};
    int timeout = 5;
    while (--timeout > 0) {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(10ms);
        if (resetEndpointCommand->IsDone()) {
            done = true;
            break;
        }
    }
    if (!done) {
        get_klogger() << "Failed to reset xhci endpoint\n";
    }
    return done;
}

bool xhci_endpoint::StopEndpoint() {
    uint8_t endpointIndex = endpoint << ((uint8_t)1);
    if (dir != usb_endpoint_direction::OUT) {
        ++endpointIndex;
    }

    auto stopEndpointCommand = xhciRef.StopEndpoint(slot, endpointIndex);
    bool done{false};
    int timeout = 5;
    using namespace std::literals::chrono_literals;
    while (--timeout > 0) {
        std::this_thread::sleep_for(10ms);
        if (stopEndpointCommand->IsDone()) {
            done = true;
            break;
        }
    }
    if (!done) {
        get_klogger() << "Failed to reset xhci endpoint\n";
        return false;
    }
    std::this_thread::sleep_for(10ms);
    return true;
}

bool xhci_endpoint::SetDequeuePtr(uint64_t ptr) {
    uint8_t endpointIndex = endpoint << ((uint8_t)1);
    if (dir != usb_endpoint_direction::OUT) {
        ++endpointIndex;
    }

    auto setDeq = xhciRef.SetDequeuePtr(slot, endpointIndex, 0, 0, ptr);
    bool done{false};
    int timeout = 5;
    while (--timeout > 0) {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(10ms);
        if (setDeq->IsDone()) {
            done = true;
            break;
        }
    }
    if (!done) {
        get_klogger() << "Failed to set dequeue ptr after reset xhci endpoint\n";
        return false;
    }
    return true;
}

bool xhci_endpoint::ReconfigureEndpoint(uint64_t dequeueuPtr) {
    uint8_t endpointIndex = endpoint << ((uint8_t)1);
    if (dir != usb_endpoint_direction::OUT) {
        ++endpointIndex;
    }

    auto *inputContext = inputctx_container->InputContext();
    inputContext->context[0].addContextFlags = (1 << endpointIndex) | 1;
    inputContext->context[0].dropContextFlags = 1 << endpointIndex;
    auto *endpoint = inputContext->EndpointContext(endpointIndex - 1, xhciRef.contextSize64);
    endpoint->trDequePointer = dequeueuPtr;

    auto stopEndpointCommand = xhciRef.ConfigureEndpoint(inputctx_container->InputContextPhys(), slot);
    bool done{false};
    int timeout = 5;
    using namespace std::literals::chrono_literals;
    while (--timeout > 0) {
        std::this_thread::sleep_for(10ms);
        if (stopEndpointCommand->IsDone()) {
            done = true;
            break;
        }
    }
    if (!done) {
        get_klogger() << "Failed to reset xhci endpoint\n";
        return false;
    }
    std::this_thread::sleep_for(10ms);
    return true;
}

bool xhci_endpoint::ClearStall() {
    uint8_t endpointIndex = endpoint << ((uint8_t)1);
    if (dir != usb_endpoint_direction::OUT) {
        ++endpointIndex;
    }

    auto *endpointContext = xhciRef.resources->DCBAA()->contexts[slot]->EndpointContext(endpointIndex - 1, xhciRef.contextSize64);

    uint64_t trbaddr{endpointContext->trDequePointer & 0xFFFFFFFFFFFFFFF0};
    uint64_t nextCycle{endpointContext->trDequePointer & 1};

    uint64_t trboffset{trbaddr - ringContainer->RingPhysAddr()};
    uint64_t nextIndex{trboffset / sizeof(xhci_trb)};

    auto *cancel_trb = ringContainer->Trb(nextIndex);
    cancel_trb->Command = (cancel_trb->Command & 0xFFFE) | ((cancel_trb->Command & 1) == 0 ? 1 : 0);

    if (nextIndex != (ringContainer->LengthIncludingLink() - 1)) {
        ++nextIndex;
        if (nextIndex == (ringContainer->LengthIncludingLink() - 1)) {
            nextIndex = 0;
            nextCycle = (nextCycle == 0 ? 1 : 0);
        }
    } else {
        nextIndex = 0;
    }

    uint64_t dequePointer = ((nextIndex * sizeof(xhci_trb)) + ringContainer->RingPhysAddr()) | (nextCycle & 1);


    if (!SetDequeuePtr(dequePointer) && WaitForEndpointState(1)) {
        get_klogger() << "XHCI post reset and stop endpoint set dequeue pointer failed\n";
        return false;
    }

    if (!ResetEndpoint() && WaitForEndpointState(1)) {
        get_klogger() << "XHCI reset endpoint failed\n";
        return false;
    }

    if (!StopEndpoint() && WaitForEndpointState(3)) {
        get_klogger() << "XHCI post reset stop endpoint failed\n";
        return false;
    }

    if (!SetDequeuePtr(dequePointer) && WaitForEndpointState(1)) {
        get_klogger() << "XHCI post reset and stop endpoint set dequeue pointer failed\n";
        return false;
    }
    cancel_trb->Command = (cancel_trb->Command & 0xFFFE) | ((cancel_trb->Command & 1) == 0 ? 1 : 0);

    return true;
}

xhci_endpoint_ring_container_endpoints::xhci_endpoint_ring_container_endpoints() : page(sizeof(*data)), data(new (page.Pointer()) xhci_endpoint_data(page.PhysAddr())) {
}

xhci_endpoint_ring_container_endpoints::~xhci_endpoint_ring_container_endpoints() {
    data->~xhci_endpoint_data();
}

xhci_trb *xhci_endpoint_ring_container_endpoints::Trb(int index) {
    return &(data->transferRing.ring[index]);
}

int xhci_endpoint_ring_container_endpoints::LengthIncludingLink() {
    return data->transferRing.LengthIncludingLink();
}

uint64_t xhci_endpoint_ring_container_endpoints::RingPhysAddr() {
    return page.PhysAddr();
}
