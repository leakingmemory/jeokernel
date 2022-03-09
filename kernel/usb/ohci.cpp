//
// Created by sigsegv on 28.05.2021.
//

#include <memory>
#include <sstream>
#include <klogger.h>
#include <core/vmem.h>
#include <thread>
#include <delay.h>
#include <core/nanotime.h>
#include <kernelconfig.h>
#include "ohci.h"

#define OHCI_PORT_STATUS_CCS    0x000001 // Current Connection Status
#define OHCI_PORT_STATUS_PES    0x000002 // Port Enable Status
#define OHCI_PORT_STATUS_PSS    0x000004 // Port Suspend Status
#define OHCI_PORT_STATUS_POCI   0x000008 // Port Over Current Indicator
#define OHCI_PORT_STATUS_PRS    0x000010 // Port Reset Status
#define OHCI_PORT_STATUS_PPS    0x000100 // Port Power Status
#define OHCI_PORT_STATUS_LSDA   0x000200 // Low Speed Device Attached
#define OHCI_PORT_STATUS_CSC    0x010000 // Connect Status Change
#define OHCI_PORT_STATUS_PESC   0x020000 // Port Enable Status Change
#define OHCI_PORT_STATUS_PSSC   0x040000 // Port Suspend Status Change
#define OHCI_PORT_STATUS_OCIC   0x080000 // Port Over Current Indicator Change
#define OHCI_PORT_STATUS_PRSC   0x100000 // Port Reset Status Change


Device *ohci_driver::probe(Bus &bus, DeviceInformation &deviceInformation) {
    if (
            deviceInformation.device_class == 0x0C /* serial bus */ &&
            deviceInformation.device_subclass == 3 /* USB */ &&
            deviceInformation.prog_if == 0x10 /* ohci */
    ) {
        return new ohci(bus, *(deviceInformation.GetPciInformation()));
    } else {
        return nullptr;
    }
}

//#define OHCI_START_VIA_RESUME

