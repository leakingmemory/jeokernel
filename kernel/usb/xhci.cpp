//
// Created by sigsegv on 28.05.2021.
//

#include <sstream>
#include <klogger.h>
#include <chrono>
#include <thread>
#include "xhci.h"
#include "xhci_resources_32.h"

#define XHCI_CMD_RUN        1
#define XHCI_CMD_HCRESET    (1 << 1)
#define XHCI_CMD_INT_ENABLE (1 << 2)

#define XHCI_STATUS_HALTED      1
#define XHCI_STATUS_NOT_READY   (1 << 11)

#define XHCI_PORT_SC_CCS                    1
#define XHCI_PORT_SC_ENABLED                (1 << 1)
#define XHCI_PORT_SC_OVERCURRENT            (1 << 3)
#define XHCI_PORT_SC_RESET                  (1 << 4)
#define XHCI_PORT_SC_PORT_LINK_STATE_MASK   (0xF << 5)
#define XHCI_PORT_SC_PORT_POWER             (1 << 9)
#define XHCI_PORT_SC_PORT_SPEED_MASK        (0xF << 10)
#define XHCI_PORT_SC_PORT_INDICATOR_MASK    (3 << 14)
#define XHCI_PORT_SC_PORT_LINK_STATE_WRITE  (1 << 16)
#define XHCI_PORT_SC_CSC                    (1 << 17)
#define XHCI_PORT_SC_ENABLED_CHANGE         (1 << 18)
#define XHCI_PORT_SC_WARM_PORT_RESET_CHANGE (1 << 19)
#define XHCI_PORT_SC_OVERCURRENT_CHANGE     (1 << 20)
#define XHCI_PORT_SC_PORT_RESET_CHANGE      (1 << 21)
#define XHCI_PORT_SC_PORT_LINK_STATE_CHANGE (1 << 22)
#define XHCI_PORT_SC_PORT_CONFIG_ERROR_CHG  (1 << 23)
#define XHCI_PORT_SC_COLD_ATTACH_STATUS     (1 << 24)
#define XHCI_PORT_SC_WAKE_ON_CONNECT_ENABLE (1 << 25)
#define XHCI_PORT_SC_WAKE_ON_DISCONN_ENABLE (1 << 26)
#define XHCI_PORT_SC_WAKE_ON_OVERCUR_ENABLE (1 << 27)
#define XHCI_PORT_SC_NOT_REMOVABLE_DEVICE   (1 << 30)
#define XHCI_PORT_SC_WARM_PORT_RESET        (1 << 31)

#define XHCI_PORT_SC_W1CLEAR_MASK   (XHCI_PORT_SC_ENABLED | XHCI_PORT_SC_CSC | XHCI_PORT_SC_ENABLED_CHANGE | XHCI_PORT_SC_WARM_PORT_RESET_CHANGE | XHCI_PORT_SC_OVERCURRENT_CHANGE | XHCI_PORT_SC_PORT_RESET_CHANGE | XHCI_PORT_SC_PORT_LINK_STATE_CHANGE | XHCI_PORT_SC_PORT_CONFIG_ERROR_CHG)

#define XHCI_PORT_SC_PLS_STATE_U0               0
#define XHCI_PORT_SC_PLS_STATE_U1               (1 << 5)
#define XHCI_PORT_SC_PLS_STATE_U2               (2 << 5)
#define XHCI_PORT_SC_PLS_STATE_U3               (3 << 5)
#define XHCI_PORT_SC_PLS_STATE_DISABLED         (4 << 5)
#define XHCI_PORT_SC_PLS_STATE_RX_DETECT        (5 << 5)
#define XHCI_PORT_SC_PLS_STATE_INACTIVE         (6 << 5)
#define XHCI_PORT_SC_PLS_STATE_POLLING          (7 << 5)
#define XHCI_PORT_SC_PLS_STATE_RECOVERY         (8 << 5)
#define XHCI_PORT_SC_PLS_STATE_HOT_RESET        (9 << 5)
#define XHCI_PORT_SC_PLS_STATE_COMPLIANCE_MODE  (10 << 5)
#define XHCI_PORT_SC_PLS_STATE_TEST_MODE        (11 << 5)
#define XHCI_PORT_SC_PLS_STATE_RESUME           (15 << 5)

