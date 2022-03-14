//
// Created by sigsegv on 28.05.2021.
//

#include <sstream>
#include <klogger.h>
#include <cpuio.h>
#include <delay.h>
#include "uhci.h"

#define REG_LEGACY_SUPPORT 0xC0
#define REG_COMMAND        0
#define REG_STATUS         0x02
#define REG_INTR           0x04
#define REG_FRAME_NUMBER   0x06
#define REG_FRAME_BASEADDR 0x08
#define REG_SOFMOD         0x0C
#define REG_PORTSC1        0x10
#define REG_PORTSC2        0x12

#define COMMAND_RUN_STOP   1
#define COMMAND_HCRESET    2
#define COMMAND_GRESET     4

#define UHCI_POINTER_TERMINATE 1
#define UHCI_POINTER_QH        2
#define UHCI_POINTER_DEPTH     4

#define UHCI_PORT_SUSPENDED         0x1000
#define UHCI_PORT_RESET             0x0200
#define UHCI_PORT_LOW_SPEED         0x0100
#define UHCI_PORT_RESUME_DETECT     0x0040
#define UHCI_PORT_LINE_STATUS_DM    0x0020
#define UHCI_PORT_LINE_STATUS_DP    0x0010
#define UHCI_PORT_ENABLED_CHANGE    0x0008
#define UHCI_PORT_ENABLED           0x0004
#define UHCI_PORT_CONNECTED_CHANGE  0x0002
#define UHCI_PORT_CONNECTED         0x0001

#define UHCI_TD_STATUS_ACTIVE           0x80
#define UHCI_TD_STATUS_STALL            0x40
#define UHCI_TD_STATUS_DATABUF_ERR      0x20
#define UHCI_TD_STATUS_BABBLE           0x10
#define UHCI_TD_STATUS_NAK              0x08
#define UHCI_TD_STATUS_CRC_OR_TIMEOUT   0x04
#define UHCI_TD_STATUS_BITSTUFF_ERR     0x02

#define UHCI_TD_STATUS_INTERRUPT_ON_CMP 0x0100
#define UHCI_TD_STATUS_ISOCHRONOUS      0x0200
#define UHCI_TD_STATUS_LOW_SPEED        0x0400
#define UHCI_TD_STATUS_SHORT_PACKET_DET 0x2000

#define UHCI_INT_SHORT_PACKET   0x0008
#define UHCI_INT_INTERRUPT_COMP 0x0004
#define UHCI_INT_RESUME         0x0002
#define UHCI_INT_TIMEOUT_OR_CRC 0x0001

#define UHCI_STATUS_USBINT 1
#define UHCI_STATUS_HALTED 0x20

Device *uhci_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    if (
            deviceInformation.device_class == 0x0C /* serial bus */ &&
            deviceInformation.device_subclass == 3 /* USB */ &&
            deviceInformation.prog_if == 0x00 /* uhci */
    ) {
        return new uhci(bus, *(deviceInformation.GetPciInformation()));
    } else {
        return nullptr;
    }
}

