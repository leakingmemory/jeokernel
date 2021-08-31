//
// Created by sigsegv on 28.05.2021.
//

#include <memory>
#include <sstream>
#include <klogger.h>
#include <core/vmem.h>
#include <thread>
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

void ohci::init() {
    uint64_t addr{0};
    uint64_t size{0};
    bool prefetch{false};
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
        uint64_t physaddr{addr};
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

    // Some drivers do a host controller reset here.

    uint32_t interval = ohciRegisters->HcFmInterval & OHCI_INTERVAL_FI_MASK;

    if (!ResetHostController()) {
        return;
    }

    ohciRegisters->HcInterruptDisable = OHCI_INT_MASK;

    ohciRegisters->HcHCCA = hcca.Addr();

    ohciRegisters->HcControlHeadED = hcca.Addr(&(hcca.ControlED()));

    ohciRegisters->HcBulkHeadED = hcca.Addr(&(hcca.BulkED()));

    {
        uint32_t ctrl = ohciRegisters->HcControl;
        ctrl &= ~OHCI_CTRL_CBSR_MASK;
        ctrl |= OHCI_CTRL_CBSR_1_4 | OHCI_CTRL_PLE | OHCI_CTRL_IE | OHCI_CTRL_CLE | OHCI_CTRL_BLE;
        // Unsuspend controller after HC reset:
        ctrl |= OHCI_CTRL_HCFS_OPERATIONAL;
        ohciRegisters->HcControl = ctrl;
    }

    {
        uint32_t max_full_speed_packet = (interval - 210) * 6 / 7;
        /* Flip the FIT-bit */
        uint32_t fmInterval = (ohciRegisters->HcFmInterval & OHCI_INTERVAL_FIT) ^ OHCI_INTERVAL_FIT;
        fmInterval |= (max_full_speed_packet << 16) | interval;
        ohciRegisters->HcFmInterval = fmInterval;
    }

    ohciRegisters->HcPeriodicStart = (ohciRegisters->HcPeriodicStart & 0xFFFFC000) | (interval * 9 / 10);

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

    ohciRegisters->HcInterruptEnable = OHCI_INT_SCHEDULING_OVERRUN | OHCI_INT_SCHEDULING_WRITE_DONE_HEAD
            | OHCI_INT_SCHEDULING_START_OF_FRAME | OHCI_INT_SCHEDULING_RESUME_DETECTED
            | OHCI_INT_SCHEDULING_UNRECOVERABLE_ERR | OHCI_INT_SCHEDULING_FRAME_NUM_OVERFLOW
            | OHCI_INT_SCHEDULING_ROOT_HUB_STATUS_CH | OHCI_INT_MASTER_INTERRUPT;

    /*
     * Clear interrupt status flags to accept new interrupts.
     */
    ohciRegisters->HcInterruptStatus = OHCI_INT_SCHEDULING_OVERRUN | OHCI_INT_SCHEDULING_WRITE_DONE_HEAD
                                       | OHCI_INT_SCHEDULING_START_OF_FRAME | OHCI_INT_SCHEDULING_RESUME_DETECTED
                                       | OHCI_INT_SCHEDULING_UNRECOVERABLE_ERR | OHCI_INT_SCHEDULING_FRAME_NUM_OVERFLOW
                                       | OHCI_INT_SCHEDULING_ROOT_HUB_STATUS_CH;

    ohciRegisters->HcRhStatus = OHCI_HC_LPSC;

    {
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(70ms);
    }

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

    Run();
}

