//
// Created by sigsegv on 28.05.2021.
//

#include <sstream>
#include <klogger.h>
#include <cpuio.h>
#include <thread>
#include "ehci.h"
#include "UsbBuffer32.h"

#define EHCI_CMD_RUN_STOP   1
#define EHCI_CMD_HCRESET    (1 << 1)
#define EHCI_CMD_ITC_1MF    (1 << 16)
#define EHCI_CMD_ITC_2MF    (2 << 16)
#define EHCI_CMD_ITC_4MF    (4 << 16)
#define EHCI_CMD_ITC_8MF    (8 << 16)
#define EHCI_CMD_ITC_16MF   (16 << 16)
#define EHCI_CMD_ITC_32MF   (32 << 16)
#define EHCI_CMD_ITC_64MF   (64 << 16)
#define EHCI_CMD_ITC_MASK   (0xFF << 16)
#define EHCI_CMD_ASYNC_DOORBELL (1 << 6)
#define EHCI_CMD_ASYNC_ENABLE   (1 << 5)
#define EHCI_CMD_PERIODIC_ENABLE    (1 << 4)


#define EHCI_STATUS_HALTED              (1 << 12)
#define EHCI_STATUS_ASYNC_ADVANCE       (1 << 5)
#define EHCI_STATUS_HOST_SYSTEM_ERROR   (1 << 4)
#define EHCI_STATUS_FRAME_LIST_ROLLOVER (1 << 3)
#define EHCI_STATUS_PORT_CHANGE         (1 << 2)
#define EHCI_STATUS_USB_ERROR           (1 << 1)
#define EHCI_STATUS_USBINT              1

#define EHCI_HCSPARAMS_PORT_POWER_CONTROL   0x10

#define EHCI_PORT_LINE_MASK             (3 << 10)
#define EHCI_PORT_LINE_LOWSPEED         (1 << 10)

#define EHCI_PORT_COMPANION_OWNS        (1 << 13)
#define EHCI_PORT_POWER                 (1 << 12)
#define EHCI_PORT_RESET                 (1 << 8)
#define EHCI_PORT_SUSPEND               (1 << 7)
#define EHCI_PORT_FORCE_RESUME          (1 << 6)
#define EHCI_PORT_OVERCURRENT_CHANGE    (1 << 5)
#define EHCI_PORT_OVERCURRENT           (1 << 4)
#define EHCI_PORT_ENABLE_CHANGE         (1 << 3)
#define EHCI_PORT_ENABLE                (1 << 2)
#define EHCI_PORT_CONNECT_CHANGE        (1 << 1)
#define EHCI_PORT_CONNECT               1

#define EHCI_INT_ASYNC_ADVANCE          (1 << 5)
#define EHCI_INT_HOST_SYSTEM_ERROR      (1 << 4)
#define EHCI_INT_FRAME_LIST_ROLLOVER    (1 << 3)
#define EHCI_INT_PORT_CHANGE            (1 << 2)
#define EHCI_INT_USB_ERROR              (1 << 1)
#define EHCI_INT_USBINT                 1

#define EHCI_QTD_STATUS_ACTIVE              (1 << 7)
#define EHCI_QTD_STATUS_HALTED              (1 << 6)
#define EHCI_QTD_STATUS_DATA_BUFFER_ERROR   (1 << 5)
#define EHCI_QTD_STATUS_BABBLE              (1 << 4)
#define EHCI_QTD_STATUS_TRANSACTION_ERROR   (1 << 3)
#define EHCI_QTD_STATUS_MISSED_MICRO_FRAME  (1 << 2)
#define EHCI_QTD_STATUS_SPLIT_X_STATE       (1 << 1)
#define EHCI_QTD_STATUS_PING_OR_ERR         (1 << 0)

Device *ehci_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    if (
            deviceInformation.device_class == 0x0C /* serial bus */ &&
            deviceInformation.device_subclass == 3 /* USB */ &&
            deviceInformation.prog_if == 0x20 /* ehci */
    ) {
        return new ehci(bus, *(deviceInformation.GetPciInformation()));
    } else {
        return nullptr;
    }
}

#define DEBUG_EHCI_INIT_OWNER