void uhci::init() {
    {
        PciBaseAddressRegister bar4 = pciDeviceInformation.readBaseAddressRegister(4);
        if (!bar4.is_io()) {
            get_klogger() << "USB uhci controller is invalid (invalid BAR4, not ioport)\n";
            return;
        }
        iobase = bar4.addr32();
    }
    outportw(iobase + REG_LEGACY_SUPPORT, 0x8F00); // Clear legacy support bit 0x8000, and clear status trap flags
    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": USB controller iobase 0x" << std::hex << iobase << "\n";
        get_klogger() << msg.str().c_str();
    }

    if (!reset()) {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": UHCI reset failed\n";
        get_klogger() << msg.str().c_str();
        return;
    }

    PciRegisterF regF{pciDeviceInformation.readRegF()};
    {
        auto *pci = GetBus()->GetPci();
        std::optional<uint8_t> irq = pci->GetIrqNumber(pciDeviceInformation, regF.InterruptPin - 1);
        {
            std::stringstream msg;
            msg << DeviceType() << (unsigned int) DeviceId() << ": INT pin " << (unsigned int) regF.InterruptPin
                << " PIC INT#" << (unsigned int) regF.InterruptLine << " ACPI INT# " << (irq ? *irq : 0) << "\n";
            get_klogger() << msg.str().c_str();
        }
        if (irq) {
            pci->AddIrqHandler(*irq,[this]() {
                return this->irq();
            });
        }
    }

    qh = qhtdPool.Alloc();
    qh->Pointer()->qh.head = UHCI_POINTER_TERMINATE;
    qh->Pointer()->qh.element = UHCI_POINTER_TERMINATE;
    qh->Pointer()->qh.NextEndpoint = nullptr;

    for (int i = 0; i < 0x1F; i++) {
        intqhs[i] = qhtdPool.Alloc();
        auto &qh = intqhs[i]->Pointer()->qh;
        qh.element = UHCI_POINTER_TERMINATE;
        qh.NextEndpoint = nullptr;
    }
    for (int i = 0; i < 0x10; i++) {
        auto &qh = intqhs[i]->Pointer()->qh;
        qh.head = intqhs[0x10 + (i >> 1)]->Phys() | UHCI_POINTER_QH;
    }
    for (int i = 0x10; i < 0x18; i++) {
        auto &qh = intqhs[i]->Pointer()->qh;
        qh.head = intqhs[0x18 + ((i - 0x10) >> 1)]->Phys() | UHCI_POINTER_QH;
    }
    for (int i = 0x18; i < 0x1C; i++) {
        auto &qh = intqhs[i]->Pointer()->qh;
        qh.head = intqhs[0x1C + ((i - 0x18) >> 1)]->Phys() | UHCI_POINTER_QH;
    }
    for (int i = 0x1C; i < 0x1E; i++) {
        auto &qh = intqhs[i]->Pointer()->qh;
        qh.head = intqhs[0x1E]->Phys() | UHCI_POINTER_QH;
    }
    {
        auto &qh = intqhs[0x1E]->Pointer()->qh;
        qh.head = this->qh->Phys() | UHCI_POINTER_QH;
    }

    for (int i = 0; i < 0x20; i++) {
        intqhroots[i] = qhtdPool.Alloc();
        auto &qh = intqhroots[i]->Pointer()->qh;
        qh.head = intqhs[i >> 1]->Phys() | UHCI_POINTER_QH;
        qh.element = UHCI_POINTER_TERMINATE;
        qh.NextEndpoint = nullptr;
    }

    {
        static const uint8_t Balance[16] =
                {0x0, 0x8, 0x4, 0xC, 0x2, 0xA, 0x6, 0xE, 0x1, 0x9, 0x5, 0xD, 0x3, 0xB, 0x7, 0xF};

        for (uint8_t i = 0; i < 16; i++) {
            uint8_t bal = Balance[i] >> 1;
            uint8_t idx = i << 1;
            intqhroots[idx]->Pointer()->qh.head = intqhs[bal]->Phys() | UHCI_POINTER_QH;
            intqhroots[idx + 1]->Pointer()->qh.head = intqhs[8 + bal]->Phys() | UHCI_POINTER_QH;
        }

        for (int i = 0; i < 1024; i++) {
            Frames[i] = UHCI_POINTER_QH | intqhroots[i & 0x1F]->Phys();
        }
    }

    outportl(iobase + REG_FRAME_BASEADDR, FramesPhys.PhysAddr());

    outportw(iobase + REG_STATUS, 0xFFFF); // Clear

    outportw(iobase + REG_INTR, UHCI_INT_INTERRUPT_COMP);

    outportw(iobase + REG_COMMAND, COMMAND_RUN_STOP);

    Run();
}

