//
// Created by sigsegv on 28.05.2021.
//

#include <sstream>
#include <klogger.h>
#include <chrono>
#include <thread>
#include "xhci.h"
#include "xhci_resources_32.h"
#include "xhci_endpoint.h"
#include "xhci_input_context_container_32.h"

#define XHCI_CMD_RUN        1
#define XHCI_CMD_HCRESET    (1 << 1)
#define XHCI_CMD_INT_ENABLE (1 << 2)

#define XHCI_STATUS_HALTED          1
#define XHCI_STATUS_EVENT_INTERRUPT (1 << 3)
#define XHCI_STATUS_NOT_READY       (1 << 11)

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

#define XHCI_EVENT_TRANSFER             32
#define XHCI_EVENT_COMMAND_COMPLETION   33
#define XHCI_EVENT_PORT_STATUS_CHANGE   34
#define XHCI_EVENT_BANDWIDTH_REQUEST    35
#define XHCI_EVENT_DOORBELL             36
#define XHCI_EVENT_HOST_CONTROLLER      37
#define XHCI_EVENT_DEVICE_NOTIFICATION  38
#define XHCI_EVENT_MFINDEX_WRAP         39

#define XHCI_CONFIG_CIE     (1 << 9)

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
//#define DEBUG_XHCI_DUMP_ENDPOINT
//#define DEBUG_XHCI_COMMAND_COMPLETION

void xhci::init() {
    uint64_t addr{0};
    uint64_t size{0};
    bool prefetch{false};
    {
        uint32_t statusCmd = read_pci_config(pciDeviceInformation.bus, pciDeviceInformation.slot,
                                             pciDeviceInformation.func, 4);
        uint32_t setStatusCmd = (statusCmd & ~((uint32_t) 1 << 10)) | 6 /* MA&BM */;
        if (setStatusCmd != statusCmd) {
            get_klogger() << "Config cmd pci/ohci " << statusCmd << "->" << setStatusCmd << "\n";
            write_pci_config(pciDeviceInformation.bus, pciDeviceInformation.slot,
                             pciDeviceInformation.func, 4, setStatusCmd);
        } else {
            get_klogger() << "Config cmd pci/ohci ok\n";
        }
    }
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
        uint64_t physaddr{addr & 0xFFFFFFFFFFFFF000};
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
            } else if (extcap->cap_id == 2) {
                auto *sup_proto = extcap->supported_protocol();
                supported_protocol_list.push_back(sup_proto);
            } else {
                std::stringstream msg;
                msg << DeviceType() << (unsigned int) DeviceId() << ": ext cap type " << (unsigned int) extcap->cap_id << " value " << std::hex << extcap->value << "\n";
                get_klogger() << msg.str().c_str();
            }
            extcap = extcap->next();
        }
    }
    for (auto *sup_proto : supported_protocol_list) {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": supported protocols for ports " << sup_proto->compat_port_num << "-" << (((uint16_t) sup_proto->compat_port_num) + ((uint16_t) sup_proto->compat_port_count)) << ":"
            << " ver " << sup_proto->rev_maj << "." << sup_proto->rev_min << " slottyp " << sup_proto->protocol_slot_type << " speeds " << sup_proto->protocol_speed_id_count << "\n";
        if (sup_proto->protocol_speed_id_count != 0) {
            msg << DeviceType() << (unsigned int) DeviceId() << ": speeds:" << std::hex;
            for (int i = 0; i < sup_proto->protocol_speed_id_count; i++) {
                msg << " " << sup_proto->speeds[i];
            }
            msg << "\n";
        }
        get_klogger() << msg.str().c_str();
    }
    controller64bit = capabilities->controllerIs64bit();
    contextSize64 = capabilities->contextSizeIs64();

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
        xhci_hccparams2 hccparams2{capabilities->hccparams2};
        uint32_t hcsparams1{capabilities->hcsparams1};
        numSlots = (uint8_t) (hcsparams1 & 0xFF);
        {
            uint32_t config{opregs->config};
            config &= 0xFFFFFF00;
            config |= numSlots;
            if (hccparams2.cic) {
                config |= XHCI_CONFIG_CIE;
            }
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
        uint32_t exitLatency{capabilities->hcsparams3};
        u1Exit = exitLatency & 0x0FF;
        exitLatency = exitLatency >> 16;
        u2Exit = exitLatency;
    }

    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": hcspa2=" << std::hex << capabilities->hcsparams2
            << " hcc2=" << capabilities->hccparams2
            << " max-scratchpad=" << maxScratchpadBuffers << std::dec << " ev-seg=" << eventRingSegmentTableMax
            << " slots=" << numSlots << " ports=" << numPorts << " inter=" << numInterrupters
            << " crng=" << opregs->commandRingControl << " dboff=" << capabilities->dboff
            << " 64bit=" << (controller64bit ? "yes" : "no") << " u1/u2exit=" << u1Exit << "/" << u2Exit << "\n";
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
        uint16_t segments = 1;
        if (segments > eventRingSegmentTableMax) {
            segments = eventRingSegmentTableMax;
        }
        inter.eventRingDequeuePointer = resources->PrimaryFirstEventPhys() |
                XHCI_INTERRUPTER_DEQ_HANDLER_BUSY;
        inter.eventRingSegmentTableSize = segments;
        inter.eventRingSegmentTableBaseAddress = resources->PrimaryEventSegmentsPhys();

        inter.interrupterModeration = 4000;
        inter.interrupterManagement = XHCI_INTERRUPT_MANAGEMENT_INTERRUPT_ENABLE | XHCI_INTERRUPT_MANAGEMENT_INTERRUPT_PENDING;
    }

    opregs->usbCommand |= XHCI_CMD_INT_ENABLE | XHCI_CMD_RUN;

    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": USB3 controller hccparams1=" << std::hex << capabilities->hccparams1 << "\n";
        get_klogger() << msg.str().c_str();
    }

    event_thread = new std::thread(
            [this] () {
                {
                    std::stringstream str{};
                    str << "[" << DeviceType() << DeviceId() << "-event]";
                    std::this_thread::set_name(str.str());
                }
                while (true) {
                    event_sema.acquire();
                    {
                        std::lock_guard lock{xhcilock};
                        if (stop) {
                            break;
                        }
                    }
                    PrimaryEventRingAsync();
                }
            }
    );

    watchdog_thread = new std::thread(
            [this] () {
                {
                    std::stringstream str{};
                    str << "[" << DeviceType() << DeviceId() << "-watchdog]";
                    std::this_thread::set_name(str.str());
                }
                Watchdog();
            }
    );

    Run();
}