void ehci::init() {
    {
        uint64_t addr{0};
        uint64_t size{0};
        bool prefetch{false};
        {
            PciBaseAddressRegister bar0 = pciDeviceInformation.readBaseAddressRegister(0);
            if (!bar0.is_memory()) {
                get_klogger() << "USB ehci controller is invalid (invalid BAR0, not memory)\n";
                return;
            }
            if (bar0.is_64bit()) {
                addr = bar0.addr64();
            } else if (bar0.is_32bit()) {
                addr = (uint64_t) bar0.addr32();
            } else {
                get_klogger() << "USB ehci controller is invalid (invalid BAR0, invalid record)\n";
                return;
            }
            size = bar0.memory_size();
            if (size == 0) {
                get_klogger() << "USB ehci controller is invalid (invalid BAR0, invalid size)\n";
                return;
            }
            prefetch = bar0.is_prefetchable();
        }
#ifdef DEBUG_EHCI_INIT_OWNER
        {
            std::stringstream msg;
            msg << DeviceType() << (unsigned int) DeviceId() << ": memory mapped registers at 0x" << std::hex << addr << " size 0x" << size << "\n";
            get_klogger() << msg.str().c_str();
        }
#endif
        uint64_t pg_offset{addr & 0xFFF};
        mapped_registers_vm = std::make_unique<vmem>(size + pg_offset);
        {
            uint64_t physaddr{addr & 0xFFFFFFFFFFFFF000};
            std::size_t pages = mapped_registers_vm->npages();
            for (std::size_t page = 0; page < pages; page++) {
                mapped_registers_vm->page(page).rwmap(physaddr, true, !prefetch);
                physaddr += 0x1000;
            }
        }
        caps = (ehci_capabilities *) (void *) (((uint8_t *) mapped_registers_vm->pointer()) + pg_offset);
        regs = caps->opregs();
    }
    {
        uint8_t eecp1_off = (uint8_t) ((caps->hccparams & 0x0000FF00) >> 8);
        if (eecp1_off >= 0x40) {
#ifdef DEBUG_EHCI_INIT_OWNER
            {
                std::stringstream msg;
                msg << DeviceType() << (unsigned int) DeviceId() << ": Reading ver 0x"<<std::hex<<(unsigned int) caps->hciversion << " par 0x" << caps->hccparams <<"\n";
                get_klogger() << msg.str().c_str();
            }
#endif
            std::unique_ptr<EECP> eecp = std::make_unique<EECP>(pciDeviceInformation, eecp1_off);
            do {
                if (eecp->capability_id == 1) {
                    if ((eecp->refresh().value & EHCI_LEGACY_BIOS) != 0) {
                        // Claim ehci
                        eecp->write(eecp->value | EHCI_LEGACY_OWNED);
                        {
                            std::stringstream msg;
                            msg << DeviceType() << (unsigned int) DeviceId() << ": Disabling legacy support\n";
                            get_klogger() << msg.str().c_str();
                        }
                        do {
                            using namespace std::literals::chrono_literals;
                            std::this_thread::sleep_for(10ms);
                            eecp->refresh();
                        } while ((eecp->value & EHCI_LEGACY_OWNED) == 0 || (eecp->value & EHCI_LEGACY_BIOS) != 0);
                        {
                            std::stringstream msg;
                            msg << DeviceType() << (unsigned int) DeviceId() << ": Disabled legacy support\n";
                            get_klogger() << msg.str().c_str();
                        }
                    } else if ((eecp->refresh().value & EHCI_LEGACY_OWNED) == 0) {
                        std::stringstream msg;
                        msg << DeviceType() << (unsigned int) DeviceId() << ": Legacy was off 0x"<<std::hex<<eecp->value<<", claiming ownership - ";
                        get_klogger() << msg.str().c_str();

                        eecp->write8(3, 1);

                        uint8_t timeout = 100;
                        do {
                            if (--timeout == 0) {
                                break;
                            }
                            using namespace std::literals::chrono_literals;
                            std::this_thread::sleep_for(10ms);
                            eecp->refresh();
                        } while ((eecp->value & EHCI_LEGACY_OWNED) == 0 || (eecp->value & EHCI_LEGACY_BIOS) != 0);
                        if (timeout != 0) {
                            get_klogger() << "ownership claimed\n";
                        } else {
                            get_klogger() << "ownership claim timeout\n";
                        }
                    } else {
                        std::stringstream msg;
                        msg << DeviceType() << (unsigned int) DeviceId() << ": Legacy support is off 0x"<<std::hex<<eecp->value<<"\n";
                        get_klogger() << msg.str().c_str();
                    }
                } else {
                    std::stringstream msg;
                    msg << DeviceType() << (unsigned int) DeviceId() << ": EECP type "
                        << (unsigned int) eecp->capability_id << "\n";
                    get_klogger() << msg.str().c_str();
                }
                eecp = eecp->next();
            } while (eecp);
        } else {
            std::stringstream msg;
            msg << DeviceType() << (unsigned int) DeviceId() << ": No legacy support to disable\n";
            get_klogger() << msg.str().c_str();
        }
    }
    regs->usbCommand &= ~EHCI_CMD_RUN_STOP;
    if ((regs->usbStatus & EHCI_STATUS_HALTED) == 0) {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(10ms);
        if ((regs->usbStatus & EHCI_STATUS_HALTED) == 0) {
            std::stringstream msg;
            msg << DeviceType() << (unsigned int) DeviceId() << "error: USB2 controller failed to stop\n";
            get_klogger() << msg.str().c_str();
            return;
        }
    }

    regs->usbCommand |= EHCI_CMD_HCRESET;
    {
        int timeout = 50;
        while ((regs->usbCommand & EHCI_CMD_HCRESET) != 0) {
            if (--timeout == 0) {
                std::stringstream msg;
                msg << DeviceType() << (unsigned int) DeviceId() << "error: USB2 controller failed to reset\n";
                get_klogger() << msg.str().c_str();
                return;
            }
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(100ms);
        }
    }

    regs->usbCommand &= ~EHCI_CMD_RUN_STOP;
    if ((regs->usbStatus & EHCI_STATUS_HALTED) == 0) {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(10ms);
        if ((regs->usbStatus & EHCI_STATUS_HALTED) == 0) {
            std::stringstream msg;
            msg << DeviceType() << (unsigned int) DeviceId() << "error: USB2 controller failed to stop\n";
            get_klogger() << msg.str().c_str();
            return;
        }
    }

    {
        uint32_t cmd{regs->usbCommand};
        cmd &= ~(EHCI_CMD_ITC_MASK | EHCI_CMD_ASYNC_ENABLE | EHCI_CMD_PERIODIC_ENABLE);
        cmd |= EHCI_CMD_ITC_8MF;
        regs->usbCommand = cmd;
    }

    if (regs->ctrlDsSegment != 0) {
        regs->ctrlDsSegment = 0;
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
    qh->Pointer()->qh.HorizLink = qh->Phys() | EHCI_POINTER_QH;
    qh->Pointer()->qh.DeviceAddress = 0;
    qh->Pointer()->qh.InactivateOnNext = false;
    qh->Pointer()->qh.EndpointNumber = 0;
    qh->Pointer()->qh.EndpointSpeed = EHCI_QH_ENDPOINT_SPEED_HIGH;
    qh->Pointer()->qh.DataToggleControl = 1;
    qh->Pointer()->qh.HeadOfReclamationList = true;
    qh->Pointer()->qh.MaxPacketLength = 0;
    qh->Pointer()->qh.ControlEndpointFlag = false;
    qh->Pointer()->qh.NakCountReload = 0;
    qh->Pointer()->qh.InterruptScheduleMask = 0;
    qh->Pointer()->qh.SplitCompletionMask = 0;
    qh->Pointer()->qh.HubAddress = 0;
    qh->Pointer()->qh.PortNumber = 0;
    qh->Pointer()->qh.HighBandwidthPipeMultiplier = 1;
    qh->Pointer()->qh.CurrentQTd = EHCI_POINTER_TERMINATE;
    qh->Pointer()->qh.NextQTd = EHCI_POINTER_TERMINATE;
    qh->Pointer()->qh.AlternateQTd = EHCI_POINTER_TERMINATE;
    qh->Pointer()->qh.Overlay[0] = 0;
    qh->Pointer()->qh.Overlay[1] = 0;
    qh->Pointer()->qh.Overlay[2] = 0;
    qh->Pointer()->qh.Overlay[3] = 0;
    qh->Pointer()->qh.Overlay[4] = 0;
    qh->Pointer()->qh.Overlay[5] = 0;
    qh->Pointer()->qh.NextEndpoint = nullptr;

    regs->asyncListAddr = qh->Phys();

    for (int i = 0; i < 0x1F; i++) {
        intqhs[i] = qhtdPool.Alloc();
        auto &qh = intqhs[i]->Pointer()->qh;
        qh.HorizLink = EHCI_POINTER_TERMINATE;
        qh.DeviceAddress = 0;
        qh.InactivateOnNext = false;
        qh.EndpointNumber = 0;
        qh.EndpointSpeed = EHCI_QH_ENDPOINT_SPEED_HIGH;
        qh.DataToggleControl = 1;
        qh.HeadOfReclamationList = false;
        qh.MaxPacketLength = 0;
        qh.ControlEndpointFlag = false;
        qh.NakCountReload = 0;
        qh.InterruptScheduleMask = 1;
        qh.SplitCompletionMask = 0;
        qh.HubAddress = 0;
        qh.PortNumber = 0;
        qh.HighBandwidthPipeMultiplier = 1;
        qh.CurrentQTd = EHCI_POINTER_TERMINATE;
        qh.NextQTd = EHCI_POINTER_TERMINATE;
        qh.AlternateQTd = EHCI_POINTER_TERMINATE;
        qh.Overlay[0] = 0;
        qh.Overlay[1] = 0;
        qh.Overlay[2] = 0;
        qh.Overlay[3] = 0;
        qh.Overlay[4] = 0;
        qh.Overlay[5] = 0;
        qh.NextEndpoint = nullptr;
    }
    for (int i = 0; i < 0x10; i++) {
        auto &qh = intqhs[i]->Pointer()->qh;
        qh.HorizLink = intqhs[0x10 + (i >> 1)]->Phys() | EHCI_POINTER_QH;
    }
    for (int i = 0x10; i < 0x18; i++) {
        auto &qh = intqhs[i]->Pointer()->qh;
        qh.HorizLink = intqhs[0x18 + ((i - 0x10) >> 1)]->Phys() | EHCI_POINTER_QH;
    }
    for (int i = 0x18; i < 0x1C; i++) {
        auto &qh = intqhs[i]->Pointer()->qh;
        qh.HorizLink = intqhs[0x1C + ((i - 0x18) >> 1)]->Phys() | EHCI_POINTER_QH;
    }
    for (int i = 0x1C; i < 0x1E; i++) {
        auto &qh = intqhs[i]->Pointer()->qh;
        qh.HorizLink = intqhs[0x1E]->Phys() | EHCI_POINTER_QH;
    }
    {
        auto &qh = intqhs[0x1E]->Pointer()->qh;
        qh.HorizLink = EHCI_POINTER_TERMINATE;
    }

    for (int i = 0; i < 0x20; i++) {
        intqhroots[i] = qhtdPool.Alloc();
        auto &qh = intqhroots[i]->Pointer()->qh;
        qh.HorizLink = EHCI_POINTER_TERMINATE;
        qh.DeviceAddress = 0;
        qh.InactivateOnNext = false;
        qh.EndpointNumber = 0;
        qh.EndpointSpeed = EHCI_QH_ENDPOINT_SPEED_HIGH;
        qh.DataToggleControl = 1;
        qh.HeadOfReclamationList = false;
        qh.MaxPacketLength = 0;
        qh.ControlEndpointFlag = false;
        qh.NakCountReload = 0;
        qh.InterruptScheduleMask = 0;
        qh.SplitCompletionMask = 0;
        qh.HubAddress = 0;
        qh.PortNumber = 0;
        qh.HighBandwidthPipeMultiplier = 1;
        qh.CurrentQTd = EHCI_POINTER_TERMINATE;
        qh.NextQTd = EHCI_POINTER_TERMINATE;
        qh.AlternateQTd = EHCI_POINTER_TERMINATE;
        qh.Overlay[0] = 0;
        qh.Overlay[1] = 0;
        qh.Overlay[2] = 0;
        qh.Overlay[3] = 0;
        qh.Overlay[4] = 0;
        qh.Overlay[5] = 0;
        qh.NextEndpoint = nullptr;
    }

    {
        static const uint8_t Balance[16] =
                {0x0, 0x8, 0x4, 0xC, 0x2, 0xA, 0x6, 0xE, 0x1, 0x9, 0x5, 0xD, 0x3, 0xB, 0x7, 0xF};

        for (uint8_t i = 0; i < 16; i++) {
            uint8_t bal = Balance[i] >> 1;
            uint8_t idx = i << 1;
            intqhroots[idx]->Pointer()->qh.HorizLink = intqhs[bal]->Phys() | EHCI_POINTER_QH;
            intqhroots[idx + 1]->Pointer()->qh.HorizLink = intqhs[8 + bal]->Phys() | EHCI_POINTER_QH;
        }

        for (int i = 0; i < 1024; i++) {
            Frames[i] = EHCI_POINTER_QH | intqhroots[i & 0x1F]->Phys();
        }
    }

    regs->periodicListBase = FramesPhys.PhysAddr();

    regs->usbCommand |= EHCI_CMD_ASYNC_ENABLE | EHCI_CMD_PERIODIC_ENABLE | EHCI_CMD_RUN_STOP;

    numPorts = (caps->hcsparams & 0xF);
    portPower = (caps->hcsparams & EHCI_HCSPARAMS_PORT_POWER_CONTROL) != 0;

    regs->usbInterruptEnable = EHCI_INT_ASYNC_ADVANCE | EHCI_INT_HOST_SYSTEM_ERROR | EHCI_INT_FRAME_LIST_ROLLOVER | EHCI_INT_PORT_CHANGE | EHCI_INT_USB_ERROR | EHCI_INT_USBINT;

    regs->configFlag |= 1;

    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": USB2 controller caps 0x" << std::hex << caps->hccparams << "\n";
        get_klogger() << msg.str().c_str();
    }

    Run();
}