bool uhci::irq() {
    uint16_t status{inportw(iobase + REG_STATUS)};
    uint16_t clear{0};
    if ((status & UHCI_STATUS_USBINT) != 0) {
        clear |= UHCI_STATUS_USBINT;
        usbint();
    }
    if (clear != 0) {
        outportw(iobase + REG_STATUS, clear);
        return true;
    }
    return false;
}

void uhci::dumpregs() {
}

int uhci::GetNumberOfPorts() {
    return 2;
}

uint32_t uhci::GetPortStatus(int port) {
    uint32_t reg = iobase + REG_PORTSC1 + (port * 2);
    uint32_t hcdStatus{USB_PORT_STATUS_PPS};

    uint16_t status = inportw(reg);

    if ((status & UHCI_PORT_SUSPENDED) != 0) {
        hcdStatus |= USB_PORT_STATUS_PSS;
    }
    if ((status & UHCI_PORT_RESET) != 0) {
        hcdStatus |= USB_PORT_STATUS_PRS;
    }
    if ((status & UHCI_PORT_LOW_SPEED) != 0) {
        hcdStatus |= USB_PORT_STATUS_LSDA;
    }
    if ((status & UHCI_PORT_RESUME_DETECT) != 0) {
        hcdStatus |= USB_PORT_STATUS_RESUME;
    }
    if ((status & UHCI_PORT_ENABLED_CHANGE) != 0) {
        hcdStatus |= USB_PORT_STATUS_PESC;
    }
    if ((status & UHCI_PORT_ENABLED) != 0) {
        hcdStatus |= USB_PORT_STATUS_PES;
    }
    if ((status & UHCI_PORT_CONNECTED_CHANGE) != 0) {
        hcdStatus |= USB_PORT_STATUS_CSC;
    }
    if ((status & UHCI_PORT_CONNECTED) != 0) {
        hcdStatus |= USB_PORT_STATUS_CCS;
    }
    return hcdStatus;
}

void uhci::SwitchPortOff(int port) {
}

void uhci::SwitchPortOn(int port) {
}

void uhci::ClearStatusChange(int port, uint32_t statuses) {
    uint32_t reg = iobase + REG_PORTSC1 + (port * 2);
    uint32_t value = inportw(reg);
    value &= ~(UHCI_PORT_CONNECTED_CHANGE | UHCI_PORT_ENABLED_CHANGE);
    if ((statuses & USB_PORT_STATUS_CSC) != 0) {
        value |= UHCI_PORT_CONNECTED_CHANGE;
    }
    if ((statuses & USB_PORT_STATUS_PESC) != 0) {
        value |= UHCI_PORT_ENABLED_CHANGE;
    }
    outportw(reg, value);
}

std::shared_ptr<usb_endpoint>
uhci::CreateControlEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize,
                            uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) {
    if (speed == LOW || speed == FULL) {
        std::shared_ptr<uhci_endpoint> endpoint{
                new uhci_endpoint(*this, maxPacketSize, functionAddr, endpointNum, speed, usb_endpoint_type::CONTROL)};
        std::shared_ptr<usb_endpoint> usbEndpoint{endpoint};
        return usbEndpoint;
    } else {
        return {};
    }
}

std::shared_ptr<usb_endpoint>
uhci::CreateInterruptEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize,
                              uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed,
                              int pollingIntervalMs) {
    if (speed == LOW || speed == FULL) {
        std::shared_ptr<uhci_endpoint> endpoint{
                new uhci_endpoint(*this, maxPacketSize, functionAddr, endpointNum, speed, usb_endpoint_type::INTERRUPT,
                                  pollingIntervalMs)};
        std::shared_ptr<usb_endpoint> usbEndpoint{endpoint};
        return usbEndpoint;
    } else {
        return {};
    }
}

std::shared_ptr<usb_endpoint>
uhci::CreateBulkEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize,
                         uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) {
    return {};
}