Device *xhci_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    if (
            deviceInformation.device_class == 0x0C /* serial bus */ &&
            deviceInformation.device_subclass == 3 /* USB */ &&
            deviceInformation.prog_if == 0x30 /* xhci */
    ) {
        return new xhci(bus, *(deviceInformation.GetPciInformation()));
    } else {
        return nullptr;
    }
}

#define DEBUG_XHCI_INIT_OWNER

void xhci::init() {
    uint64_t addr{0};
    uint64_t size{0};
    bool prefetch{false};
    {
        PciBaseAddressRegister bar0 = pciDeviceInformation.readBaseAddressRegister(0);
        if (!bar0.is_memory()) {
            get_klogger() << "USB xhci controller is invalid (invalid BAR0, not memory)\n";
            return;
        }
        if (bar0.is_64bit()) {
            addr = bar0.addr64();
        } else if (bar0.is_32bit()) {
            addr = (uint64_t) bar0.addr32();
        } else {
            get_klogger() << "USB xhci controller is invalid (invalid BAR0, invalid record)\n";
            return;
        }
        size = bar0.memory_size();
        if (size == 0) {
            get_klogger() << "USB xhci controller is invalid (invalid BAR0, invalid size)\n";
            return;
        }
        prefetch = bar0.is_prefetchable();
    }
#ifdef DEBUG_XHCI_INIT_OWNER
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
    capabilities = (xhci_capabilities *) (void *) (((uint8_t *) mapped_registers_vm->pointer()) + pg_offset);
    opregs = capabilities->opregs();
    runtimeregs = capabilities->runtimeregs();
    doorbellregs = capabilities->doorbellregs();
    {
        std::optional<xhci_ext_cap> extcap = capabilities->extcap();
        while (extcap)  {
            if (extcap->cap_id == 1) {
                auto *legsup = extcap->legsup();
                if (!legsup->is_owned()) {
                    {
                        std::stringstream msg;
                        msg << DeviceType() << (unsigned int) DeviceId() << ": disabling legacy support\n";
                        get_klogger() << msg.str().c_str();
                    }
                    legsup->assert_os_own();
                    while (!legsup->is_owned()) {
                        using namespace std::literals::chrono_literals;
                        std::this_thread::sleep_for(20ms);
                    }
                }
            } else {
                std::stringstream msg;
                msg << DeviceType() << (unsigned int) DeviceId() << ": ext cap type " << (unsigned int) extcap->cap_id << " value " << std::hex << extcap->value << "\n";
                get_klogger() << msg.str().c_str();
            }
            extcap = extcap->next();
        }
    }

    /* Stop HC */
    opregs->usbCommand &= ~XHCI_CMD_RUN;
    {
        int timeout = 5;
        while (true) {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(20ms);
            if ((opregs->usbStatus & XHCI_STATUS_HALTED) != 0) {
                break;
            }
            if (--timeout == 0) {
                std::stringstream msg;
                msg << DeviceType() << (unsigned int) DeviceId() << ": USB3 failed to stop, no use - cmd=" << std::hex << opregs->usbCommand << " status="<< opregs->usbStatus << "\n";
                get_klogger() << msg.str().c_str();
            }
        }
    }

    /* Reset HC */
    opregs->usbCommand |= XHCI_CMD_HCRESET;
    {
        int timeout = 20;
        while (true) {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10ms);
            if ((opregs->usbCommand & XHCI_CMD_HCRESET) == 0) {
                break;
            }
            if (--timeout == 0) {
                std::stringstream msg;
                msg << DeviceType() << (unsigned int) DeviceId() << ": USB3 failed to reset, no use\n";
                get_klogger() << msg.str().c_str();
            }
        }
    }
    {
        int timeout = 20;
        while (true) {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10ms);
            if ((opregs->usbStatus & XHCI_STATUS_NOT_READY) == 0) {
                break;
            }
            if (--timeout == 0) {
                std::stringstream msg;
                msg << DeviceType() << (unsigned int) DeviceId() << ": USB3 failed to become available after reset, no use\n";
                get_klogger() << msg.str().c_str();
            }
        }
    }

    {
        uint32_t hcsparams1{capabilities->hcsparams1};
        numSlots = (uint8_t) (hcsparams1 & 0xFF);
        {
            uint32_t config{opregs->config};
            config &= 0xFFFFFF00;
            config |= numSlots;
            opregs->config = config;
        }
        numInterrupters = (uint16_t) ((hcsparams1 >> 8) & 0x7FF);
        numPorts = (uint8_t) (hcsparams1 >> 24);
    }
    {
        uint32_t hcsparams2{capabilities->hcsparams2};
        eventRingSegmentTableMax = (1 << ((hcsparams2 >> 4) & 0xF));
    }

    uint32_t maxScratchpadBuffers{capabilities->hcsparams2};
    {
        uint32_t hi{maxScratchpadBuffers};
        maxScratchpadBuffers = maxScratchpadBuffers >> 27;
        hi = (hi >> (21 - 5)) & (0x1F << 5);
        maxScratchpadBuffers |= hi;
    }

    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": hcspa2=" << std::hex << capabilities->hcsparams2
            << " max-scratchpad=" << maxScratchpadBuffers << std::dec << " ev-seg=" << eventRingSegmentTableMax
            << " slots=" << numSlots << " ports=" << numPorts << " inter=" << numInterrupters << "\n";
        get_klogger() << msg.str().c_str();
    }

    resources = std::unique_ptr<xhci_resources>(new xhci_resources_32(maxScratchpadBuffers, Pagesize()));
    {
        auto dcbaa = resources->DCBAA();
        dcbaa->phys[0] = resources->ScratchpadPhys();
        for (int i = 0; i < numSlots; i++) {
            dcbaa->contexts[i] = 0;
            dcbaa->phys[i + 1] = 0;
        }
    }
    opregs->deviceContextBaseAddressArrayPointer = resources->DCBAAPhys();
    opregs->commandRingControl = resources->CommandRingPhys() | XHCI_TRB_CYCLE;

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

    for (int i = 1; i < numInterrupters; i++) {
        auto &inter = runtimeregs->interrupters[i];
        inter.interrupterManagement = 0;
        inter.eventRingSegmentTableSize = 0;
    }
    {
        auto &inter = runtimeregs->interrupters[0];
        inter.interrupterManagement = XHCI_INTERRUPT_MANAGEMENT_INTERRUPT_ENABLE;
        uint16_t segments = 1;
        if (segments > eventRingSegmentTableMax) {
            segments = eventRingSegmentTableMax;
        }
        inter.eventRingDequeuePointer = resources->PrimaryFirstEventPhys() |
                XHCI_INTERRUPTER_DEQ_HANDLER_BUSY;
        inter.eventRingSegmentTableBaseAddress = resources->PrimaryEventSegmentsPhys();
        inter.eventRingSegmentTableSize = segments;
    }

    opregs->usbCommand |= XHCI_CMD_INT_ENABLE | XHCI_CMD_RUN;

    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": USB3 controller hccparams1=" << std::hex << capabilities->hccparams1 << "\n";
        get_klogger() << msg.str().c_str();
    }

    Run();
}