void ehci::dumpregs() {
}

int ehci::GetNumberOfPorts() {
    return numPorts;
}

uint32_t ehci::GetPortStatus(int port) {
    uint32_t status{0};
    uint32_t ehci_status = regs->portStatusControl[port];
    if ((ehci_status & EHCI_PORT_POWER) != 0) {
        status |= USB_PORT_STATUS_PPS;
    }
    if ((ehci_status & EHCI_PORT_RESET) != 0) {
        status |= USB_PORT_STATUS_PRS;
    }
    if ((ehci_status & EHCI_PORT_OVERCURRENT_CHANGE) != 0) {
        status |= USB_PORT_STATUS_OCIC;
    }
    if ((ehci_status & EHCI_PORT_OVERCURRENT) != 0) {
        status |= USB_PORT_STATUS_POCI;
    }
    if ((ehci_status & EHCI_PORT_ENABLE_CHANGE) != 0) {
        status |= USB_PORT_STATUS_PESC;
    }
    if ((ehci_status & EHCI_PORT_ENABLE) != 0) {
        status |= USB_PORT_STATUS_PES;
    }
    if ((ehci_status & EHCI_PORT_CONNECT_CHANGE) != 0) {
        status |= USB_PORT_STATUS_CSC;
    }
    if ((ehci_status & EHCI_PORT_CONNECT) != 0) {
        status |= USB_PORT_STATUS_CCS;
    }
    return status;
}