std::shared_ptr<usb_buffer> uhci::Alloc(size_t size) {
    if (size <= UHCI_TRANSFER_SHORT_SIZE) {
        std::shared_ptr<usb_buffer> buffer{new uhci_buffer<UHCI_TRANSFER_SHORT_SIZE>(shortBufPool.Alloc())};
        return buffer;
    } else if (size <= UHCI_TRANSFER_BUFFER_SIZE) {
        std::shared_ptr<usb_buffer> buffer{new uhci_buffer<UHCI_TRANSFER_BUFFER_SIZE>(bufPool.Alloc())};
        return buffer;
    } else {
        return {};
    }
}

void uhci::EnablePort(int port) {
    uint32_t reg = iobase + REG_PORTSC1 + (port * 2);
    outportw(reg, UHCI_PORT_ENABLED);
}

void uhci::DisablePort(int port) {
    uint32_t reg = iobase + REG_PORTSC1 + (port * 2);
    outportw(reg, 0);
}

void uhci::ResetPort(int port) {
    uint32_t reg = iobase + REG_PORTSC1 + (port * 2);
    {
        uint32_t value = inportw(reg);
        value &= ~(UHCI_PORT_CONNECTED_CHANGE | UHCI_PORT_ENABLED_CHANGE);
        value |= UHCI_PORT_RESET;
        outportw(reg, value);
    }
    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(50ms);
    }
    {
        uint32_t value = inportw(reg);
        value &= ~(UHCI_PORT_CONNECTED_CHANGE | UHCI_PORT_ENABLED_CHANGE | UHCI_PORT_RESET);
        outportw(reg, value);
    }
}

usb_speed uhci::PortSpeed(int port) {
    uint32_t reg = iobase + REG_PORTSC1 + (port * 2);
    uint32_t value = inportw(reg);
    return (value & UHCI_PORT_LOW_SPEED) == 0 ? FULL : LOW;
}

void uhci::usbint() {
    std::vector<uhci_endpoint *> watched{};

    std::lock_guard lock{uhcilock};

    for (uhci_endpoint *endpoint : watchList) {
        watched.push_back(endpoint);
    }
    for (uhci_endpoint *endpoint : watched) {
        endpoint->IntWithLock();
    }
}

bool uhci::reset() {
    outportw(iobase + REG_INTR, 0); // Disable intr

    outportw(iobase + REG_COMMAND, COMMAND_GRESET);

    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(100ms);
    }

    outportw(iobase + REG_COMMAND, COMMAND_HCRESET);

    {
        uint16_t cmd{COMMAND_HCRESET};
        for (int i = 0; i < 10; i++) {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10ms);
            cmd = inportw(iobase + REG_COMMAND);
            if ((cmd & COMMAND_HCRESET) == 0) {
                break;
            }
        }
        if ((cmd & COMMAND_HCRESET) != 0) {
            return false;
        }
    }

    {
        uint16_t status{0};
        for (int i = 0; i < 10; i++) {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10ms);
            status = inportw(iobase + REG_STATUS);
            if ((status & UHCI_STATUS_HALTED) != 0) {
                break;
            }
        }
        if ((status & UHCI_STATUS_HALTED) == 0) {
            return false;
        }
    }

    outportw(iobase + REG_FRAME_NUMBER, 0);
    outportb(iobase + REG_SOFMOD, 0x40);

    return true;
}

bool uhci::PollPorts() {
    return true;
}

std::shared_ptr<usb_buffer> uhci_endpoint::Alloc(size_t size) {
    return uhciRef.Alloc(size);
}

