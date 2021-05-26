//
// Created by sigsegv on 5/20/21.
//

#include "pci.h"
#include <cpuio.h>
#include <thread>
#include <mutex>
#include <core/cpu_mpfp.h>
#include <devices/drivers.h>

#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

static std::mutex *pci_bus_mtx = nullptr;

uint32_t read_pci_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    if (slot > 0x1F || func > 7) {
        return 0xFFFFFFFF;
    }
    if ((offset & 3) != 0) {
        asm("ud2");
    }
    uint32_t addr = bus;
    addr |= 0x8000;
    addr = addr << 5;
    addr |= slot;
    addr = addr << 3;
    addr |= func;
    addr = addr << 8;
    addr |= offset;
    std::lock_guard lock(*pci_bus_mtx);
    outportl(PCI_CONFIG_ADDRESS, addr);
    return inportl(PCI_CONFIG_DATA);
}

void pci::ProbeDevices() {
    Bus::ProbeDevices();
    for (uint8_t i = 0; i < 0x20; i++) {
        std::optional<PciDeviceInformation> opt = probeDevice(i);
        if (opt) {
            if (!get_drivers().probe(*this, *opt)) {
                get_klogger() << "PCI device " << opt->vendor_id << ":" << opt->device_id << " CL " << opt->device_class
                              << ":" << opt->device_subclass << ":" << opt->prog_if << " rev " << opt->revision_id
                              << " header  " << opt->header_type << (opt->multifunction ? " MFD\n" : "\n");
            }
            if (opt->multifunction) {
                for (uint8_t func = 1; func < 8; func++) {
                    std::optional<PciDeviceInformation> mfd = probeDevice(i, func);
                    if (mfd && !get_drivers().probe(*this, *mfd)) {
                        get_klogger() << "PCI device " << mfd->vendor_id << ":" << mfd->device_id << " CL " << mfd->device_class << ":" << mfd->device_subclass << ":" << mfd->prog_if  << " rev " << mfd->revision_id << " header  " << mfd->header_type << (mfd->multifunction ? " MFD\n" : "\n");
                    }
                }
            }
        }
    }
}

std::optional<PciDeviceInformation> pci::probeDevice(uint8_t addr, uint8_t func) {
    uint32_t reg0 = read_pci_config(bus, addr, func, 0);
    uint16_t device_id = (uint16_t) (reg0 >> 16);
    uint16_t vendor_id = (uint16_t) (reg0 & 0xFFFF);
    if (vendor_id == 0xFFFF) {
        return {};
    }
    uint32_t reg2 = read_pci_config(bus, addr, func, 8);
    uint8_t class_id = (uint8_t) (reg2 >> 24);
    uint8_t subclass_id = (uint8_t) ((reg2 >> 16) & 0xFF);
    uint8_t prog_if = (uint8_t) ((reg2 >> 8) & 0xFF);
    uint8_t revision_id = (uint8_t) (reg2 & 0xFF);
    uint32_t reg3 = read_pci_config(bus, addr, func, 0xC);
    uint8_t mfd = (uint8_t) ((reg3 >> 16) & 0x80);
    uint8_t header_type = (uint8_t) ((reg3 >> 16) & 0x7F);
    PciDeviceInformation info{};
    info.vendor_id = vendor_id;
    info.device_id = device_id;
    info.device_subclass = subclass_id;
    info.device_class = class_id;
    info.prog_if = prog_if;
    info.bus = bus;
    info.slot = addr;
    info.func = func;
    info.revision_id = revision_id;
    info.multifunction = mfd == 0x80;
    info.header_type = header_type;
    return {info};
}

void detect_root_pcis() {
    pci_bus_mtx = new std::mutex;
    cpu_mpfp *mpfp = get_mpfp();
    if (mpfp != nullptr) {
        for (int i = 0; i < mpfp->get_num_bus(); i++) {
            const mp_bus_entry &bus_entry = mpfp->get_bus(i);
            if (
                    bus_entry.bus_type[0] == 'P' &&
                    bus_entry.bus_type[1] == 'C' &&
                    bus_entry.bus_type[2] == 'I'
                    ) {
                pci *pcibus = new pci(bus_entry.bus_id);
                devices().add(*pcibus);
                get_klogger() << "PCI bus=" << bus_entry.bus_id << "\n";
                pcibus->ProbeDevices();
            }
        }
    }
}

PciDeviceInformation *PciDeviceInformation::GetPciInformation() {
    return this;
}