void ohci::init() {
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
            get_klogger() << "USB ohci controller is invalid (invalid BAR0, not memory mapped)\n";
            return;
        }
        if (bar0.is_64bit()) {
            addr = bar0.addr64();
        } else if (bar0.is_32bit()) {
            addr = (uint32_t) bar0.addr32();
        } else {
            get_klogger() << "USB ohci controller is invalid (invalid BAR0, invalid record)\n";
            return;
        }
        size = bar0.memory_size();
        if (size == 0) {
            get_klogger() << "USB ohci controller is invalid (invalid BAR0, invalid size)\n";
            return;
        }
        prefetch = bar0.is_prefetchable();
    }
    mapped_registers_vm = std::make_unique<vmem>(size);
    {
        uint64_t physaddr{addr & 0xFFFFFFFFFFFFFFF0};
        std::size_t pages = mapped_registers_vm->npages();
        for (std::size_t page = 0; page < pages; page++) {
            mapped_registers_vm->page(page).rwmap(physaddr, true, !prefetch);
            physaddr += 0x1000;
        }
    }
    using namespace std::literals::chrono_literals;
    ohciRegisters = (ohci_registers *) mapped_registers_vm->pointer();
    if ((ohciRegisters->HcControl & OHCI_CTRL_IR) != 0){
        //get_klogger() << "OHCI SMM active\n";
        ohciRegisters->HcCommandStatus = OHCI_CMD_OCR;
        do {
            std::this_thread::sleep_for(10ms);
        } while ((ohciRegisters->HcControl & OHCI_CTRL_IR) != 0);
        get_klogger() << "OHCI SMM disabled\n";
    } else if ((ohciRegisters->HcControl & OHCI_CTRL_HCFS_MASK) == OHCI_CTRL_HCFS_OPERATIONAL) {
        get_klogger() << "OHCI BIOS/Firmware driver operational\n";
    } else if ((ohciRegisters->HcControl & OHCI_CTRL_HCFS_MASK) != OHCI_CTRL_HCFS_RESET) {
        get_klogger() << "OHCI BIOS/Firmware driver not operational - wait for resume..\n";
        {
            uint32_t ctrl{ohciRegisters->HcControl};
            ctrl = ctrl & (~OHCI_CTRL_HCFS_MASK);
            ctrl |= OHCI_CTRL_HCFS_RESUME;
            ohciRegisters->HcControl = ctrl;
        }
        do {
            std::this_thread::sleep_for(10ms);
        } while ((ohciRegisters->HcControl & OHCI_CTRL_HCFS_MASK) != OHCI_CTRL_HCFS_OPERATIONAL);
        get_klogger() << "OHCI resumed\n";
        std::this_thread::sleep_for(10ms);
    } else {
        get_klogger() << "OHCI cold start\n";
        std::this_thread::sleep_for(2ms);
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
            pci->AddIrqHandler(*irq, [this]() {
                return this->irq();
            });
        }
    }

    dumpregs();

    // HC, etc, reset
    ohciRegisters->HcControl = OHCI_CTRL_HCFS_RESET;

    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(100ms);
    }

    dumpregs();

    uint32_t interval = ohciRegisters->HcFmInterval & OHCI_INTERVAL_FI_MASK;

    uint64_t timing{0};
    {
        critical_section cli{};

        uint64_t nanoref = get_nanotime_ref();

        if (!ResetHostController()) {
            return;
        }

        ohciRegisters->HcInterruptDisable = OHCI_INT_MASK;

        ohciRegisters->HcInterruptEnable = OHCI_INT_SCHEDULING_OVERRUN | OHCI_INT_SCHEDULING_WRITE_DONE_HEAD
                                           | OHCI_INT_SCHEDULING_RESUME_DETECTED
                                           | OHCI_INT_SCHEDULING_UNRECOVERABLE_ERR |
                                           OHCI_INT_SCHEDULING_FRAME_NUM_OVERFLOW
                                           | OHCI_INT_SCHEDULING_ROOT_HUB_STATUS_CH | OHCI_INT_MASTER_INTERRUPT;

        ohciRegisters->HcHCCA = hcca.Addr();

        ohciRegisters->HcControlHeadED = controlHead->Phys();

        ohciRegisters->HcBulkHeadED = bulkHead->Phys();

        {
            uint32_t ctrl = ohciRegisters->HcControl;
            ctrl &= ~(OHCI_CTRL_CBSR_MASK | OHCI_CTRL_HCFS_MASK | OHCI_CTRL_ENDPOINTS_MASK | OHCI_CTRL_IR);
            ctrl |= OHCI_CTRL_CBSR_1_4 | OHCI_CTRL_PLE | OHCI_CTRL_IE | OHCI_CTRL_CLE | OHCI_CTRL_BLE;
            // Unsuspend controller after HC reset:
#ifdef OHCI_START_VIA_RESUME
            ctrl |= OHCI_CTRL_HCFS_RESUME;
#else
            ctrl |= OHCI_CTRL_HCFS_OPERATIONAL;
#endif
            ohciRegisters->HcControl = ctrl;
        }

#ifdef OHCI_START_VIA_RESUME
        {
            int timeout = 10;
            do {
                delay_nano(10000);
                --timeout;
            } while (timeout > 0 && (ohciRegisters->HcControl & OHCI_CTRL_HCFS_OPERATIONAL) == 0);
            if (timeout == 0) {
                {
                    std::stringstream msg;
                    msg << DeviceType() << (unsigned int) DeviceId() << ": resume timeout\n";
                    get_klogger() << msg.str().c_str();
                }
                auto ct = ohciRegisters->HcControl;
                ct &= ~OHCI_CTRL_HCFS_MASK;
                ct |= OHCI_CTRL_HCFS_OPERATIONAL;
            }
        }
#endif

        {
            uint32_t max_full_speed_packet = (interval - 210) * 6 / 7;
            /* Flip the FIT-bit */
            uint32_t fmInterval = (ohciRegisters->HcFmInterval & OHCI_INTERVAL_FIT) ^ OHCI_INTERVAL_FIT;
            fmInterval |= (max_full_speed_packet << 16) | interval;
            ohciRegisters->HcFmInterval = fmInterval;
        }

        ohciRegisters->HcPeriodicStart = (ohciRegisters->HcPeriodicStart & 0xFFFFC000) | (interval * 9 / 10);

        /*
         * Clear interrupt status flags to accept new interrupts.
         */
        ohciRegisters->HcInterruptStatus = OHCI_INT_SCHEDULING_OVERRUN | OHCI_INT_SCHEDULING_WRITE_DONE_HEAD
                                           /*| OHCI_INT_SCHEDULING_START_OF_FRAME*/ | OHCI_INT_SCHEDULING_RESUME_DETECTED
                                           | OHCI_INT_SCHEDULING_UNRECOVERABLE_ERR | OHCI_INT_SCHEDULING_FRAME_NUM_OVERFLOW
                                           | OHCI_INT_SCHEDULING_ROOT_HUB_STATUS_CH;

        ohciRegisters->HcRhStatus = OHCI_HC_LPSC;

        timing = get_nanotime_ref() - nanoref;
    }
    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": started using total time " << timing << "ns\n";
        get_klogger() << msg.str().c_str();
    }

    dumpregs();

    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(70ms);
    }

    dumpregs();

    uint16_t retries = 20;
    while (no_ports == 0 || no_ports > 15) {
        if (--retries == 0) {
            break;
        }
        {
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10ms);
        }
        ohci_descriptor_a desc_a{ohciRegisters->HcRhDescriptorA};
        no_ports = desc_a.no_ports;
        power_on_port_delay_ms = desc_a.potpgt;
        power_on_port_delay_ms = power_on_port_delay_ms << 1;
    }

    {
        std::stringstream msg;
        //msg << DeviceType() << (unsigned int) DeviceId() << ": USB controller, memory registers 0x" << std::hex << addr << " size " << std::dec << size << " ports x" << (unsigned int) no_ports << "power on p. delay " << (unsigned int) power_on_port_delay_ms << "ms\n";
        msg << DeviceType() << (unsigned int) DeviceId() << ": USB controller, ports x" << (unsigned int) no_ports << " power on port " << (unsigned int) power_on_port_delay_ms << "ms\n";
        get_klogger() << msg.str().c_str();
    }

    bvalue = ohciRegisters->HcRhDescriptorB;

    Run();
}