uhci_endpoint::uhci_endpoint(uhci &uhciRef, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum,
                             usb_speed speed, usb_endpoint_type endpointType, int pollingRateMs)
                             : uhciRef(uhciRef), speed(speed), endpointType(endpointType), pollingRateMs(pollingRateMs),
                             maxPacketSize(maxPacketSize), functionAddr(functionAddr), endpointNum(endpointNum),
                             head(), qh(), pending(), active() {
    std::lock_guard lock{uhciRef.HcdSpinlock()};
    if (endpointType == usb_endpoint_type::CONTROL) {
        head = uhciRef.qh;
    }
    if (endpointType == usb_endpoint_type::INTERRUPT) {
        if (pollingRateMs < 2) {
            head = uhciRef.intqhs[0x1E]; // 1ms
        } else if (pollingRateMs < 3) {
            head = uhciRef.intqhs[0x1C + (uhciRef.intcycle++ & 1)]; // 2ms
        } else if (pollingRateMs < 5) {
            head = uhciRef.intqhs[0x18 + (uhciRef.intcycle++ & 3)]; // 4ms
        } else if (pollingRateMs < 9) {
            head = uhciRef.intqhs[0x10 + (uhciRef.intcycle++ & 7)]; // 8ms
        } else if (pollingRateMs < 17) {
            head = uhciRef.intqhs[uhciRef.intcycle++ & 0xF]; // 16ms
        } else {
            head = uhciRef.intqhroots[uhciRef.intcycle++ & 0x1F]; // 32ms
        }
    }
    qh = uhciRef.qhtdPool.Alloc();
    auto &qhv = qh->Pointer()->qh;
    qhv.head = head->Pointer()->qh.head;
    qhv.element = UHCI_POINTER_TERMINATE;
    qhv.NextEndpoint = head->Pointer()->qh.NextEndpoint;
    head->Pointer()->qh.head = qh->Phys() | UHCI_POINTER_QH;
    head->Pointer()->qh.NextEndpoint = this;
}

std::shared_ptr<usb_transfer> uhci_endpoint::CreateTransferWithLock(bool commitTransaction, std::shared_ptr<usb_buffer> buffer, uint32_t size,
                                                                    usb_transfer_direction direction,
                                                                    int8_t dataToggle,
                                                                    std::function<void(uhci_transfer &)> &applyFunc) {
    std::shared_ptr<uhci_transfer> transfer = std::make_shared<uhci_transfer>(uhciRef);
    transfer->buffer = buffer;
    auto &td = transfer->TD();
    td.next = UHCI_POINTER_TERMINATE;
    td.ActLen = 0;
    td.Status = UHCI_TD_STATUS_ACTIVE;
    if (speed == LOW) {
        td.Status |= UHCI_TD_STATUS_LOW_SPEED;
    }
    if (commitTransaction) {
        td.Status |= UHCI_TD_STATUS_INTERRUPT_ON_CMP;
    }
    switch (direction) {
        case usb_transfer_direction::IN:
            td.PID = 0x69;
            break;
        case usb_transfer_direction::OUT:
            td.PID = 0xE1;
            break;
        case usb_transfer_direction::SETUP:
            td.PID = 0x2D;
            break;
    }
    td.DeviceAddress = functionAddr;
    td.Endpoint = endpointNum;
    td.DataToggle = dataToggle;
    td.Reserved = 0;
    td.MaxLen = size - 1;
    td.BufferPointer = buffer ? buffer->Addr() : 0;

    applyFunc(*transfer);

    {
        std::shared_ptr<uhci_transfer> pending{this->pending};
        if (pending) {
            while (pending->next) {
                pending = pending->next;
            }
            pending->TD().next = transfer->td->Phys() | UHCI_POINTER_DEPTH;
            pending->next = transfer;
        } else {
            this->pending = transfer;
        }
    }

    if (commitTransaction) {
        IntWithLock();
        bool found{false};
        for (uhci_endpoint *watched : uhciRef.watchList) {
            if (watched == this) {
                found = true;
                break;
            }
        }
        if (!found) {
            uhciRef.watchList.push_back(this);
        }
    }

    std::shared_ptr<usb_transfer> usbTransfer{transfer};
    return usbTransfer;
}