uint32_t xhci::Pagesize() {
    uint32_t n{opregs->pageSize & 0xFFFF};
    uint32_t pagesize(n * 4096);
    return pagesize;
}

bool xhci::irq() {
    bool handled{false};
    uint32_t status{opregs->usbStatus};
    while (true) {
        uint32_t clear{0};
        if ((status & XHCI_STATUS_EVENT_INTERRUPT) != 0) {
            clear |= XHCI_STATUS_EVENT_INTERRUPT;
            HcEvent();
        }
        if (clear == 0) {
            break;
        }
        opregs->usbStatus = clear;
        handled = true;
        status = opregs->usbStatus;
    }
    return handled;
}

void xhci::dumpregs() {
    std::stringstream str{};
    str << "XHCI cmd=" << std::hex << opregs->usbCommand << " sts=" << opregs->usbStatus
    << " crng=" << opregs->commandRingControl << " i0=" << runtimeregs->interrupters[0].interrupterManagement
    << " im0=" << runtimeregs->interrupters[0].interrupterModeration
    << " evd=" << runtimeregs->interrupters[0].eventRingDequeuePointer << "\n";
    get_klogger() << str.str().c_str();
}

void xhci::Watchdog() {
    bool controllerRunning{false};
    while (true) {
        {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(1s);
        }
        bool checkIrq{false};
        {
            std::lock_guard lock{xhcilock};
            if (stop) {
                return;
            }
            if (irqWatchdog) {
                checkIrq = true;
            } else {
                irqWatchdog = true;
            }
        }
        if ((opregs->usbStatus & XHCI_STATUS_HALTED) == 0) {
            if (!controllerRunning) {
                std::stringstream str{};
                str << DeviceType() << DeviceId() << ": controller running\n";
                get_klogger() << str.str().c_str();
            }
            controllerRunning = true;
        } else {
            if (controllerRunning) {
                std::stringstream str{};
                str << DeviceType() << DeviceId() << ": controller halted\n";
                get_klogger() << str.str().c_str();
            }
            controllerRunning = false;
        }

        if (checkIrq) {
            if (runtimeregs->interrupters[0].eventRingDequeuePointer & XHCI_INTERRUPTER_DEQ_HANDLER_BUSY) {
                {
                    std::stringstream str{};
                    str << DeviceType() << DeviceId() << ": irq silence recovery for primary event ring\n";
                    get_klogger() << str.str().c_str();
                }

                {
                    uint32_t iman{runtimeregs->interrupters[0].interrupterManagement};
                    if ((iman & XHCI_INTERRUPT_MANAGEMENT_INTERRUPT_PENDING) != 0) {
                        runtimeregs->interrupters[0].interrupterManagement = iman;

                        std::stringstream str{};
                        str << DeviceType() << DeviceId() << ": irq silence recovery: cleared irq pending\n";
                        get_klogger() << str.str().c_str();
                    }
                }

                PrimaryEventRing();
            }
        }
    }
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
    uint32_t wcr = XHCI_PORT_SC_RESET;
    opregs->portRegisters[port].portStatusControl = (opregs->portRegisters[port].portStatusControl & ~wcr) | XHCI_PORT_SC_ENABLED;
    int timeout = 100;
    while (--timeout > 0) {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(20ms);
        uint32_t status{GetPortStatus(port)};
        if ((status & USB_PORT_STATUS_PES) == 0) {
            get_klogger() << "XHCI disable port success\n";
            return;
        }
    }
    get_klogger() << "XHCI disable port failed\n";
}

void xhci::ResetPort(int port) {
    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(200ms);
    }
    dumpregs();
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

xhci_supported_protocol_cap * xhci::SupportedProtocolsForPort(int port) {
    for (auto *sup : supported_protocol_list) {
        if (sup->compat_port_num > 0 && sup->compat_port_count > 0) {
            if ((port + 1) >= sup->compat_port_num) {
                int upper{sup->compat_port_num};
                upper += sup->compat_port_count;
                if ((port + 1) < upper) {
                    return sup;
                }
            }
        }
    }
    return nullptr;
}

static uint32_t usb_kbrate(uint32_t value) {
    uint32_t exp{(value >> 4) & 3};
    uint32_t mantissa{value >> 16};
    switch (exp) {
        case 0:
            return mantissa / 1000;
        case 1:
            return mantissa;
        case 2:
            return mantissa * 1000;
        case 3:
            return mantissa * 1000000;
        default:
            return mantissa;
    }
}