bool ohci::irq() {
    std::lock_guard lock{ohcilock};
    uint32_t IrqHandled{0};
    uint32_t IrqStatus{0};
    do {
        uint32_t IrqEnabled{ohciRegisters->HcInterruptEnable};
        IrqStatus = ohciRegisters->HcInterruptStatus & IrqEnabled;
        if ((IrqStatus & OHCI_INT_SCHEDULING_OVERRUN) != 0) {
            get_klogger() << "ohci scheduling overrun\n";
        }
        if ((IrqStatus & OHCI_INT_SCHEDULING_WRITE_DONE_HEAD) != 0) {
            ProcessDoneQueue();
        }
        if ((IrqStatus & OHCI_INT_SCHEDULING_START_OF_FRAME) != 0) {
            IrqStatus = IrqStatus & ~OHCI_INT_SCHEDULING_START_OF_FRAME;
            ohciRegisters->HcInterruptDisable = OHCI_INT_SCHEDULING_START_OF_FRAME;
            StartOfFrame();
        }
        if ((IrqStatus & OHCI_INT_SCHEDULING_RESUME_DETECTED) != 0) {
            get_klogger() << "ohci resume detected\n";
        }
        if ((IrqStatus & OHCI_INT_SCHEDULING_UNRECOVERABLE_ERR) != 0) {
            get_klogger() << "ohci unrecoverable error\n";
        }
        if ((IrqStatus & OHCI_INT_SCHEDULING_FRAME_NUM_OVERFLOW) != 0) {
        }
        if ((IrqStatus & OHCI_INT_SCHEDULING_ROOT_HUB_STATUS_CH) != 0) {
            RootHubStatusChange();
        }
        ohciRegisters->HcInterruptStatus = IrqStatus;
        IrqHandled |= IrqStatus;
    } while (IrqStatus != 0);
    return IrqHandled != 0;
}

void ohci::StartOfFrame() {
    if ((ohciRegisters->HcControl & OHCI_CTRL_CLE) == 0) {
        ohciRegisters->HcControlCurrentED = 0;
        ohciRegisters->HcControl |= OHCI_CTRL_CLE;
    }
    destroyEds.clear();
}

uint32_t ohci::GetPortStatus(int port) {
    uint32_t status{ohciRegisters->PortStatus[port]};
    uint32_t usb{0};
    if ((status & OHCI_PORT_STATUS_CCS) != 0) {
        usb |= USB_PORT_STATUS_CCS;
    }
    if ((status & OHCI_PORT_STATUS_PES) != 0) {
        usb |= USB_PORT_STATUS_PES;
    }
    if ((status & OHCI_PORT_STATUS_PSS) != 0) {
        usb |= USB_PORT_STATUS_PSS;
    }
    if ((status & OHCI_PORT_STATUS_POCI) != 0) {
        usb |= USB_PORT_STATUS_POCI;
    }
    if ((status & OHCI_PORT_STATUS_PRS) != 0) {
        usb |= USB_PORT_STATUS_PRS;
    }
    if ((status & OHCI_PORT_STATUS_PPS) != 0) {
        usb |= USB_PORT_STATUS_PPS;
    }
    if ((status & OHCI_PORT_STATUS_LSDA) != 0) {
        usb |= USB_PORT_STATUS_LSDA;
    }
    if ((status & OHCI_PORT_STATUS_CSC) != 0) {
        usb |= USB_PORT_STATUS_CSC;
    }
    if ((status & OHCI_PORT_STATUS_PESC) != 0) {
        usb |= USB_PORT_STATUS_PESC;
    }
    if ((status & OHCI_PORT_STATUS_PSSC) != 0) {
        usb |= USB_PORT_STATUS_PSSC;
    }
    if ((status & OHCI_PORT_STATUS_OCIC) != 0) {
        usb |= USB_PORT_STATUS_OCIC;
    }
    if ((status & OHCI_PORT_STATUS_PRSC) != 0) {
        usb |= USB_PORT_STATUS_PRSC;
    }
    return usb;
}

void ohci::SwitchPortOff(int port) {
    std::lock_guard lock{ohcilock};
    if (PortPowerIndividuallyControlled(port)) {
        ohciRegisters->PortStatus[port] = OHCI_PORT_STATUS_CCS;
    }
}

void ohci::SwitchPortOn(int port) {
    std::lock_guard lock{ohcilock};
    if (PortPowerIndividuallyControlled(port)) {
        ohciRegisters->PortStatus[port] = OHCI_PORT_STATUS_PPS;
    }
}

void ohci::ClearStatusChange(int port, uint32_t statuses) {
    std::lock_guard lock{ohcilock};
    uint32_t usb{0};
    if ((statuses & USB_PORT_STATUS_CSC) != 0) {
        usb |= OHCI_PORT_STATUS_CSC;
    }
    if ((statuses & USB_PORT_STATUS_PESC) != 0) {
        usb |= OHCI_PORT_STATUS_PESC;
    }
    if ((statuses & USB_PORT_STATUS_PSSC) != 0) {
        usb |= OHCI_PORT_STATUS_PSSC;
    }
    if ((statuses & USB_PORT_STATUS_OCIC) != 0) {
        usb |= OHCI_PORT_STATUS_OCIC;
    }
    if ((statuses & USB_PORT_STATUS_PRSC) != 0) {
        usb |= OHCI_PORT_STATUS_PRSC;
    }
    ohciRegisters->PortStatus[port] = usb;
}

bool ohci::ResetHostController() {
    ohciRegisters->HcCommandStatus = OHCI_CMD_HCR;
    for (int i = 0; i < 10; i++) {
        {
            delay_nano(10000);
        }
        auto cmdStat = ohciRegisters->HcCommandStatus;
        if ((cmdStat & OHCI_CMD_HCR) == 0) {
            return true;
        }
    }

    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": Error: Host controller reset timeout\n";
        get_klogger() << msg.str().c_str();
    }

    return false;
}