bool ohci::irq() {
    uint32_t IrqClear{0};
    uint32_t IrqStatus{ohciRegisters->HcInterruptStatus};
    if ((IrqStatus & OHCI_INT_SCHEDULING_OVERRUN) != 0) {
        IrqClear |= OHCI_INT_SCHEDULING_OVERRUN;
        get_klogger() << "ohci scheduling overrun\n";
    }
    if ((IrqStatus & OHCI_INT_SCHEDULING_WRITE_DONE_HEAD) != 0) {
        IrqClear |= OHCI_INT_SCHEDULING_WRITE_DONE_HEAD;
        get_klogger() << "ohci write back done head\n";
    }
    if ((IrqStatus & OHCI_INT_SCHEDULING_START_OF_FRAME) != 0) {
        IrqClear |= OHCI_INT_SCHEDULING_START_OF_FRAME;
        StartOfFrame();
    }
    if ((IrqStatus & OHCI_INT_SCHEDULING_RESUME_DETECTED) != 0) {
        IrqClear |= OHCI_INT_SCHEDULING_RESUME_DETECTED;
        get_klogger() << "ohci resume detected\n";
    }
    if ((IrqStatus & OHCI_INT_SCHEDULING_UNRECOVERABLE_ERR) != 0) {
        IrqClear |= OHCI_INT_SCHEDULING_UNRECOVERABLE_ERR;
        get_klogger() << "ohci unrecoverable error\n";
    }
    if ((IrqStatus & OHCI_INT_SCHEDULING_FRAME_NUM_OVERFLOW) != 0) {
        IrqClear |= OHCI_INT_SCHEDULING_FRAME_NUM_OVERFLOW;
        get_klogger() << "ohci frame number overflow\n";
    }
    if ((IrqStatus & OHCI_INT_SCHEDULING_ROOT_HUB_STATUS_CH) != 0) {
        IrqClear |= OHCI_INT_SCHEDULING_ROOT_HUB_STATUS_CH;
        get_klogger() << "ohci root hub status change\n";
    }
    ohciRegisters->HcInterruptStatus = IrqClear;
    return IrqClear != 0;
}

void ohci::StartOfFrame() {
    if (!StartOfFrameReceived) {
        get_klogger() << "OHCI: First start of frame\n";
    }
    StartOfFrameReceived = true;
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
    ohciRegisters->PortStatus[port] = OHCI_PORT_STATUS_CCS;
}

void ohci::SwitchPortOn(int port) {
    ohciRegisters->PortStatus[port] = OHCI_PORT_STATUS_PES;
}

void ohci::ClearStatusChange(int port, uint32_t statuses) {
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
            using namespace std::literals::chrono_literals;
            std::this_thread::sleep_for(10us);
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

OhciHcca::OhciHcca() : page(sizeof(*hcca)), hcca((typeof(hcca)) page.Pointer()), hcca_ed_ptrs() {
    for (int i = 0; i < 0x3F; i++) {
        hcca_ed_ptrs[i].ed = &(hcca->hcca_with_eds.eds[i]);
        hcca->hcca_with_eds.eds[i].HcControl = 0;
        hcca->hcca_with_eds.eds[i].HeadP = 0;
        hcca->hcca_with_eds.eds[i].TailP = 0;
    }
    for (int i = 0; i < 0x20; i++) {
        int j = (i >> 1) + 0x20;
        hcca->hcca_with_eds.eds[i].NextED = page.PhysAddr() + hcca->hcca_with_eds.RelativeAddr(&(hcca->hcca_with_eds.eds[j]));
    }
    for (int i = 0; i < 0x10; i++) {
        int j = (i >> 1) + 0x30;
        hcca->hcca_with_eds.eds[0x20 + i].NextED = page.PhysAddr() + hcca->hcca_with_eds.RelativeAddr(&(hcca->hcca_with_eds.eds[j]));
    }
    for (int i = 0; i < 8; i++) {
        int j = (i >> 1) + 0x38;
        hcca->hcca_with_eds.eds[0x30 + i].NextED = page.PhysAddr() + hcca->hcca_with_eds.RelativeAddr(&(hcca->hcca_with_eds.eds[j]));
    }
    for (int i = 0; i < 4; i++) {
        int j = (i >> 1) + 0x3C;
        hcca->hcca_with_eds.eds[0x38 + i].NextED = page.PhysAddr() + hcca->hcca_with_eds.RelativeAddr(&(hcca->hcca_with_eds.eds[j]));
    }
    for (int i = 0; i < 2; i++) {
        hcca->hcca_with_eds.eds[0x3C + i].NextED = page.PhysAddr() + hcca->hcca_with_eds.RelativeAddr(&(hcca->hcca_with_eds.eds[0x3E]));
    }
    hcca->hcca_with_eds.eds[0x3E].NextED = 0;

    hcca->hc_control_ed.NextED = 0;
    hcca->hc_control_ed.TailP = 0;
    hcca->hc_control_ed.HeadP = 0;
    hcca->hc_control_ed.HcControl = 0;

    hcca->hc_bulk_ed.NextED = 0;
    hcca->hc_bulk_ed.TailP = 0;
    hcca->hc_bulk_ed.HeadP = 0;
    hcca->hc_bulk_ed.HcControl = 0;
}