usb_speed xhci::PortSpeed(int port) {
    uint32_t speed{((opregs->portRegisters[port].portStatusControl >> 10) & 0xF)};
    xhci_supported_protocol_cap *sup_proto = SupportedProtocolsForPort(port);
    if (sup_proto != nullptr) {
        if (sup_proto->is_3()) {
            for (uint32_t i = 0; i < sup_proto->protocol_speed_id_count; i++) {
                uint32_t value{sup_proto->speeds[i]};
                uint32_t psiv{value & 0xF};
                if (psiv == speed) {
                    uint32_t proto{(value >> 14) & 3};
                    if (proto == 0) {
                        return SUPER;
                    } else if (proto == 1) {
                        return SUPERPLUS;
                    }
                }
            }
        } if (sup_proto->is_2_0()) {
            for (uint32_t i = 0; i < sup_proto->protocol_speed_id_count; i++) {
                uint32_t value{sup_proto->speeds[i]};
                uint32_t psiv{value & 0xF};
                if (psiv == speed) {
                    uint32_t kbrate{usb_kbrate(value)};
                    if (kbrate <= 1500) {
                        return LOW;
                    } else if (kbrate <= 12000) {
                        return FULL;
                    } else {
                        return HIGH;
                    }
                }
            }
        }
    }
    switch (speed) {
        case 1:
            return FULL;
        case 2:
            return LOW;
        case 3:
            return HIGH;
        case 4:
            return SUPER;
        case 5:
        case 6:
        case 7:
        default:
            return SUPERPLUS;
    }
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
xhci::CreateControlEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize,
                            uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) {
    return {};
}

std::shared_ptr<usb_endpoint>
xhci::CreateInterruptEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize,
                              uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed,
                              int pollingIntervalMs) {
    return {};
}

std::shared_ptr<usb_endpoint>
xhci::CreateBulkEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize,
                         uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) {
    return {};
}

std::shared_ptr<usb_buffer> xhci::Alloc(size_t size) {
    return resources->Alloc(size);
}

std::tuple<uint64_t,xhci_trb *> xhci::NextCommand() {
    xhci_trb *trb = &(resources->Rings()->CommandRing.ring[commandIndex]);
    if (commandBarrier == nullptr) {
        commandBarrier = trb;
    } else if (commandBarrier == trb) {
        return std::make_tuple<uint64_t,xhci_trb *>(0, nullptr);
    }
    uint64_t physaddr{resources->CommandRingPhys()};
    physaddr += sizeof(resources->Rings()->CommandRing.ring[0]) * commandIndex;
    if (commandIndex == 0) {
        commandCycle = commandCycle != 0 ? 0 : 1;
    }
    ++commandIndex;
    if (commandIndex == (resources->Rings()->CommandRing.LengthIncludingLink() - 1)) {
        uint16_t cmd{resources->Rings()->CommandRing.ring[commandIndex].Command};
        cmd &= 0xFFFE;
        cmd |= commandCycle;
        resources->Rings()->CommandRing.ring[commandIndex].Command = cmd;
        commandIndex = 0;
    }
    trb->Data.DataPtr = 0;
    trb->Data.TransferLength = 0;
    trb->Data.TDSize = 0;
    trb->Data.InterrupterTarget = 0;
    trb->Command = trb->Command & 1;
    trb->EnableSlot.SlotType = 0;
    return std::make_tuple<uint64_t,xhci_trb *>(physaddr, trb);
}

std::shared_ptr<usb_hw_enumeration> xhci::EnumeratePort(int port) {
    return std::shared_ptr<usb_hw_enumeration>(new xhci_port_enumeration(*this, port));
}

std::shared_ptr<usb_hw_enumeration_addressing> xhci::EnumerateHubPort(const std::vector<uint8_t> &portRouting, usb_speed speed, usb_speed hubSpeed, uint8_t hubSlot) {
    return std::shared_ptr<usb_hw_enumeration_addressing>(new xhci_hub_port_enumeration_addressing(*this, portRouting, speed, hubSlot, hubSpeed));
}

void xhci::HcEvent() {
    {
        std::lock_guard lock{xhcilock};
        irqWatchdog = false;
    }
    uint32_t iman{runtimeregs->interrupters[0].interrupterManagement};
    if ((iman & XHCI_INTERRUPT_MANAGEMENT_INTERRUPT_PENDING) != 0) {
        runtimeregs->interrupters[0].interrupterManagement = iman;
        PrimaryEventRing();
    }
}

xhci::~xhci() {
    {
        std::lock_guard lock{xhcilock};
        stop = true;
    }
    event_sema.release();
    if (event_thread != nullptr) {
        event_thread->join();
        delete event_thread;
        event_thread = nullptr;
    }
    if (watchdog_thread != nullptr) {
        watchdog_thread->join();
        delete watchdog_thread;
        watchdog_thread = nullptr;
    }
}

void xhci::PrimaryEventRing() {
    if (0 == (runtimeregs->interrupters[0].eventRingDequeuePointer & XHCI_INTERRUPTER_DEQ_HANDLER_BUSY)) {
        return;
    }
    uint32_t index{0};
    uint8_t cycle{0};
    std::lock_guard lock{xhcilock};
    cycle = primaryEventCycle;
    index = primaryEventIndex;
    bool hasEvents{false};
    while (true) {
        xhci_trb event = resources->Rings()->PrimaryEventRing.ring[index];
        if ((event.Command & XHCI_TRB_CYCLE) != cycle) {
            break;
        }
        events.push_back(event);
        hasEvents = true;
        ++index;
        if (index == resources->Rings()->PrimaryEventRing.Length()) {
            cycle = cycle != 0 ? 0 : 1;
            index = 0;
        }
    }
    primaryEventCycle = cycle;
    primaryEventIndex = index;

    if (hasEvents) {
        event_sema.release();
    }

    uint64_t dequeptr{resources->PrimaryFirstEventPhys()};
    dequeptr += sizeof(resources->Rings()->PrimaryEventRing.ring[0]) * index;
    runtimeregs->interrupters[0].eventRingDequeuePointer = dequeptr |
                                                           XHCI_INTERRUPTER_DEQ_HANDLER_BUSY;
}

