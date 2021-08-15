//
// Created by sigsegv on 28.05.2021.
//

#include <memory>
#include <sstream>
#include <klogger.h>
#include <core/vmem.h>
#include <thread>
#include "ohci.h"

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

    uint32_t interval = ohciRegisters->HcFmInterval & OHCI_INTERVAL_FI_MASK;

    ohciRegisters->HcInterruptDisable = OHCI_INT_MASK;

    ohciRegisters->HcHCCA = hcca.Addr();

    ohciRegisters->HcControlHeadED = hcca.Addr(&(hcca.ControlED()));

    ohciRegisters->HcBulkHeadED = hcca.Addr(&(hcca.BulkED()));

    {
        uint32_t ctrl = ohciRegisters->HcControl;
        ctrl &= ~OHCI_CTRL_CBSR_MASK;
        ctrl |= OHCI_CTRL_CBSR_1_4 | OHCI_CTRL_PLE | OHCI_CTRL_IE | OHCI_CTRL_CLE | OHCI_CTRL_BLE;
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
            pci->AddIrqHandler(*irq, []() {
                get_klogger() << "ohci int\n";
                return true;
            });
        }
    }

    ohciRegisters->HcInterruptEnable = OHCI_INT_SCHEDULING_OVERRUN | OHCI_INT_SCHEDULING_WRITE_DONE_HEAD
            | OHCI_INT_SCHEDULING_START_OF_FRAME | OHCI_INT_SCHEDULING_RESUME_DETECTED
            | OHCI_INT_SCHEDULING_UNRECOVERABLE_ERR | OHCI_INT_SCHEDULING_FRAME_NUM_OVERFLOW
            | OHCI_INT_SCHEDULING_ROOT_HUB_STATUS_CH | OHCI_INT_MASTER_INTERRUPT;

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