void ehci::SwitchPortOff(int port) {
    if (portPower) {
        uint32_t cmd{regs->portStatusControl[port]};
        cmd &= ~(EHCI_PORT_ENABLE_CHANGE | EHCI_PORT_CONNECT_CHANGE | EHCI_PORT_OVERCURRENT_CHANGE | EHCI_PORT_POWER);
        regs->portStatusControl[port] = cmd;
    }
}

void ehci::SwitchPortOn(int port) {
    if (portPower) {
        uint32_t cmd{regs->portStatusControl[port]};
        cmd &= ~(EHCI_PORT_ENABLE_CHANGE | EHCI_PORT_CONNECT_CHANGE | EHCI_PORT_OVERCURRENT_CHANGE);
        cmd |= EHCI_PORT_POWER;
        regs->portStatusControl[port] = cmd;
    }
}

void ehci::EnablePort(int port) {
    uint32_t cmd{regs->portStatusControl[port]};
    cmd &= ~(EHCI_PORT_ENABLE_CHANGE | EHCI_PORT_CONNECT_CHANGE | EHCI_PORT_OVERCURRENT_CHANGE);
    cmd |= EHCI_PORT_ENABLE;
    regs->portStatusControl[port] = cmd;
}

void ehci::DisablePort(int port) {
    uint32_t cmd{regs->portStatusControl[port]};
    cmd &= ~(EHCI_PORT_ENABLE_CHANGE | EHCI_PORT_CONNECT_CHANGE | EHCI_PORT_OVERCURRENT_CHANGE | EHCI_PORT_ENABLE);
    regs->portStatusControl[port] = cmd;
}