void xhci::PrimaryEventRingAsync() {
    std::vector<xhci_trb> events{};

    {
        std::lock_guard lock{xhcilock};

        for (xhci_trb trb : this->events) {
            events.push_back(trb);
        }

        this->events.clear();
    }

    for (xhci_trb event : events) {
        uint16_t trbtype{event.Command};
        trbtype = trbtype >> 10;
        Event(trbtype, event);
    }
}

void xhci::Event(uint8_t trbtype, const xhci_trb &event) {
    switch (trbtype) {
        case XHCI_EVENT_TRANSFER:
            TransferEvent(event);
            break;
        case XHCI_EVENT_COMMAND_COMPLETION:
            CommandCompletion(event);
            break;
        case XHCI_EVENT_PORT_STATUS_CHANGE:
            RootHubStatusChange();
            break;
        case XHCI_EVENT_BANDWIDTH_REQUEST:
            get_klogger() << "XHCI bandwidth request event\n";
            break;
        case XHCI_EVENT_DOORBELL:
            get_klogger() << "XHCI doorbell event\n";
            break;
        case XHCI_EVENT_HOST_CONTROLLER:
            get_klogger() << "XHCI host controller event\n";
            break;
        case XHCI_EVENT_DEVICE_NOTIFICATION:
            get_klogger() << "XHCI device notification\n";
            break;
        case XHCI_EVENT_MFINDEX_WRAP:
            get_klogger() << "XHCI MFINDEX wrap\n";
            break;
    }
}

void xhci::CommandCompletion(const xhci_trb &event) {
    uint64_t phys = event.CommandCompletion.CommandPtr & 0xFFFFFFFFFFFFFFF0;
    uint8_t code = event.CommandCompletion.CompletionCode;
    uint8_t slotId = event.EnableSlot.SlotId;

#ifdef DEBUG_XHCI_COMMAND_COMPLETION
    {
        std::stringstream str{};
        str << "Command completion " << std::hex << phys << " code " << std::dec << code << " slot " << slotId << "\n";
        get_klogger() << str.str().c_str();
    }
#endif

    std::lock_guard lock{xhcilock};
    for (auto &cmd : commands) {
        if (cmd->phys == phys) {
            cmd->code = code;
            cmd->slotID = slotId;
            auto *trb = cmd->trb;
            cmd->SetDone();
            commandBarrier = trb;
            break;
        }
    }
    auto iter = commands.begin();
    while (iter != commands.end()) {
        std::shared_ptr<xhci_command> cmd = *iter;
        if (!cmd->done) {
            break;
        }
        commands.erase(iter);
    }
}

void xhci::TransferEvent(const xhci_trb &event) {
    std::lock_guard lock{xhcilock};
    auto *slotContext = resources->DCBAA()->contexts[event.TransferEvent.SlotId];
    if (slotContext != nullptr) {
        auto *eventContext = slotContext->Endpoint(event.TransferEvent.EndpointId - 1);
        if (eventContext != nullptr) {
            eventContext->TransferEvent(event.TransferCompletion.TransferPtr, event.TransferCompletion.TransferLength,
                                        event.TransferCompletion.CompletionCode);
        } else {
            std::stringstream str{};
            str << "XHCI transfer event without a recipient endp " << (event.TransferEvent.EndpointId - 1) << " addr " << std::hex << event.TransferCompletion.TransferPtr << " code " << event.TransferCompletion.CompletionCode << "\n";
            get_klogger() << str.str().c_str();
        }
    }
}

xhci_port_enumerated_device::~xhci_port_enumerated_device() {
    auto disableSlotCommand = xhciRef.DisableSlot(slot);
    if (!disableSlotCommand) {
        std::stringstream str{};
        str << "XHCI disable slot failed: Couldn't create command TRB on ring\n";
        get_klogger() << str.str().c_str();
        return;
    }
    {
        bool done{false};
        int timeout = 100;
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(20ms);
        while (--timeout > 0) {
            if (disableSlotCommand->IsDone()) {
                done = true;
                break;
            }
            std::this_thread::sleep_for(40ms);
        }
        if (!done) {
            std::stringstream str{};
            str << "XHCI disable slot failed with code "<< disableSlotCommand->CompletionCode() << "\n";
            get_klogger() << str.str().c_str();
            return;
        }
        std::stringstream str{};
        str << "XHCI disabled slot with ID " << slot << "\n";
        get_klogger() << str.str().c_str();
    }
}

void xhci_port_enumerated_device::Stop() {
    std::lock_guard lock{xhciRef.xhcilock};
    xhciRef.resources->DCBAA()->contexts[slot]->Stop();
}

usb_speed xhci_port_enumerated_device::Speed() const {
    return speed;
}

uint8_t xhci_port_enumerated_device::SlotId() const {
    return slot;
}

usb_minimum_device_descriptor xhci_port_enumerated_device::MinDesc() const {
    return minDesc;
}