std::shared_ptr<usb_transfer>
uhci_endpoint::CreateTransferWithoutLock(bool commitTransaction, std::shared_ptr<usb_buffer> buffer, uint32_t size,
                                         usb_transfer_direction direction,
                                         int8_t dataToggle, std::function<void(uhci_transfer &)> &applyFunc) {
    std::lock_guard lock{uhciRef.HcdSpinlock()};

    return CreateTransferWithLock(commitTransaction, buffer, size, direction, dataToggle, applyFunc);
}

std::shared_ptr<usb_transfer>
uhci_endpoint::CreateTransfer(bool commitTransaction, void *data, uint32_t size, usb_transfer_direction direction, bool bufferRounding,
                              uint16_t delayInterrupt, int8_t dataToggle) {
    std::shared_ptr<usb_buffer> buffer = Alloc(size);
    memcpy(buffer->Pointer(), data, size);
    std::function<void (uhci_transfer &)> applyFunc = [] (uhci_transfer &) {};
    return CreateTransferWithoutLock(commitTransaction, buffer, size, direction, dataToggle, applyFunc);
}

std::shared_ptr<usb_transfer>
uhci_endpoint::CreateTransfer(bool commitTransaction, void *data, uint32_t size, usb_transfer_direction direction, std::function<void()> doneCall, bool bufferRounding,
                              uint16_t delayInterrupt, int8_t dataToggle) {
    std::shared_ptr<usb_buffer> buffer = Alloc(size);
    memcpy(buffer->Pointer(), data, size);
    std::function<void (uhci_transfer &)> applyFunc = [doneCall] (uhci_transfer &transfer) {
        transfer.SetDoneCall(doneCall);
    };
    return CreateTransferWithoutLock(commitTransaction, buffer, size, direction, dataToggle, applyFunc);
}

std::shared_ptr<usb_transfer>
uhci_endpoint::CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction, bool bufferRounding,
                              uint16_t delayInterrupt, int8_t dataToggle) {
    std::shared_ptr<usb_buffer> buffer{};
    if (size > 0) {
        buffer = Alloc(size);
    }
    std::function<void (uhci_transfer &)> applyFunc = [] (uhci_transfer &) {};
    return CreateTransferWithoutLock(commitTransaction, buffer, size, direction, dataToggle, applyFunc);
}

std::shared_ptr<usb_transfer>
uhci_endpoint::CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction, std::function<void()> doneCall,
                              bool bufferRounding, uint16_t delayInterrupt, int8_t dataToggle) {
    std::shared_ptr<usb_buffer> buffer{};
    if (size > 0) {
        buffer = Alloc(size);
    }
    std::function<void (uhci_transfer &)> applyFunc = [doneCall] (uhci_transfer &transfer) {
        transfer.SetDoneCall(doneCall);
    };
    return CreateTransferWithoutLock(commitTransaction, buffer, size, direction, dataToggle, applyFunc);
}

std::shared_ptr<usb_transfer>
uhci_endpoint::CreateTransferWithLock(bool commitTransaction, const void *data, uint32_t size,
                                      usb_transfer_direction direction, std::function<void ()> doneCall,
                                      bool bufferRounding, uint16_t delayInterrupt, int8_t dataToggle) {
    std::shared_ptr<usb_buffer> buffer{};
    if (size > 0) {
        buffer = Alloc(size);
        memcpy(buffer->Pointer(), data, size);
    }
    std::function<void (uhci_transfer &)> applyFunc = [doneCall] (uhci_transfer &transfer) {
        transfer.SetDoneCall(doneCall);
    };
    return CreateTransferWithLock(commitTransaction, buffer, size, direction, dataToggle, applyFunc);
}

std::shared_ptr<usb_transfer>
uhci_endpoint::CreateTransferWithLock(bool commitTransaction, uint32_t size, usb_transfer_direction direction, std::function<void()> doneCall,
                                      bool bufferRounding, uint16_t delayInterrupt, int8_t dataToggle) {
    std::shared_ptr<usb_buffer> buffer{};
    if (size > 0) {
        buffer = Alloc(size);
    }
    std::function<void (uhci_transfer &)> applyFunc = [doneCall] (uhci_transfer &transfer) {
        transfer.SetDoneCall(doneCall);
    };
    return CreateTransferWithLock(commitTransaction, buffer, size, direction, dataToggle, applyFunc);
}