void ehci::ResetPort(int port) {
    {
        uint32_t cmd{regs->portStatusControl[port]};
        bool portEnabled = (cmd & EHCI_PORT_ENABLE) != 0;
        cmd &= ~(EHCI_PORT_ENABLE_CHANGE | EHCI_PORT_CONNECT_CHANGE | EHCI_PORT_OVERCURRENT_CHANGE | EHCI_PORT_ENABLE);
        if (!portEnabled && (cmd & EHCI_PORT_LINE_MASK) == EHCI_PORT_LINE_LOWSPEED) {
            if ((cmd & EHCI_PORT_CONNECT) != 0) {
                cmd |= EHCI_PORT_COMPANION_OWNS;
                regs->portStatusControl[port] = cmd;
                std::stringstream msg;
                msg << DeviceType() << (unsigned int) DeviceId() << ": Low speed device handed off to companion\n";
                get_klogger() << msg.str().c_str();
            } else {
                std::stringstream msg;
                msg << DeviceType() << (unsigned int) DeviceId() << ": Lost connection during port reset\n";
                get_klogger() << msg.str().c_str();
            }
            return;
        }
        cmd |= EHCI_PORT_RESET;
        regs->portStatusControl[port] = cmd;
    }
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(50ms);
    {
        uint32_t cmd{regs->portStatusControl[port]};
        cmd &= ~(EHCI_PORT_ENABLE_CHANGE | EHCI_PORT_CONNECT_CHANGE | EHCI_PORT_OVERCURRENT_CHANGE | EHCI_PORT_RESET);
        regs->portStatusControl[port] = cmd;
    }
    int timeout{5};
    while (timeout > 0) {
        std::this_thread::sleep_for(10ms);
        if ((regs->portStatusControl[port] & EHCI_PORT_ENABLE) != 0) {
            return;
        }
    }
    uint32_t cmd{regs->portStatusControl[port]};
    cmd &= ~(EHCI_PORT_ENABLE_CHANGE | EHCI_PORT_CONNECT_CHANGE | EHCI_PORT_OVERCURRENT_CHANGE | EHCI_PORT_RESET);
    if ((cmd & EHCI_PORT_CONNECT) != 0) {
        cmd |= EHCI_PORT_COMPANION_OWNS;
        regs->portStatusControl[port] = cmd;
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": Full speed device handed off\n";
        get_klogger() << msg.str().c_str();
    } else {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": Lost connection with full/high speed during port reset\n";
        get_klogger() << msg.str().c_str();
    }
    return;
}

usb_speed ehci::PortSpeed(int port) {
    return HIGH;
}

void ehci::ClearStatusChange(int port, uint32_t statuses) {
    uint32_t ehci_clear{regs->portStatusControl[port]};
    ehci_clear &= ~(EHCI_PORT_ENABLE_CHANGE | EHCI_PORT_CONNECT_CHANGE | EHCI_PORT_OVERCURRENT_CHANGE);
    if ((statuses & USB_PORT_STATUS_PESC) != 0) {
        ehci_clear |= EHCI_PORT_ENABLE_CHANGE;
    }
    if ((statuses & USB_PORT_STATUS_CSC) != 0) {
        ehci_clear |= EHCI_PORT_CONNECT_CHANGE;
    }
    if ((statuses & USB_PORT_STATUS_OCIC) != 0) {
        ehci_clear |= EHCI_PORT_OVERCURRENT_CHANGE;
    }
    regs->portStatusControl[port] = ehci_clear;
}

std::shared_ptr<usb_endpoint>
ehci::CreateControlEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize,
                            uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir,
                            usb_speed speed) {
    std::shared_ptr<ehci_endpoint> endpoint{
            new ehci_endpoint(*this, portRouting, hubAddress, maxPacketSize, functionAddr, endpointNum, speed, usb_endpoint_type::CONTROL)};
    std::shared_ptr<usb_endpoint> usbEndpoint{endpoint};
    return usbEndpoint;
}

std::shared_ptr<usb_endpoint>
ehci::CreateInterruptEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize,
                              uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir,
                              usb_speed speed, int pollingIntervalMs) {
    std::shared_ptr<ehci_endpoint> endpoint{
            new ehci_endpoint(*this, portRouting, hubAddress, maxPacketSize, functionAddr, endpointNum, speed, usb_endpoint_type::INTERRUPT,
                              pollingIntervalMs)};
    std::shared_ptr<usb_endpoint> usbEndpoint{endpoint};
    return usbEndpoint;
}