void ohci::dumpregs() {
    std::stringstream str{};
    str << DeviceType() << (unsigned int) DeviceId()
        << ": C=" << std::hex << ohciRegisters->HcControl
        << " CS=" << ohciRegisters->HcCommandStatus << " Int="
        << ohciRegisters->HcInterruptStatus << " Per="
        << ohciRegisters->HcPeriodicStart << " FmInt="
        << ohciRegisters->HcFmInterval << " A=" << ohciRegisters->HcRhDescriptorA
        << " B=" << ohciRegisters->HcRhDescriptorB
        << " IE=" << ohciRegisters->HcInterruptEnable << "\n";
    get_klogger() << str.str().c_str();
}

void ohci::EnablePort(int port) {
    std::lock_guard lock{ohcilock};
    ohciRegisters->PortStatus[port] = OHCI_PORT_STATUS_PES;
}

void ohci::DisablePort(int port) {
    std::lock_guard lock{ohcilock};
    ohciRegisters->PortStatus[port] = OHCI_PORT_STATUS_CCS;
}

void ohci::ResetPort(int port) {
    std::lock_guard lock{ohcilock};
    ohciRegisters->PortStatus[port] = OHCI_PORT_STATUS_PRS;
}

std::shared_ptr<usb_endpoint>
ohci::CreateControlEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize,
                            uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) {
    if (speed == LOW || speed == FULL) {
        auto endpoint = new ohci_endpoint(*this, maxPacketSize, functionAddr, endpointNum, dir, speed,
                                          usb_endpoint_type::CONTROL);
        return std::shared_ptr<usb_endpoint>(endpoint);
    } else {
        return {};
    }
}

std::shared_ptr<usb_endpoint> ohci::CreateInterruptEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed, int pollingIntervalMs) {
    if (speed == LOW || speed == FULL) {
        auto endpoint = new ohci_endpoint(*this, maxPacketSize, functionAddr, endpointNum, dir, speed,
                                          usb_endpoint_type::INTERRUPT, pollingIntervalMs);
        return std::shared_ptr<usb_endpoint>(endpoint);
    } else {
        return {};
    }
}

std::shared_ptr<usb_endpoint>
ohci::CreateBulkEndpoint(const std::vector<uint8_t> &portRouting, uint8_t hubAddress, uint32_t maxPacketSize,
                         uint8_t functionAddr, uint8_t endpointNum, usb_endpoint_direction dir, usb_speed speed) {
    if (speed == LOW || speed == FULL) {
        auto endpoint = new ohci_endpoint(*this, maxPacketSize, functionAddr, endpointNum, dir, speed,
                                          usb_endpoint_type::BULK);
        return std::shared_ptr<usb_endpoint>(endpoint);
    } else {
        return {};
    }
}

std::shared_ptr<usb_buffer> ohci::Alloc(size_t size) {
    if (size <= OHCI_SHORT_TRANSFER_BUFSIZE) {
        std::shared_ptr<usb_buffer> buffer{new ohci_buffer<OHCI_SHORT_TRANSFER_BUFSIZE>(shortBufPool.Alloc())};
        return buffer;
    } else if (size <= OHCI_TRANSFER_BUFSIZE) {
        std::shared_ptr<usb_buffer> buffer{new ohci_buffer<OHCI_TRANSFER_BUFSIZE>(bufPool.Alloc())};
        return buffer;
    } else {
        return {};
    }
}

usb_speed ohci::PortSpeed(int port) {
    return (ohciRegisters->PortStatus[port] & OHCI_PORT_STATUS_LSDA) == 0 ? FULL : LOW;
}

void ohci::ProcessDoneQueue() {
    uint32_t doneHead = hcca.Hcca().HccaDoneHead & 0xFFFFFFF0;
    uint32_t prevHead = 0;
    {
        std::vector<std::shared_ptr<usb_transfer>> doneItems{};
        while (doneHead != 0) {
#ifdef OHCI_DEBUGPRINTS_ENDPOINTS
            {
                std::stringstream str{};
                str << std::hex << "Done queue item " << doneHead << "\n";
                get_klogger() << str.str().c_str();
            }
#endif
            std::shared_ptr<usb_transfer> item = ExtractDoneQueueItem(doneHead);
            if (!item) {
                std::stringstream str{};
                str << "done queue item " << std::hex << doneHead << " not found after " << prevHead;
                wild_panic(str.str().c_str());
            }
            doneItems.push_back(item);
            ohci_transfer &ohciTransfer = *((ohci_transfer *) &(*item));
            prevHead = doneHead;
            doneHead = ohciTransfer.TD()->NextTD;
        }
        auto iterator = doneItems.end();
        while (iterator != doneItems.begin()) {
            --iterator;
            (*iterator)->SetDone();
        }
    }
    auto iter = transfersInProgress.begin();
    while (iter != transfersInProgress.end()) {
        std::shared_ptr<usb_transfer> item = *iter;
        ohci_transfer &ohciTransfer = *((ohci_transfer *) &(*item));
        if (ohciTransfer.waitCancelled) {
            if (ohciTransfer.waitCancelledAndWaitedCycle) {
                transfersInProgress.erase(iter);
            } else {
                ohciTransfer.waitCancelledAndWaitedCycle = true;
                ++iter;
            }
        } else {
            ++iter;
        }
    }
}

std::shared_ptr<usb_transfer> ohci::ExtractDoneQueueItem(uint32_t physaddr) {
    auto iter = transfersInProgress.begin();
    while (iter != transfersInProgress.end()) {
        std::shared_ptr<usb_transfer> transfer{*iter};
        ohci_transfer &ohciTransfer = *((ohci_transfer *) &(*transfer));
        if (ohciTransfer.PhysAddr() == physaddr) {
            transfersInProgress.erase(iter);
            return transfer;
        }
        ++iter;
    }
    return {};
}

