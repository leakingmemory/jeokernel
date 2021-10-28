//
// Created by sigsegv on 28.05.2021.
//

#include <sstream>
#include <klogger.h>
#include <chrono>
#include <thread>
#include "xhci.h"

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
    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": USB3 controller hccparams1=" << std::hex << capabilities->hccparams1 << "\n";
        get_klogger() << msg.str().c_str();
    }
}
