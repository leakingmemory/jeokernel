//
// Created by sigsegv on 28.05.2021.
//

#include <sstream>
#include <klogger.h>
#include <cpuio.h>
#include "ehci.h"



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
        caps = (ehci_capabilities *) mapped_registers_vm->pointer();
        regs = caps->opregs();
    }
    {
        uint8_t eecp1_off = (uint8_t) ((caps->hccparams & 0x0000FF00) >> 8);
        if (eecp1_off >= 0x40) {
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
                            eecp->refresh();
                        } while ((eecp->value & EHCI_LEGACY_OWNED) == 0 || (eecp->value & EHCI_LEGACY_BIOS) != 0);
                        {
                            std::stringstream msg;
                            msg << DeviceType() << (unsigned int) DeviceId() << ": Disabled legacy support\n";
                            get_klogger() << msg.str().c_str();
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
    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": USB2 controller caps 0x" << std::hex << caps->hccparams << "\n";
        get_klogger() << msg.str().c_str();
    }
}