OhciHcca::OhciHcca() : page(sizeof(*hcca)), hcca((typeof(hcca)) page.Pointer()), hcca_ed_ptrs(), cycle(0) {
    for (int i = 0; i < 0x1F; i++) {
        hcca_ed_ptrs[i].ed = &(hcca->hcca_with_eds.eds[i]);
        hcca_ed_ptrs[i].head = nullptr;
        hcca_ed_ptrs[i].index = i;
        hcca_root_ptrs[i].ed = nullptr;
        hcca_root_ptrs[i].head = nullptr;
        hcca_root_ptrs[i].index = i;
        hcca->hcca_with_eds.eds[i].HcControl = 0;
        hcca->hcca_with_eds.eds[i].HeadP = 0;
        hcca->hcca_with_eds.eds[i].TailP = 0;
    }
    hcca_root_ptrs[0x1F].ed = nullptr;
    hcca_root_ptrs[0x1F].head = nullptr;
    hcca_root_ptrs[0x1F].index = 0x1F;
    for (int i = 0; i < 0x10; i++) {
        int j = (i >> 1) + 0x10;
        hcca->hcca_with_eds.eds[i].NextED = page.PhysAddr() + hcca->hcca_with_eds.RelativeAddr(&(hcca->hcca_with_eds.eds[j]));
    }
    for (int i = 0; i < 8; i++) {
        int j = (i >> 1) + 0x18;
        hcca->hcca_with_eds.eds[0x10 + i].NextED = page.PhysAddr() + hcca->hcca_with_eds.RelativeAddr(&(hcca->hcca_with_eds.eds[j]));
    }
    for (int i = 0; i < 4; i++) {
        int j = (i >> 1) + 0x1C;
        hcca->hcca_with_eds.eds[0x18 + i].NextED = page.PhysAddr() + hcca->hcca_with_eds.RelativeAddr(&(hcca->hcca_with_eds.eds[j]));
    }
    for (int i = 0; i < 2; i++) {
        int j = 0x1E;
        hcca->hcca_with_eds.eds[0x1C + i].NextED = page.PhysAddr() + hcca->hcca_with_eds.RelativeAddr(&(hcca->hcca_with_eds.eds[j]));
    }
    hcca->hcca_with_eds.eds[0x1E].NextED = 0;

    hcca->hc_control_ed.NextED = 0;
    hcca->hc_control_ed.TailP = 0;
    hcca->hc_control_ed.HeadP = 0;
    hcca->hc_control_ed.HcControl = 0;

    hcca->hc_bulk_ed.NextED = 0;
    hcca->hc_bulk_ed.TailP = 0;
    hcca->hc_bulk_ed.HeadP = 0;
    hcca->hc_bulk_ed.HcControl = 0;

    static const uint8_t Balance[16] =
            {0x0, 0x8, 0x4, 0xC, 0x2, 0xA, 0x6, 0xE, 0x1, 0x9, 0x5, 0xD, 0x3, 0xB, 0x7, 0xF};

    for (uint8_t i = 0; i < 16; i++) {
        uint8_t bal = Balance[i] >> 1;
        uint8_t idx = i << 1;
        hcca->hcca_with_eds.hcca.hcca.intr_ed[idx] = hcca->hcca_with_eds.RelativeAddr(hcca_ed_ptrs[bal].ed) + page.PhysAddr();
        hcca->hcca_with_eds.hcca.hcca.intr_ed[idx + 1] = hcca->hcca_with_eds.RelativeAddr(hcca_ed_ptrs[8 + bal].ed) + page.PhysAddr();
    }
}

#define OHCI_ED_DIRECTION_BOTH 0x0000
#define OHCI_ED_DIRECTION_OUT  0x0800
#define OHCI_ED_DIRECTION_IN   0x1000

#define OHCI_ED_SPEED_LOW      0x2000
#define OHCI_ED_SKIP           0x4000
#define OHCI_ED_ISOC_TD        0x8000


ohci_endpoint::ohci_endpoint(ohci &ohci, usb_endpoint_type endpointType) :
        ohciRef(ohci),
        edPtr(ohci.edPool.Alloc()),
        head(), tail(),
        endpointType(endpointType),
        next(nullptr), int_ed_chain(nullptr) {
    auto *ptr = edPtr->Pointer();
    ptr->NextED = 0;
    ptr->TailP = 0;
    ptr->HeadP = 0;
    ptr->HcControl = 0;
}

