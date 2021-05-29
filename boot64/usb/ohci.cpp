//
// Created by sigsegv on 28.05.2021.
//

#include <memory>
#include <sstream>
#include <klogger.h>
#include <core/vmem.h>
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
    ohciRegisters = (ohci_registers *) mapped_registers_vm->pointer();
    if ((ohciRegisters->HcControl & OHCI_CTRL_IR) != 0){
        get_klogger() << "OHCI SMM active\n";
    } else if ((ohciRegisters->HcControl & OHCI_CTRL_HCFS_MASK) == OHCI_CTRL_HCFS_OPERATIONAL) {
        get_klogger() << "OHCI BIOS/Firmware driver operational\n";
    } else if ((ohciRegisters->HcControl & OHCI_CTRL_HCFS_MASK) != OHCI_CTRL_HCFS_RESET) {
        get_klogger() << "OHCI BIOS/Firmware driver not operational\n";
    } else {
        get_klogger() << "OHCI cold start\n";
    }
    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": USB controller, memory registers 0x" << std::hex << addr << " size " << std::dec << size << "\n";
        get_klogger() << msg.str().c_str();
    }
}