bool ehci::irq() {
    bool handled{false};
    std::lock_guard lock{ehcilock};
    while (true) {
        uint32_t clear{0};
        uint32_t status{regs->usbStatus};
        if ((status & EHCI_STATUS_ASYNC_ADVANCE) != 0) {
            clear |= EHCI_STATUS_ASYNC_ADVANCE;
            asyncAdvance();
        }
        if ((status & EHCI_STATUS_HOST_SYSTEM_ERROR) != 0) {
            clear |= EHCI_STATUS_HOST_SYSTEM_ERROR;
            get_klogger() << "EHCI host system error\n";
        }
        if ((status & EHCI_STATUS_FRAME_LIST_ROLLOVER) != 0) {
            clear |= EHCI_STATUS_FRAME_LIST_ROLLOVER;
            frameListRollover();
        }
        if ((status & EHCI_STATUS_PORT_CHANGE) != 0) {
            clear |= EHCI_STATUS_PORT_CHANGE;
            RootHubStatusChange();
        }
        if ((status & EHCI_STATUS_USB_ERROR) != 0) {
            clear |= EHCI_STATUS_USB_ERROR;
            get_klogger() << "EHCI USB error\n";
            usbint();
        }
        if ((status & EHCI_STATUS_USBINT) != 0) {
            clear |= EHCI_STATUS_USBINT;
            usbint();
        }
        if (clear != 0) {
            handled = true;
            regs->usbStatus = clear;
        } else {
            break;
        }
    }
    return handled;
}

void ehci::frameListRollover() {
}

void ehci::usbint() {
    std::vector<ehci_endpoint *> watched{};

    for (ehci_endpoint *endpoint : watchList) {
        watched.push_back(endpoint);
    }
    for (ehci_endpoint *endpoint : watched) {
        endpoint->IntWithLock();
    }
}

void ehci::asyncAdvance() {
    nextForDestruction.clear();
    for (std::shared_ptr<ehci_endpoint_cleanup> destroy : delayedDestruction) {
        nextForDestruction.push_back(destroy);
    }
    delayedDestruction.clear();
}

ehci_transfer::ehci_transfer(ehci &ehciRef) : td(ehciRef.qhtdPool.Alloc()), buffer(), next() {
}

usb_transfer_status ehci_transfer::GetStatus() {
    uint8_t status{qTD().status};
    if ((status & EHCI_QTD_STATUS_ACTIVE) != 0) {
        return usb_transfer_status::NOT_ACCESSED;
    }
    if ((status & EHCI_QTD_STATUS_HALTED) != 0) {
        if ((status & EHCI_QTD_STATUS_DATA_BUFFER_ERROR) != 0) {
            return usb_transfer_status::DATA_UNDERRUN;
        }
        if ((status & EHCI_QTD_STATUS_BABBLE) != 0) {
            return usb_transfer_status::BIT_STUFFING;
        }
        return usb_transfer_status::STALL;
    }
    return usb_transfer_status::NO_ERROR;
}

ehci_endpoint::ehci_endpoint(ehci &ehciRef, const std::vector<uint8_t> &portRouting, uint8_t hubAddress,
                             uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_speed speed,
                             usb_endpoint_type endpointType, int pollingRateMs) :
                             ehciRef(ehciRef), head(), qh(ehciRef.qhtdPool.Alloc()), endpointType(endpointType) {
    if (endpointType == usb_endpoint_type::CONTROL) {
        head = ehciRef.qh;
    }
    if (endpointType == usb_endpoint_type::INTERRUPT) {
        if (pollingRateMs < 2) {
            head = ehciRef.intqhs[0x1E]; // 1ms
        } else if (pollingRateMs < 3) {
            head = ehciRef.intqhs[0x1C + (ehciRef.intcycle++ & 1)]; // 2ms
        } else if (pollingRateMs < 5) {
            head = ehciRef.intqhs[0x18 + (ehciRef.intcycle++ & 3)]; // 4ms
        } else if (pollingRateMs < 9) {
            head = ehciRef.intqhs[0x10 + (ehciRef.intcycle++ & 7)]; // 8ms
        } else if (pollingRateMs < 17) {
            head = ehciRef.intqhs[ehciRef.intcycle++ & 0xF]; // 16ms
        } else {
            head = ehciRef.intqhroots[ehciRef.intcycle++ & 0x1F]; // 32ms
        }
    }
    if (head) {
        critical_section cli{};
        std::lock_guard lock{ehciRef.ehcilock};
        qh->Pointer()->qh.HorizLink = head->Pointer()->qh.HorizLink;
        qh->Pointer()->qh.DeviceAddress = functionAddr;
        qh->Pointer()->qh.InactivateOnNext = false;
        qh->Pointer()->qh.EndpointNumber = endpointNum;
        switch (speed) {
            case LOW:
                qh->Pointer()->qh.EndpointSpeed = EHCI_QH_ENDPOINT_SPEED_LOW;
                break;
            case FULL:
                qh->Pointer()->qh.EndpointSpeed = EHCI_QH_ENDPOINT_SPEED_FULL;
                break;
            default:
                qh->Pointer()->qh.EndpointSpeed = EHCI_QH_ENDPOINT_SPEED_HIGH;
        }
        qh->Pointer()->qh.DataToggleControl = 1;
        qh->Pointer()->qh.HeadOfReclamationList = false;
        qh->Pointer()->qh.MaxPacketLength = maxPacketSize;
        if (speed != HIGH && endpointType == usb_endpoint_type::CONTROL) {
            qh->Pointer()->qh.ControlEndpointFlag = true;
        } else {
            qh->Pointer()->qh.ControlEndpointFlag = false;
        }
        qh->Pointer()->qh.NakCountReload = 0;
        if (endpointType == usb_endpoint_type::INTERRUPT) {
            if (speed != LOW && speed != FULL) {
                qh->Pointer()->qh.InterruptScheduleMask = 1 << (ehciRef.intcycle & 7);
                qh->Pointer()->qh.SplitCompletionMask = 0;
            } else {
                qh->Pointer()->qh.InterruptScheduleMask = 1;
                qh->Pointer()->qh.SplitCompletionMask = 0x1C;
            }
        } else {
            qh->Pointer()->qh.InterruptScheduleMask = 0;
            qh->Pointer()->qh.SplitCompletionMask = 0;
        }
        qh->Pointer()->qh.HubAddress = hubAddress;
        qh->Pointer()->qh.PortNumber = portRouting[portRouting.size() - 1];
        qh->Pointer()->qh.HighBandwidthPipeMultiplier = 1;
        qh->Pointer()->qh.CurrentQTd = EHCI_POINTER_TERMINATE;
        qh->Pointer()->qh.NextQTd = EHCI_POINTER_TERMINATE;
        qh->Pointer()->qh.AlternateQTd = EHCI_POINTER_TERMINATE;
        qh->Pointer()->qh.Overlay[0] = 0;
        qh->Pointer()->qh.Overlay[1] = 0;
        qh->Pointer()->qh.Overlay[2] = 0;
        qh->Pointer()->qh.Overlay[3] = 0;
        qh->Pointer()->qh.Overlay[4] = 0;
        qh->Pointer()->qh.Overlay[5] = 0;
        qh->Pointer()->qh.NextEndpoint = head->Pointer()->qh.NextEndpoint;
        head->Pointer()->qh.HorizLink = qh->Phys() | EHCI_POINTER_QH;
        head->Pointer()->qh.NextEndpoint = this;
    }
}