ohci_endpoint::ohci_endpoint(ohci &ohci, uint32_t maxPacketSize, uint8_t functionAddr, uint8_t endpointNum,
                             usb_endpoint_direction dir, usb_speed speed, usb_endpoint_type endpointType,
                             int pollingRateMs) :
                             ohciRef(ohci),
                             edPtr(ohci.edPool.Alloc()),
                             head(new ohci_transfer(ohci, *this)), tail(head),
                             endpointType(endpointType),
                             next(nullptr), int_ed_chain(nullptr) {
    ohci_endpoint_descriptor *ed = edPtr->Pointer();
    {
        uint32_t control = ((maxPacketSize & 0x7FF) << 16)
                           | ((endpointNum & 0x0F) << 7)
                           | (functionAddr & 0x07F);
        if (dir == usb_endpoint_direction::IN) {
            control |= OHCI_ED_DIRECTION_IN;
        } else if (dir == usb_endpoint_direction::OUT) {
            control |= OHCI_ED_DIRECTION_OUT;
        }
        if (speed == LOW) {
            control |= OHCI_ED_SPEED_LOW;
        }
        ed->HcControl = control;
    }
    ed->HeadP = head->PhysAddr();
    ed->TailP = tail->PhysAddr();

    std::lock_guard lock{ohci.ohcilock};

    if (endpointType == usb_endpoint_type::BULK) {
        ed->NextED = ohci.ohciRegisters->HcBulkHeadED;
        ohci.ohciRegisters->HcBulkHeadED = edPtr->Phys();
        next = ohci.bulkHead;
        ohci.bulkHead = this;
    }
    if (endpointType == usb_endpoint_type::CONTROL) {
        ed->NextED = ohci.ohciRegisters->HcControlHeadED;
        ohci.ohciRegisters->HcControlHeadED = edPtr->Phys();
        next = ohci.controlHead;
        ohci.controlHead = this;
    }
    if (endpointType == usb_endpoint_type::INTERRUPT) {
        auto &edptr = ohci.hcca.GetInterruptEDHead(pollingRateMs);
        if (edptr.ed != nullptr) {
            ed->NextED = edptr.ed->NextED;
            edptr.ed->NextED = edPtr->Phys();
        } else {
            ed->NextED = ohci.hcca.Hcca().intr_ed[edptr.index];
            ohci.hcca.Hcca().intr_ed[edptr.index] = edPtr->Phys();
        }
        next = edptr.head;
        edptr.head = this;
        int_ed_chain = &edptr;
    }
#ifdef OHCI_DEBUGPRINTS_ENDPOINTS
    {
        std::stringstream str{};
        str << std::hex << "Endpoint startup status ctrl=" << this->edPtr->Pointer()->HcControl<<" head="<<this->edPtr->Pointer()->HeadP<<" tail="<<this->edPtr->Pointer()->TailP << "\n";
        get_klogger() << str.str().c_str();
    }
#endif
}

ohci_endpoint::~ohci_endpoint() {
    {
        std::lock_guard lock{ohciRef.ohcilock};
        SetSkip();
        ohci_endpoint *endpoint{nullptr};
        if (endpointType == usb_endpoint_type::CONTROL) {
            endpoint = ohciRef.controlHead;
        }
        if (endpointType == usb_endpoint_type::INTERRUPT) {
            endpoint = int_ed_chain->head;
        }
        if (endpoint == this) {
            if (endpointType == usb_endpoint_type::BULK) {
                if (endpoint->next != nullptr) {
                    this->ohciRef.ohciRegisters->HcBulkCurrentED = endpoint->next->edPtr->Phys();
                    this->ohciRef.bulkHead = endpoint->next;
                } else {
                    this->ohciRef.ohciRegisters->HcBulkCurrentED = 0;
                    this->ohciRef.bulkHead = nullptr;
                }
            }
            if (endpointType == usb_endpoint_type::CONTROL) {
                if (endpoint->next != nullptr) {
                    this->ohciRef.ohciRegisters->HcControlHeadED = endpoint->next->edPtr->Phys();
                    this->ohciRef.controlHead = endpoint->next;
                } else {
                    this->ohciRef.ohciRegisters->HcControlHeadED = 0;
                    this->ohciRef.controlHead = nullptr;
                }
            }
            if (endpointType == usb_endpoint_type::INTERRUPT)
            {
                if (int_ed_chain->ed != nullptr)
                    int_ed_chain->ed->NextED = endpoint->edPtr->Pointer()->NextED;
                else
                    ohciRef.hcca.Hcca().intr_ed[int_ed_chain->index] = endpoint->edPtr->Pointer()->NextED;
                int_ed_chain->head = next;
            }
            ohciRef.destroyEds.emplace_back(ohciRef, edPtr, head);
        } else
            while (endpoint) {
                if (endpoint->next == this) {
                    endpoint->SetNext(next);
                    ohciRef.destroyEds.emplace_back(ohciRef, edPtr, head);
                }
                endpoint = endpoint->next;
            }
        if (endpointType == usb_endpoint_type::CONTROL || endpointType == usb_endpoint_type::BULK) {
            this->ohciRef.ohciRegisters->HcControl &= ~OHCI_CTRL_CLE;
            this->ohciRef.ohciRegisters->HcInterruptStatus = OHCI_INT_SCHEDULING_START_OF_FRAME;
            this->ohciRef.ohciRegisters->HcInterruptEnable = OHCI_INT_SCHEDULING_START_OF_FRAME;
        }
    }
#ifdef OHCI_DEBUGPRINTS_ENDPOINTS
    get_klogger() << "Transfers:\n";
#endif
    {
        std::lock_guard lock{ohciRef.ohcilock};
        std::shared_ptr<ohci_transfer> transfer = head;
        while (transfer->next) {
            transfer->waitCancelled = true;
#ifdef OHCI_DEBUGPRINTS_ENDPOINTS
            auto *TD = transfer->TD();
            {
                std::stringstream str{};
                str << std::hex << "Cancelling - c=" << TD->TdControl
                    << " buf=" << TD->CurrentBufferPointer << "/" << TD->BufferEnd
                    << " next=" << TD->NextTD << "\n";
                get_klogger() << str.str().c_str();
            }
#endif
            transfer = transfer->next;
        }
    }
#ifdef OHCI_DEBUGPRINTS_ENDPOINTS
    {
        std::stringstream str{};
        str << std::hex << "Endpoint finalizing status ctrl=" << this->edPtr->Pointer()->HcControl<<" head="<<this->edPtr->Pointer()->HeadP<<" tail="<<this->edPtr->Pointer()->TailP << "\n";
        get_klogger() << str.str().c_str();
    }
#endif
}