std::shared_ptr<usb_endpoint> xhci_port_enumerated_device::Endpoint0() const {
    return endpoint0;
}

bool xhci_port_enumerated_device::SetHub(uint8_t numberOfPorts, bool multiTT, uint8_t ttThinkTime) {
    ttThinkTime -= 8;
    ttThinkTime = (ttThinkTime >> 3) & 3;
    auto *inputctx = inputctx_container->InputContext();
    {
        xhci_input_control_context *inputContext = &(inputctx->context[0]);
        xhci_slot_context *slotContext = inputctx->GetSlotContext(xhciRef.contextSize64);
        inputContext->addContextFlags = 1; // A0
        slotContext->flags = XHCI_SLOT_CONTEXT_FLAG_HUB;
        if (multiTT) {
            slotContext->flags |= XHCI_SLOT_CONTEXT_FLAG_MTT;
        }
        slotContext->numberOfPorts = numberOfPorts;
        slotContext->ttThinkTime = ttThinkTime;
        auto configCmd = xhciRef.ConfigureEndpoint(inputctx_container->InputContextPhys(), slot);
        if (!configCmd) {
            std::stringstream str{};
            str << "XHCI evaluate context (set hub) failed: Couldn't create command on ring\n";
            get_klogger() << str.str().c_str();
            return false;
        }
        bool done{false};
        int timeout = 50;
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(20ms);
        while (--timeout > 0) {
            if (configCmd->IsDone()) {
                done = true;
                break;
            }
            std::this_thread::sleep_for(40ms);
        }
        if (!done) {
            std::stringstream str{};
            str << "XHCI evaluate context (set hub) failed with code " << configCmd->CompletionCode() << "\n";
            get_klogger() << str.str().c_str();
            return false;
        }
    }
    return true;
}

bool xhci_port_enumerated_device::SetConfigurationValue(uint8_t configurationValue, uint8_t interfaceNumber, uint8_t alternateSetting) {
    auto *inputctx = &(inputctx_container->InputContext()->context[0]);
    inputctx->addContextFlags = 1;
    inputctx->dropContextFlags = 0;
    inputctx->alternateSetting = alternateSetting;
    inputctx->interfaceNumber = interfaceNumber;
    inputctx->configurationValue = configurationValue;
    if (ControlRequest(*endpoint0, usb_set_configuration(configurationValue))) {
        return true;
    } else {
        return false;
    }
}

std::shared_ptr<usb_endpoint>
xhci_port_enumerated_device::CreateInterruptEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress,
                                                     uint32_t maxPacketSize, uint8_t endpointNum,
                                                     usb_endpoint_direction dir, int pollingIntervalMs) {
    uint8_t interval{0};
    if (speed == LOW || speed == FULL) {
        if (pollingIntervalMs < 1) {
            pollingIntervalMs = 1;
        }
        int intervalUnits{pollingIntervalMs-1};
        while (intervalUnits > 0) {
            intervalUnits = intervalUnits >> 1;
            ++interval;
        }
        if (interval > 7) {
            interval = 7;
        }
        interval += 3;
    } else {
        interval = pollingIntervalMs;
        if (interval < 1) {
            interval = 1;
        }
        if (interval > 16) {
            interval = 16;
        }
        --interval;
    }
    uint32_t maxBurst{0};
    uint32_t maxESITpayload{0};
    switch (speed) {
        case LOW:
        case FULL:
            maxBurst = 0;
            maxESITpayload = 8;
            break;
        case HIGH:
            maxESITpayload = 64;
            break;
        default:
            maxESITpayload = 512;
            break;
    }
    auto *inputctx = &(inputctx_container->InputContext()->context[0]);
    uint8_t endpointIndex = ((endpointNum << 1) + (dir == usb_endpoint_direction::IN ? 1 : 0));
    auto *slotContext = inputctx_container->InputContext()->GetSlotContext(xhciRef.contextSize64);
    if (slotContext->contextEntries <= endpointIndex) {
        inputctx->addContextFlags = 1;
        inputctx->dropContextFlags = 0;
        slotContext->maxExitLatency = 0;
        slotContext->contextEntries = endpointIndex + 1;
    }
    inputctx->addContextFlags = (1 << endpointIndex) | 1;
    --endpointIndex;
    inputctx->dropContextFlags = 0;
    std::shared_ptr<xhci_endpoint_ring_container> endpointRing{new xhci_endpoint_ring_container_endpoints};
    auto *endpoint = inputctx_container->InputContext()->EndpointContext(endpointIndex, xhciRef.contextSize64);
    endpoint->endpointState = 0;
    endpoint->reserved1 = 0;
    endpoint->mult = 0;
    endpoint->maxPrimaryStreams = 0;
    endpoint->linearStreamArray = false;
    endpoint->interval = interval;
    endpoint->maxESITPayloadHigh = 0;
    endpoint->errorCount = 3;
    endpoint->endpointType = dir == usb_endpoint_direction::IN ? 7 : 3;
    endpoint->reserved2 = 0;
    endpoint->hostInitiateDisable = false;
    endpoint->maxBurstSize = maxBurst;
    endpoint->maxPacketSize = maxPacketSize;
    endpoint->trDequePointer = endpointRing->RingPhysAddr() | XHCI_TRB_CYCLE;
    endpoint->averageTrbLength = maxPacketSize;
    endpoint->maxESITPayloadLow = maxESITpayload;
    {
        auto evalContextCommand = xhciRef.ConfigureEndpoint(inputctx_container->InputContextPhys(), slot);
        if (!evalContextCommand) {
            return {};
        }
        bool done{false};
        int timeout = 50;
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(20ms);
        while (--timeout > 0) {
            if (evalContextCommand->IsDone()) {
                done = true;
                break;
            }
            std::this_thread::sleep_for(40ms);
        }
        if (!done) {
            return {};
        }
    }
#ifdef DEBUG_XHCI_DUMP_ENDPOINT
    {
        std::stringstream str{};
        str << "After creation endpoint state " << std::hex << xhciRef.resources->DCBAA()->contexts[slot]->GetEndpointState(endpointIndex, xhciRef.contextSize64)
            << "\n";
        get_klogger() << str.str().c_str();
    }
#endif
    xhci_endpoint *xhciEndpoint = new xhci_endpoint(xhciRef, endpointRing, inputctx_container, slot, endpointNum, dir, usb_endpoint_type::INTERRUPT, 0);
    std::shared_ptr<usb_endpoint> uendpoint{xhciEndpoint};
    {
        std::lock_guard lock{xhciRef.xhcilock};
        xhciRef.resources->DCBAA()->contexts[slot]->SetEndpoint(endpointIndex, xhciEndpoint);
    }
    return uendpoint;
}