uint32_t xhci::Pagesize() {
    uint32_t n{opregs->pageSize & 0xFFFF};
    uint32_t pagesize(n * 4096);
    return pagesize;
}

bool xhci::irq() {
    get_klogger() << "XHCI intr\n";
    return false;
}

void xhci::dumpregs() {
}

int xhci::GetNumberOfPorts() {
    return numPorts;
}

uint32_t xhci::GetPortStatus(int port) {
    uint32_t status{0};
    uint32_t hcstatus{opregs->portRegisters[port].portStatusControl};
    if ((hcstatus & XHCI_PORT_SC_CCS) != 0) {
        status |= USB_PORT_STATUS_CCS;
    }
    if ((hcstatus & XHCI_PORT_SC_ENABLED) != 0) {
        status |= USB_PORT_STATUS_PES;
    }
    if ((hcstatus & XHCI_PORT_SC_OVERCURRENT) != 0) {
        status |= USB_PORT_STATUS_POCI;
    }
    if ((hcstatus & XHCI_PORT_SC_RESET) != 0) {
        status |= USB_PORT_STATUS_PRS;
    }
    if ((hcstatus & XHCI_PORT_SC_PORT_POWER) != 0) {
        status |= USB_PORT_STATUS_PPS;
    }
    if ((hcstatus & XHCI_PORT_SC_CSC) != 0) {
        status |= USB_PORT_STATUS_CSC;
    }
    if ((hcstatus & XHCI_PORT_SC_ENABLED_CHANGE) != 0) {
        status |= USB_PORT_STATUS_PESC;
    }
    if ((hcstatus & XHCI_PORT_SC_OVERCURRENT_CHANGE) != 0) {
        status |= USB_PORT_STATUS_OCIC;
    }
    if ((hcstatus & XHCI_PORT_SC_PORT_RESET_CHANGE) != 0) {
        status |= USB_PORT_STATUS_PRSC;
    }
    return status;
}