void ohci_endpoint::SetNext(ohci_endpoint *endpoint) {
    ohci_endpoint_descriptor *ed = edPtr->Pointer();
    ed->NextED = endpoint->edPtr->Phys();
    this->next = endpoint;
}

uint32_t ohci_endpoint::Phys() {
    return edPtr->Phys();
}

std::shared_ptr<usb_transfer>
ohci_endpoint::CreateTransfer(std::shared_ptr<usb_buffer> buffer, uint32_t size, usb_transfer_direction direction, bool bufferRounding,
                              uint16_t delayInterrupt, int8_t dataToggle, std::function<void (ohci_transfer &transfer)> &applyFunc) {
    std::shared_ptr<usb_transfer> transfer{};
    {
        std::lock_guard lock{ohciRef.ohcilock};
        transfer = CreateTransferWithLock(buffer, size, direction, bufferRounding, delayInterrupt, dataToggle,
                                          applyFunc);
    }
    return transfer;
}

std::shared_ptr<usb_transfer>
ohci_endpoint::CreateTransferWithLock(std::shared_ptr<usb_buffer> buffer, uint32_t size, usb_transfer_direction direction, bool bufferRounding,
                              uint16_t delayInterrupt, int8_t dataToggle, std::function<void (ohci_transfer &transfer)> &applyFunc) {
    uint32_t ctrl{0};
    if (dataToggle >= 0) {
        ctrl |= ((uint32_t) (dataToggle & 1) | 2) << 24;
    }
    if (delayInterrupt == TRANSFER_NO_INTERRUPT) {
        delayInterrupt = 7;
    } else if (delayInterrupt > 6) {
        delayInterrupt = 6;
    }
    ctrl |= ((uint32_t) 0xF) << 28; // Condition code 0xF - Not accessed
    ctrl |= ((uint32_t) delayInterrupt) << 21;
    switch (direction) {
        case usb_transfer_direction::IN:
            ctrl |= 1 << 20;
            break;
        case usb_transfer_direction::OUT:
            ctrl |= 1 << 19;
            break;
    }
    if (bufferRounding) {
        ctrl |= 1 << 18;
    }
    auto newtd = std::make_shared<ohci_transfer>(ohciRef, *this);

    {
        auto *TD = newtd->TD();
        TD->NextTD = 0;
        TD->BufferEnd = 0;
        TD->CurrentBufferPointer = 0;
        TD->TdControl = 0;
    }
    tail->buffer = buffer;
    auto *TD = tail->TD();
    TD->TdControl = ctrl;
    if (buffer) {
        TD->CurrentBufferPointer = buffer->Addr();
        if (size > buffer->Size()) {
            size = buffer->Size();
        }
        TD->BufferEnd = TD->CurrentBufferPointer + size - 1;
    } else {
        TD->CurrentBufferPointer = 0;
        TD->BufferEnd = 0;
    }
    applyFunc(*tail);
    std::shared_ptr<ohci_transfer> filled{tail};
    tail->next = newtd;
    TD->NextTD = newtd->PhysAddr();
#ifdef OHCI_DEBUGPRINTS_ENDPOINTS
    {
        std::stringstream str{};
        str << std::hex << "Adding transfer c=" << TD->TdControl
        << " buf=" << TD->CurrentBufferPointer << "/" << size
        << " next=" << TD->NextTD << "\n";
        get_klogger() << str.str().c_str();
    }
#endif
    ohciRef.transfersInProgress.push_back(tail);
    tail = newtd;
    edPtr->Pointer()->TailP = newtd->PhysAddr();
    if (endpointType == usb_endpoint_type::CONTROL) {
        ohciRef.ohciRegisters->HcCommandStatus = OHCI_CMD_CLF;
    } else if (endpointType == usb_endpoint_type::BULK) {
        ohciRef.ohciRegisters->HcCommandStatus = OHCI_CMD_BLF;
    }
    std::shared_ptr<usb_transfer> ref{filled};
    return ref;
}

std::shared_ptr<usb_transfer>
ohci_endpoint::CreateTransfer(bool commitTransaction, void *data, uint32_t size, usb_transfer_direction direction, bool bufferRounding,
                              uint16_t delayInterrupt, int8_t dataToggle) {
    std::shared_ptr<usb_buffer> buffer = Alloc(size);
    memcpy(buffer->Pointer(), data, size);
    std::function<void (ohci_transfer &)> applyFunc = [] (ohci_transfer &) {};
    return CreateTransfer(buffer, size, direction, bufferRounding, delayInterrupt, dataToggle, applyFunc);
}

std::shared_ptr<usb_transfer>
ohci_endpoint::CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction, bool bufferRounding,
                              uint16_t delayInterrupt, int8_t dataToggle) {
    std::shared_ptr<usb_buffer> buffer{};
    if (size > 0) {
        buffer = Alloc(size);
    }
    std::function<void (ohci_transfer &)> applyFunc = [] (ohci_transfer &) {};
    return CreateTransfer(buffer, size, direction, bufferRounding, delayInterrupt, dataToggle, applyFunc);
}

std::shared_ptr<usb_transfer>
ohci_endpoint::CreateTransfer(bool commitTransaction, uint32_t size, usb_transfer_direction direction, std::function<void()> doneCall,
                              bool bufferRounding, uint16_t delayInterrupt, int8_t dataToggle) {
    std::shared_ptr<usb_buffer> buffer{};
    if (size > 0) {
        buffer = Alloc(size);
    }
    std::function<void (ohci_transfer &)> applyFunc = [doneCall] (ohci_transfer &transfer) {
        transfer.SetDoneCall(doneCall);
    };
    return CreateTransfer(buffer, size, direction, bufferRounding, delayInterrupt, dataToggle, applyFunc);
}