std::shared_ptr<usb_endpoint>
xhci_port_enumerated_device::CreateBulkEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress,
                                                uint32_t maxPacketSize, uint8_t endpointNum,
                                                usb_endpoint_direction dir) {
    uint32_t maxBurst{0};
    switch (speed) {
        case LOW:
        case FULL:
        case HIGH:
            maxBurst = 0;
            break;
        default:
            break;
    }
    auto *inputctx = &(inputctx_container->InputContext()->context[0]);
    uint8_t endpointIndex = ((endpointNum << 1) + (dir == usb_endpoint_direction::IN ? 1 : 0));
    auto *slotContext = inputctx_container->InputContext()->GetSlotContext(xhciRef.contextSize64);
    if (slotContext->contextEntries <= endpointIndex) {
        inputctx->addContextFlags = 1;
        inputctx->dropContextFlags = 0;
        slotContext->maxExitLatency = 0;
        slotContext->contextEntries = endpointIndex + 1;
    }
    inputctx->addContextFlags = (1 << endpointIndex) | 1;
    --endpointIndex;
    inputctx->dropContextFlags = 0;
    std::shared_ptr<xhci_endpoint_ring_container> endpointRing{new xhci_endpoint_ring_container_endpoints};
    auto *endpoint = inputctx_container->InputContext()->EndpointContext(endpointIndex, xhciRef.contextSize64);
    endpoint->endpointState = 0;
    endpoint->reserved1 = 0;
    endpoint->mult = 0;
    endpoint->maxPrimaryStreams = 0;
    endpoint->linearStreamArray = false;
    endpoint->interval = 0;
    endpoint->maxESITPayloadHigh = 0;
    endpoint->errorCount = 3;
    endpoint->endpointType = dir == usb_endpoint_direction::IN ? 6 : 2;
    endpoint->reserved2 = 0;
    endpoint->hostInitiateDisable = false;
    endpoint->maxBurstSize = maxBurst;
    endpoint->maxPacketSize = maxPacketSize;
    endpoint->trDequePointer = endpointRing->RingPhysAddr() | XHCI_TRB_CYCLE;
    endpoint->averageTrbLength = maxPacketSize;
    endpoint->maxESITPayloadLow = 0;
    {
        auto evalContextCommand = xhciRef.ConfigureEndpoint(inputctx_container->InputContextPhys(), slot);
        if (!evalContextCommand) {
            return {};
        }
        bool done{false};
        int timeout = 100;
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(20ms);
        while (--timeout > 0) {
            if (evalContextCommand->IsDone()) {
                done = true;
                break;
            }
            std::this_thread::sleep_for(40ms);
        }
        if (!done) {
            return {};
        }
    }
#ifdef DEBUG_XHCI_DUMP_ENDPOINT
    {
        std::stringstream str{};
        str << "After creation endpoint state " << std::hex << xhciRef.resources->DCBAA()->contexts[slot]->GetEndpointState(endpointIndex, xhciRef.contextSize64)
            << "\n";
        get_klogger() << str.str().c_str();
    }
#endif
    xhci_endpoint *xhciEndpoint = new xhci_endpoint(xhciRef, endpointRing, inputctx_container, slot, endpointNum, dir, usb_endpoint_type::BULK, 0);
    std::shared_ptr<usb_endpoint> uendpoint{xhciEndpoint};
    {
        std::lock_guard lock{xhciRef.xhcilock};
        xhciRef.resources->DCBAA()->contexts[slot]->SetEndpoint(endpointIndex, xhciEndpoint);
    }
    return uendpoint;
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
                new xhci_root_port_enumeration_addressing(xhciRef, port)
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
                    new xhci_root_port_enumeration_addressing(xhciRef, port)
            );
        }
    }
    return {};
}

xhci_slot_data *xhci_port_enumeration_addressing::enable_slot() {
    auto enableSlotCommand = xhciRef.EnableSlot(0);
    if (!enableSlotCommand) {
        std::stringstream str{};
        str << "XHCI enable slot failed: Couldn't create command TRB on ring\n";
        get_klogger() << str.str().c_str();
        return nullptr;
    }
    bool done{false};
    int timeout = 100;
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(20ms);
    while (--timeout > 0) {
        if (enableSlotCommand->IsDone()) {
            done = true;
            break;
        }
        std::this_thread::sleep_for(40ms);
    }
    if (!done) {
        std::stringstream str{};
        str << "XHCI enable slot failed with code "<< enableSlotCommand->CompletionCode() << "\n";
        get_klogger() << str.str().c_str();
        return nullptr;
    }
    slot = enableSlotCommand->SlotId();
    deviceData = xhciRef.resources->CreateDeviceData();
    auto *slotData = deviceData->SlotData();
    xhciRef.resources->DCBAA()->contexts[slot] = slotData;
    xhciRef.resources->DCBAA()->phys[slot] = deviceData->SlotContextPhys();
    return slotData;
}

