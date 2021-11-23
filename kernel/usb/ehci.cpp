//
// Created by sigsegv on 28.05.2021.
//

#include <sstream>
#include <klogger.h>
#include <cpuio.h>
#include <thread>
#include "ehci.h"

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
#define EHCI_CMD_ASYNC_ENABLE   (1 << 5)
#define EHCI_CMD_PERIODIC_ENABLE    (1 << 4)


#define EHCI_STATUS_HALTED  (1 << 12)

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
            uint64_t physaddr{addr & 0xFFFFF000};
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
    qh->Pointer()->qh.HorizLink = EHCI_POINTER_TERMINATE;
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
    regs->usbCommand |= EHCI_CMD_ASYNC_ENABLE | EHCI_CMD_RUN_STOP;

    numPorts = (caps->hcsparams & 0xF);
    portPower = (caps->hcsparams & EHCI_HCSPARAMS_PORT_POWER_CONTROL) != 0;

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
    return LOW;
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
ehci::CreateControlEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum,
                            usb_endpoint_direction dir, usb_speed speed) {
    return std::shared_ptr<usb_endpoint>();
}

std::shared_ptr<usb_endpoint>
ehci::CreateInterruptEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum,
                              usb_endpoint_direction dir, usb_speed speed, int pollingIntervalMs) {
    return std::shared_ptr<usb_endpoint>();
}

bool ehci::irq() {
    get_klogger() << "EHCI intr\n";
    return false;
}
