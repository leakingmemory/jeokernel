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

#define XHCI_STATUS_HALTED      1
#define XHCI_STATUS_NOT_READY   (1 << 11)

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

    numSlots = (uint8_t) (capabilities->hcsparams1 & 0xFF);
    {
        uint32_t config{opregs->config};
        config &= 0xFFFFFF00;
        config |= numSlots;
        opregs->config = config;
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
        msg << DeviceType() << (unsigned int) DeviceId() << ": hcsparams2=" << std::hex << capabilities->hcsparams2
            << " max-scratchpad=" << maxScratchpadBuffers << "\n";
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
    opregs->commandRingControl = resources->CommandRingPhys();

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

    {
        std::stringstream msg;
        msg << DeviceType() << (unsigned int) DeviceId() << ": USB3 controller hccparams1=" << std::hex << capabilities->hccparams1 << "\n";
        get_klogger() << msg.str().c_str();
    }
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