std::shared_ptr<usb_transfer>
ohci_endpoint::CreateTransferWithLock(bool commitTransaction, const void *data, uint32_t size,
                                      usb_transfer_direction direction, std::function<void()> doneCall,
                                      bool bufferRounding, uint16_t delayInterrupt, int8_t dataToggle) {
    std::shared_ptr<usb_buffer> buffer{};
    if (size > 0) {
        buffer = Alloc(size);
        memcpy(buffer->Pointer(), data, size);
    }
    std::function<void (ohci_transfer &)> applyFunc = [doneCall] (ohci_transfer &transfer) {
        transfer.SetDoneCall(doneCall);
    };
    return CreateTransferWithLock(buffer, size, direction, bufferRounding, delayInterrupt, dataToggle, applyFunc);
}

std::shared_ptr<usb_transfer>
ohci_endpoint::CreateTransferWithLock(bool commitTransaction, uint32_t size, usb_transfer_direction direction, std::function<void()> doneCall,
                              bool bufferRounding, uint16_t delayInterrupt, int8_t dataToggle) {
    std::shared_ptr<usb_buffer> buffer{};
    if (size > 0) {
        buffer = Alloc(size);
    }
    std::function<void (ohci_transfer &)> applyFunc = [doneCall] (ohci_transfer &transfer) {
        transfer.SetDoneCall(doneCall);
    };
    return CreateTransferWithLock(buffer, size, direction, bufferRounding, delayInterrupt, dataToggle, applyFunc);
}

std::shared_ptr<usb_buffer> ohci_endpoint::Alloc(size_t size) {
    return ohciRef.Alloc(size);
}

void ohci_endpoint::SetSkip(bool skip) {
    if (skip) {
        edPtr->Pointer()->HcControl |= OHCI_ED_SKIP;
    } else {
        edPtr->Pointer()->HcControl &= ~OHCI_ED_SKIP;
    }
}

bool ohci_endpoint::ClearStall() {
    return false;
}

ohci_transfer::ohci_transfer(ohci &ohci, ohci_endpoint &endpoint) : usb_transfer(), transferPtr(ohci.xPool.Alloc()), buffer(), next(), endpoint(endpoint), waitCancelled(false), waitCancelledAndWaitedCycle(false) {
#ifdef OHCI_DEBUGPRINTS_ENDPOINTS
    std::stringstream str{};
    str << std::hex << "Transfer descriptor at " << transferPtr->Phys() << "\n";
    get_klogger() << str.str().c_str();
#endif
}

void ohci_transfer::SetDone() {
    usb_transfer::SetDone();
    if (!waitCancelled) {
        if (endpoint.head == this) {
            endpoint.head = this->next;
        } else {
            std::shared_ptr<ohci_transfer> transfer = endpoint.head;
            while (transfer->next) {
                if (transfer->next == this) {
                    transfer->next = this->next;
                    return;
                }
                transfer = transfer->next;
            }
        }
        this->next = {};
    }
}

usb_transfer_status ohci_transfer::GetStatus() {
    uint32_t status = transferPtr->Pointer()->TdControl;
    status = status >> 28;
    switch (status) {
        case 0:
            return usb_transfer_status::NO_ERROR;
        case 1:
            return usb_transfer_status::CRC;
        case 2:
            return usb_transfer_status::BIT_STUFFING;
        case 3:
            return usb_transfer_status::DATA_TOGGLE_MISMATCH;
        case 4:
            return usb_transfer_status::STALL;
        case 5:
            return usb_transfer_status::DEVICE_NOT_RESPONDING;
        case 6:
            return usb_transfer_status::PID_CHECK_FAILURE;
        case 7:
            return usb_transfer_status::UNEXPECTED_PID;
        case 8:
            return usb_transfer_status::DATA_OVERRUN;
        case 9:
            return usb_transfer_status::DATA_UNDERRUN;
        case 12:
            return usb_transfer_status::BUFFER_OVERRUN;
        case 13:
            return usb_transfer_status::BUFFER_UNDERRUN;
        case 14:
        case 15:
            return usb_transfer_status::NOT_ACCESSED;
        default:
            return usb_transfer_status::UNKNOWN_ERROR;
    }
}

ohci_transfer::~ohci_transfer() {
#ifdef OHCI_DEBUGPRINTS_ENDPOINTS
    auto *TD = transferPtr->Pointer();
    {
        std::stringstream str{};
        str << std::hex << "Destroying transfer c=" << TD->TdControl
            << " buf=" << TD->CurrentBufferPointer << "/" << TD->BufferEnd
            << " next=" << TD->NextTD << "\n";
        get_klogger() << str.str().c_str();
    }
#endif
}

ohci_endpoint_cleanup::~ohci_endpoint_cleanup() {
#ifdef OHCI_DEBUGPRINTS_ENDPOINTS
    {
        uint32_t ctrlHeadEd = ohciRef.ohciRegisters->HcControlHeadED;
        uint32_t ctrlCurrentEd = ohciRef.ohciRegisters->HcControlCurrentED;
        std::stringstream str{};
        str << std::hex << "ctrl-h=" << ctrlHeadEd << " cur-ctrl="<<ctrlCurrentEd<<"\n";
        get_klogger() << str.str().c_str();
    }
#endif
}