void xhci::SwitchPortOff(int port) {
}

void xhci::SwitchPortOn(int port) {
}

void xhci::EnablePort(int port) {
    /* Not possible on xhci */
}

void xhci::DisablePort(int port) {
}

void xhci::ResetPort(int port) {
    {
        uint32_t change{GetPortStatus(port) & USB_PORT_STATUS_PRSC};
        if (change != 0) {
            ClearStatusChange(port, change);
        }
    }
    opregs->portRegisters[port].portStatusControl |= XHCI_PORT_SC_RESET;
    int timeout = 10;
    while (--timeout > 0) {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(20ms);
        uint32_t status{GetPortStatus(port)};
        if ((status & USB_PORT_STATUS_PRSC) != 0 && (status & USB_PORT_STATUS_PRS) == 0) {
            ClearStatusChange(port, USB_PORT_STATUS_PRSC);
            return;
        }
    }
}

usb_speed xhci::PortSpeed(int port) {
    return LOW;
}

void xhci::ClearStatusChange(int port, uint32_t statuses) {
    uint32_t clear{0};
    if ((statuses & USB_PORT_STATUS_CSC) != 0) {
        clear |= XHCI_PORT_SC_CSC;
    }
    if ((statuses & USB_PORT_STATUS_PESC) != 0) {
        clear |= XHCI_PORT_SC_ENABLED_CHANGE;
    }
    if ((statuses & USB_PORT_STATUS_OCIC) != 0) {
        clear |= XHCI_PORT_SC_OVERCURRENT_CHANGE;
    }
    if ((statuses & USB_PORT_STATUS_PRSC) != 0) {
        clear |= XHCI_PORT_SC_PORT_RESET_CHANGE;
    }
    uint32_t status{opregs->portRegisters[port].portStatusControl};
    status &= ~XHCI_PORT_SC_W1CLEAR_MASK;
    status |= clear;
    opregs->portRegisters[port].portStatusControl = status;
}

std::shared_ptr<usb_endpoint>
xhci::CreateControlEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum,
                            usb_endpoint_direction dir, usb_speed speed) {
    return {};
}

std::shared_ptr<usb_endpoint>
xhci::CreateInterruptEndpoint(uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum,
                              usb_endpoint_direction dir, usb_speed speed, int pollingIntervalMs) {
    return {};
}

std::shared_ptr<usb_hw_enumeration> xhci::EnumeratePort(int port) {
    return std::shared_ptr<usb_hw_enumeration>(new xhci_port_enumeration(*this, port));
}

std::shared_ptr<usb_hw_enumeration_addressing> xhci_port_enumeration::enumerate() {
    uint32_t status{xhciRef.GetPortStatus(port)};
    {
        int timeout = 5;
        while ((status & USB_PORT_STATUS_PES) == 0) {
            if (--timeout == 0) {
                break;
            }
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10ms);
            status = xhciRef.GetPortStatus(port);
        }
    }
    if ((status & USB_PORT_STATUS_PES) != 0) {
        /* USB3 */
        return std::shared_ptr<usb_hw_enumeration_addressing>(
                new xhci_port_enumeration_addressing(xhciRef, port)
                );
    } else if ((xhciRef.opregs->portRegisters[port].portStatusControl & XHCI_PORT_SC_PORT_LINK_STATE_MASK) == XHCI_PORT_SC_PLS_STATE_POLLING){
        /* USB2 */
        xhciRef.ResetPort(port);
        uint32_t portstate{xhciRef.opregs->portRegisters[port].portStatusControl};
        uint32_t pls{portstate & XHCI_PORT_SC_PORT_LINK_STATE_MASK};
        if (
                (portstate & XHCI_PORT_SC_ENABLED) != 0 &&
                pls == XHCI_PORT_SC_PLS_STATE_U0
                ) {
            return std::shared_ptr<usb_hw_enumeration_addressing>(
                    new xhci_port_enumeration_addressing(xhciRef, port)
            );
        }
    }
    return {};
}

std::shared_ptr<usb_hw_enumeration_ready> xhci_port_enumeration_addressing::set_address(uint8_t addr) {
    return {};
}