uhci_endpoint::~uhci_endpoint() {
    if (endpointType == usb_endpoint_type::CONTROL || endpointType == usb_endpoint_type::INTERRUPT) {
        std::lock_guard lock{uhciRef.HcdSpinlock()};
        {
            auto iter = uhciRef.watchList.begin();
            while (iter != uhciRef.watchList.end()) {
                uhci_endpoint *endpoint = *iter;
                if (endpoint == this) {
                    uhciRef.watchList.erase(iter);
                } else {
                    ++iter;
                }
            }
        }
        auto *qh = &(head->Pointer()->qh);
        bool found{false};
        while (qh != nullptr) {
            auto *endpoint = qh->NextEndpoint;
            if (this == endpoint) {
                auto *nqh = &(endpoint->qh->Pointer()->qh);
                qh->head = nqh->head;
                qh->NextEndpoint = nqh->NextEndpoint;
                found = true;
                break;
            }
            qh = endpoint != nullptr ? &(endpoint->qh->Pointer()->qh) : nullptr;
        }
        if (!found) {
            wild_panic("Endpoint destruction but did not find the endpoint");
        }
        std::shared_ptr<uhci_endpoint_cleanup> cleanup = std::make_shared<uhci_endpoint_cleanup>();
        cleanup->qh = this->qh;
        cleanup->transfers = this->active;
        uhciRef.delayedDestruction.push_back(cleanup);
    }
}

void uhci_endpoint::IntWithLock() {
    auto &qhv = qh->Pointer()->qh;
    std::vector<std::shared_ptr<uhci_transfer>> done{};
    while (active) {
        if (active->td->Phys() == (qhv.element & 0xFFFFFFF0)) {
            break;
        }
        done.push_back(active);
        active = active->next;
    }
    if (!active) {
        if (this->pending) {
            active = this->pending;
            this->pending = std::shared_ptr<uhci_transfer>();
            qhv.element = active->td->Phys();
        } else {
            auto iter = uhciRef.watchList.begin();
            while (iter != uhciRef.watchList.end()) {
                uhci_endpoint *endpoint = *iter;
                if (endpoint == this) {
                    uhciRef.watchList.erase(iter);
                } else {
                    ++iter;
                }
            }
        }
    }
    for (auto &transfer : done) {
        transfer->SetDone();
    }
}

bool uhci_endpoint::ClearStall() {
    return false;
}

uhci_transfer::uhci_transfer(uhci &uhciRef) : td(uhciRef.qhtdPool.Alloc()), buffer(), next() {
}

usb_transfer_status uhci_transfer::GetStatus() {
    auto &td = TD();
    if ((td.Status & UHCI_TD_STATUS_ACTIVE) != 0) {
        return usb_transfer_status::NOT_ACCESSED;
    }
    if ((td.Status & UHCI_TD_STATUS_STALL) != 0) {
        return usb_transfer_status::STALL;
    }
    if ((td.Status & UHCI_TD_STATUS_DATABUF_ERR) != 0) {
        return usb_transfer_status::BUFFER_UNDERRUN;
    }
    if ((td.Status & UHCI_TD_STATUS_BABBLE) != 0) {
        return usb_transfer_status::DEVICE_NOT_RESPONDING;
    }
    if ((td.Status & UHCI_TD_STATUS_NAK) != 0) {
        return usb_transfer_status::UNKNOWN_ERROR;
    }
    if ((td.Status & UHCI_TD_STATUS_CRC_OR_TIMEOUT) != 0) {
        return usb_transfer_status::CRC;
    }
    if ((td.Status & UHCI_TD_STATUS_BITSTUFF_ERR) != 0) {
        return usb_transfer_status::BIT_STUFFING;
    }
    return usb_transfer_status::NO_ERROR;
}