xhci_input_context *xhci_port_enumeration_addressing::set_address(xhci_slot_data &slotData, usb_speed speed, uint32_t routeString, uint8_t hubSlot, usb_speed hubSpeed, uint8_t hubPort) {
    inputctx_container = std::shared_ptr<xhci_input_context_container>(new xhci_input_context_container_32);
    xhci_input_context *inputctx = inputctx_container->InputContext();
    {
        xhci_input_control_context *inputContext = &(inputctx->context[0]);
        xhci_slot_context *slotContext = inputctx->GetSlotContext(xhciRef.contextSize64);
        inputContext->addContextFlags = 3; // A0+A1
        switch (speed) {
            case LOW:
                slotContext->was_speed_now_deprecated = 2;
                break;
            case FULL:
                slotContext->was_speed_now_deprecated = 1;
                break;
            case HIGH:
                slotContext->was_speed_now_deprecated = 3;
                break;
            case SUPER:
                slotContext->was_speed_now_deprecated = 4;
                break;
            default:
                slotContext->was_speed_now_deprecated = 0;
        }
        slotContext->rootHubPortNumber = rootHubPort + 1;
        slotContext->routeString = routeString;
        slotContext->contextEntries = 1;
        slotContext->maxExitLatency = 0;
        if (routeString == 0 || speed == HIGH) {
            slotContext->parentHubSlotId = 0;
            slotContext->parentPortNumber = 0;
        } else {
            if (speed == LOW || speed == FULL) {
                if (hubSpeed == LOW || hubSpeed == FULL) {
                    slotContext->parentHubSlotId = 0;
                    slotContext->parentPortNumber = 0;
                } else {
                    slotContext->parentHubSlotId = hubSlot;
                    slotContext->parentPortNumber = hubPort + 1;
                }
            } else {
                if (hubSpeed == SUPERPLUS && speed != hubSpeed) {
                    slotContext->parentHubSlotId = hubSlot;
                    slotContext->parentPortNumber = hubPort + 1;
                } else {
                    slotContext->parentHubSlotId = 0;
                    slotContext->parentPortNumber = 0;
                }
            }
        }
        auto *endpoint0 = inputctx->EndpointContext(0, xhciRef.contextSize64);
        endpoint0->trDequePointer = deviceData->Endpoint0RingPhys() | XHCI_TRB_CYCLE;
        endpoint0->maxPacketSize = 8;
        {
            auto setAddressCommand = xhciRef.SetAddress(inputctx_container->InputContextPhys(), slot);
            if (!setAddressCommand) {
                std::stringstream str{};
                str << "XHCI set address failed: Couldn't create command TRB on ring\n";
                get_klogger() << str.str().c_str();
                disable_slot();
                return nullptr;
            }
            bool done{false};
            int timeout = 100;
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(20ms);
            while (--timeout > 0) {
                if (setAddressCommand->IsDone()) {
                    done = true;
                    break;
                }
                std::this_thread::sleep_for(40ms);
            }
            if (!done) {
                std::stringstream str{};
                str << "XHCI set address failed with code " << setAddressCommand->CompletionCode() << "\n";
                get_klogger() << str.str().c_str();
                disable_slot();
                return nullptr;
            }
        }
    }
    {
        auto *slotContext = &(slotData.slotContext[0]);
        this->addr = slotContext->deviceAddress;
        std::stringstream str{};
        str << "XHCI enabled slot with ID " << slot << " addr " << this->addr << "\n";
        get_klogger() << str.str().c_str();
        memmove(inputctx->GetSlotContext(xhciRef.contextSize64), slotContext, 64);
    }
    return inputctx;
}