ehci_endpoint::~ehci_endpoint() {
    if (head) {
        critical_section cli{};
        std::lock_guard lock{ehciRef.ehcilock};
        {
            auto iterator = ehciRef.watchList.begin();
            while (iterator != ehciRef.watchList.end()) {
                if (*iterator == this) {
                    ehciRef.watchList.erase(iterator);
                    break;
                }
                ++iterator;
            }
        }
        auto *qh = &(head->Pointer()->qh);
        bool found{false};
        while (qh != nullptr) {
            auto *endpoint = qh->NextEndpoint;
            if (this == endpoint) {
                auto *nqh = &(endpoint->qh->Pointer()->qh);
                qh->HorizLink = nqh->HorizLink;
                qh->NextEndpoint = nqh->NextEndpoint;
                found = true;
                break;
            }
            qh = endpoint != nullptr ? &(endpoint->qh->Pointer()->qh) : nullptr;
        }
        if (!found) {
            wild_panic("Endpoint destruction but did not find the endpoint");
        }
        std::shared_ptr<ehci_endpoint_cleanup> cleanup = std::make_shared<ehci_endpoint_cleanup>();
        cleanup->qh = this->qh;
        cleanup->transfers = this->active;
        ehciRef.delayedDestruction.push_back(cleanup);
        ehciRef.regs->usbCommand |= EHCI_CMD_ASYNC_DOORBELL;
    }
}

std::shared_ptr<usb_transfer>
ehci_endpoint::CreateTransferWithLock(bool commitTransaction, std::shared_ptr<usb_buffer> buffer, uint32_t size,
                                         usb_transfer_direction direction, int8_t dataToggle,
                                         std::function<void (ehci_transfer &transfer)> &applyFunc) {
    auto buffer_phys{0};
    if (buffer) {
        buffer_phys = buffer->Addr();
    } else if (size > 0) {
        return {};
    }
    if (buffer_phys > 0xFFFFFFFF || (buffer_phys & 4095) != 0) {
        return {};
    }
    if (size > 4096) {
        return {};
    }
    std::shared_ptr<ehci_transfer> transfer = std::make_shared<ehci_transfer>(ehciRef);
    transfer->buffer = buffer;
    auto &td = transfer->qTD();
    td.next_qtd = EHCI_POINTER_TERMINATE;
    td.data_toggle = (dataToggle & 1) != 0;
    td.total_bytes = size;
    td.interrup_on_cmp = commitTransaction;
    td.current_page = 0;
    td.error_count = 0;
    switch (direction) {
        case usb_transfer_direction::OUT:
            td.pid = 0;
            break;
        case usb_transfer_direction::IN:
            td.pid = 1;
            break;
        case usb_transfer_direction::SETUP:
            td.pid = 2;
            break;
    }
    td.status = EHCI_QTD_STATUS_ACTIVE;
    td.buffer_pages[0] = buffer_phys;

    applyFunc(*transfer);

    {
        std::shared_ptr<ehci_transfer> pending{this->pending};
        if (pending) {
            while (pending->next) {
                pending = pending->next;
            }
            pending->qTD().next_qtd = transfer->td->Phys();
            pending->next = transfer;
        } else {
            this->pending = transfer;
        }
    }

    if (commitTransaction) {
        IntWithLock();
        bool found{false};
        for (ehci_endpoint *watched : ehciRef.watchList) {
            if (watched == this) {
                found = true;
                break;
            }
        }
        if (!found) {
            ehciRef.watchList.push_back(this);
        }
    }

    std::shared_ptr<usb_transfer> usbTransfer{transfer};
    return usbTransfer;
}

std::shared_ptr<usb_transfer>
ehci_endpoint::CreateTransferWithoutLock(bool commitTransaction, std::shared_ptr<usb_buffer> buffer, uint32_t size,
                                         usb_transfer_direction direction, int8_t dataToggle,
                                         std::function<void (ehci_transfer &transfer)> &applyFunc) {
    critical_section cli{};
    std::lock_guard lock{ehciRef.HcdSpinlock()};

    return CreateTransferWithLock(commitTransaction, buffer, size, direction, dataToggle, applyFunc);
}

std::shared_ptr<usb_transfer>
ehci_endpoint::CreateTransfer(bool commitTransaction, void *data, uint32_t size, usb_transfer_direction direction,
                              bool bufferRounding, uint16_t delayInterrupt, int8_t dataToggle) {
    std::shared_ptr<usb_buffer> buffer = Alloc();
    memcpy(buffer->Pointer(), data, size);
    std::function<void (ehci_transfer &)> applyFunc = [] (ehci_transfer &) {};
    return CreateTransferWithoutLock(commitTransaction, buffer, size, direction, dataToggle, applyFunc);
}

std::shared_ptr<usb_transfer>
ehci_endpoint::CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction,
                              bool bufferRounding, uint16_t delayInterrupt, int8_t dataToggle) {
    std::shared_ptr<usb_buffer> buffer{};
    if (size > 0) {
        buffer = Alloc();
    }
    std::function<void (ehci_transfer &)> applyFunc = [] (ehci_transfer &) {};
    return CreateTransferWithoutLock(commitTransaction, buffer, size, direction, dataToggle, applyFunc);
}

std::shared_ptr<usb_transfer>
ehci_endpoint::CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction,
                              std::function<void()> doneCall, bool bufferRounding, uint16_t delayInterrupt,
                              int8_t dataToggle) {
    std::shared_ptr<usb_buffer> buffer{};
    if (size > 0) {
        buffer = Alloc();
    }
    std::function<void (ehci_transfer &)> applyFunc = [doneCall] (ehci_transfer &transfer) {
        transfer.SetDoneCall(doneCall);
    };
    return CreateTransferWithoutLock(commitTransaction, buffer, size, direction, dataToggle, applyFunc);
}

std::shared_ptr<usb_transfer>
ehci_endpoint::CreateTransferWithLock(bool commitTransaction, uint32_t size, usb_transfer_direction direction,
                                      std::function<void()> doneCall, bool bufferRounding, uint16_t delayInterrupt,
                                      int8_t dataToggle) {
    std::shared_ptr<usb_buffer> buffer{};
    if (size > 0) {
        buffer = Alloc();
    }
    std::function<void (ehci_transfer &)> applyFunc = [doneCall] (ehci_transfer &transfer) {
        transfer.SetDoneCall(doneCall);
    };
    return CreateTransferWithLock(commitTransaction, buffer, size, direction, dataToggle, applyFunc);
}

std::shared_ptr<usb_buffer> ehci_endpoint::Alloc() {
    std::shared_ptr<usb_buffer> buffer{new UsbBuffer32(4096)};
    return buffer;
}

bool ehci_endpoint::Addressing64bit() {
    return false;
}

void ehci_endpoint::IntWithLock() {
    auto &qhv = qh->Pointer()->qh;
    std::vector<std::shared_ptr<ehci_transfer>> done{};
    uint8_t status{0};
    while (active) {
        auto a_phys = active->td->Phys();
        if (a_phys == (qhv.NextQTd & 0xFFFFFFE0) || (a_phys == (qhv.CurrentQTd & 0xFFFFFFE0) && (qhv.Status & EHCI_QTD_STATUS_ACTIVE) != 0)) {
            break;
        }
        status = active->td->Pointer()->qtd.status;
        done.push_back(active);
        active = active->next;
    }
    if ((status & EHCI_QTD_STATUS_HALTED) != 0 && endpointType == usb_endpoint_type::CONTROL) {
        {
            std::stringstream str{};
            str << "qTD with error status " << status << "\n";
            get_klogger() << str.str().c_str();
        }
        while (active) {
            active->qTD().status = status;
            done.push_back(active);
            active = active->next;
        }
    }
    if (!active) {
        if (this->pending) {
            active = this->pending;
            this->pending = std::shared_ptr<ehci_transfer>();
            if (endpointType == usb_endpoint_type::CONTROL && (qhv.Status & EHCI_QTD_STATUS_HALTED) != 0) {
                qhv.NextQTd = active->td->Phys();
                qhv.Status = qhv.Status & ~(EHCI_QTD_STATUS_ACTIVE | EHCI_QTD_STATUS_HALTED);
            } else {
                qhv.NextQTd = active->td->Phys();
            }
        } else {
            auto iter = ehciRef.watchList.begin();
            while (iter != ehciRef.watchList.end()) {
                ehci_endpoint *endpoint = *iter;
                if (endpoint == this) {
                    ehciRef.watchList.erase(iter);
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

bool ehci_endpoint::ClearStall() {
    return false;
}