std::shared_ptr<usb_endpoint> xhci_port_enumeration_addressing::configure_baseline(usb_minimum_device_descriptor &minDevDesc, xhci_input_context &inputctx) {
    std::shared_ptr<xhci_endpoint_ring_container> ringContainer{new xhci_endpoint_ring_container_endpoint0(deviceData)};
    std::shared_ptr<xhci_endpoint> endpoint0{new xhci_endpoint(xhciRef, ringContainer, {}, slot, 0, usb_endpoint_direction::BOTH, usb_endpoint_type::CONTROL, 0)};
    {
        std::lock_guard lock{xhciRef.xhcilock};
        xhciRef.resources->DCBAA()->contexts[slot]->SetEndpoint(0, &(*endpoint0));
    }
    {
        usb_get_descriptor get_descr0{DESCRIPTOR_TYPE_DEVICE, 0, 8};
        std::shared_ptr<usb_buffer> descr0_buf = ControlRequest(*endpoint0, get_descr0);
        if (descr0_buf) {
            descr0_buf->CopyTo(minDevDesc);
            {
                std::stringstream str{};
                str << std::hex << "Dev desc len=" << minDevDesc.bLength
                    << " type=" << minDevDesc.bDescriptorType << " USB=" << minDevDesc.bcdUSB
                    << " class=" << minDevDesc.bDeviceClass << " sub=" << minDevDesc.bDeviceSubClass
                    << " proto=" << minDevDesc.bDeviceProtocol << " packet-0=" << minDevDesc.bMaxPacketSize0
                    << "\n";
                get_klogger() << str.str().c_str();
            }
        } else {
            get_klogger() << "Dev desc error\n";
            disable_slot();
            return {};
        }
    }
    {
        auto *inputContext = &(inputctx.context[0]);
        inputContext->addContextFlags = 2; // A1=endpoint0
        auto *endpoint0_ctx = inputctx.EndpointContext(0, xhciRef.contextSize64);
        {
            uint64_t phys = deviceData->Endpoint0RingPhys();
            phys += sizeof(xhci_trb) * endpoint0->index;
            endpoint0_ctx->trDequePointer = phys | XHCI_TRB_CYCLE;
        }
        endpoint0_ctx->maxPacketSize = minDevDesc.bMaxPacketSize0;
        {
            auto evalContextCommand = xhciRef.EvaluateContext(inputctx_container->InputContextPhys(), slot);
            if (!evalContextCommand) {
                std::stringstream str{};
                str << "XHCI evaluate context failed to create command TRB on ring\n";
                get_klogger() << str.str().c_str();
                disable_slot();
                return {};
            }
            bool done{false};
            int timeout = 100;
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(20ms);
            while (--timeout > 0) {
                if (evalContextCommand->IsDone()) {
                    done = true;
                    break;
                }
                std::this_thread::sleep_for(40ms);
            }
            if (!done) {
                std::stringstream str{};
                str << "XHCI evaluate context failed with code " << evalContextCommand->CompletionCode() << "\n";
                get_klogger() << str.str().c_str();
                disable_slot();
                return {};
            }
        }
    }
    return endpoint0;
}

uint8_t xhci_port_enumeration_addressing::get_address() {
    return addr;
}

void xhci_port_enumeration_addressing::disable_slot() {
    auto disableSlotCommand = xhciRef.DisableSlot(slot);
    if (!disableSlotCommand) {
        std::stringstream str{};
        str << "XHCI disable slot failed: Failed to create command TRB on ring\n";
        get_klogger() << str.str().c_str();
        return;
    }
    {
        bool done{false};
        int timeout = 200;
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(20ms);
        while (--timeout > 0) {
            if (disableSlotCommand->IsDone()) {
                done = true;
                break;
            }
            std::this_thread::sleep_for(40ms);
        }
        if (!done) {
            std::stringstream str{};
            str << "XHCI disable slot failed with code "<< disableSlotCommand->CompletionCode() << "\n";
            get_klogger() << str.str().c_str();
            return;
        }
        std::stringstream str{};
        str << "XHCI disabled slot with ID " << slot << "\n";
        get_klogger() << str.str().c_str();
    }
}

std::shared_ptr<usb_hw_enumerated_device> xhci_root_port_enumeration_addressing::set_address(uint8_t addr) {
    auto *slotData = enable_slot();
    if (slotData == nullptr) {
        return {};
    }
    auto speed = xhciRef.PortSpeed(port);
    auto *inputctx = xhci_port_enumeration_addressing::set_address(*slotData, speed);
    if (inputctx == nullptr) {
        return {};
    }
    usb_minimum_device_descriptor minDevDesc{};
    std::shared_ptr<usb_endpoint> endpoint0 = configure_baseline(minDevDesc, *inputctx);
    if (!endpoint0) {
        return {};
    }
    std::shared_ptr<usb_hw_enumerated_device> enumeratedDevice{
            new xhci_port_enumerated_device(
                    xhciRef, DeviceData(), endpoint0, InputContextContainer(), minDevDesc, speed, Slot()
            )
    };
    return enumeratedDevice;
}

xhci_hub_port_enumeration_addressing::xhci_hub_port_enumeration_addressing(xhci &xhciRef,
                                                                           const std::vector<uint8_t> &portRouting,
                                                                           usb_speed speed, uint8_t hubSlot,
                                                                           usb_speed hubSpeed) :
        xhci_port_enumeration_addressing(xhciRef, portRouting[0]), xhciRef(xhciRef), speed(speed), routeString(0), hubSlot(hubSlot), hubSpeed(hubSpeed) {
    auto iterator = portRouting.begin();
    if (iterator != portRouting.end()) {
        ++iterator;
        uint8_t shift = 0;
        while (iterator != portRouting.end() && shift < 16) {
            hubPort = *iterator;
            uint32_t bits = ((uint32_t) hubPort) & 0xF;
            bits = bits << shift;
            routeString |= bits;
            shift += 4;
        }
    }
}

std::shared_ptr<usb_hw_enumerated_device> xhci_hub_port_enumeration_addressing::set_address(uint8_t addr) {
    auto *slotData = enable_slot();
    if (slotData == nullptr) {
        return {};
    }
    auto *inputctx = xhci_port_enumeration_addressing::set_address(*slotData, speed, routeString, hubSlot, hubSpeed, hubPort);
    if (inputctx == nullptr) {
        return {};
    }
    usb_minimum_device_descriptor minDevDesc{};
    std::shared_ptr<usb_endpoint> endpoint0 = configure_baseline(minDevDesc, *inputctx);
    if (!endpoint0) {
        return {};
    }
    std::shared_ptr<usb_hw_enumerated_device> enumeratedDevice{
            new xhci_port_enumerated_device(
                    xhciRef, DeviceData(), endpoint0, InputContextContainer(), minDevDesc, speed, Slot()
            )
    };
    return enumeratedDevice;
}
