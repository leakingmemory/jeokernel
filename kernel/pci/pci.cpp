//
// Created by sigsegv on 5/20/21.
//

#include "pci.h"
#include "../ApStartup.h"
#include <cpuio.h>
#include <thread>
#include <mutex>
#include <core/cpu_mpfp.h>
#include <devices/drivers.h>
#include <sstream>
#include <acpi/acpica.h>

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
    {
        critical_section cli{}; // No context switches between
        outportl(PCI_CONFIG_ADDRESS, addr);
        return inportl(PCI_CONFIG_DATA);
    }
}

uint32_t read8_pci_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    if (slot > 0x1F || func > 7) {
        return 0xFFFFFFFF;
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
    {
        critical_section cli{}; // No context switches between
        outportl(PCI_CONFIG_ADDRESS, addr);
        return inportb(PCI_CONFIG_DATA);
    }
}

void write_pci_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    if (slot > 0x1F || func > 7) {
        asm("ud2");
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
    {
        critical_section cli{}; // No context switches between
        outportl(PCI_CONFIG_ADDRESS, addr);
        outportl(PCI_CONFIG_DATA, value);
    }
}

void write8_pci_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t value) {
    if (slot > 0x1F || func > 7) {
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
    {
        critical_section cli{}; // No context switches between
        outportl(PCI_CONFIG_ADDRESS, addr);
        outportb(PCI_CONFIG_DATA, value);
    }
}

pci::pci(uint16_t bus, uint16_t br_bus, uint16_t br_slot, uint16_t br_func) : Bus("pci"), bus(bus), br_bus(br_bus), br_slot(br_slot), br_func(br_func), irqr(), SourceMap(), ioapic(), lapic() {
    ApStartup *ap = GetApStartup();
    ioapic = ap->GetIoapic();
    lapic = ap->GetLocalApic();
    if (!get_acpica().find_pci_bridges([this] (void *handle, ACPI_DEVICE_INFO *dev_info) {
        ACPI_PCI_ID pciId;
        if (get_acpica().determine_pci_id(pciId, dev_info, handle)) {
            if (this->br_bus != pciId.Bus || this->br_slot != pciId.Device || this->br_func != pciId.Function) {
                return;
            }
        } else {
            return;
        }
        get_klogger() << "PCI bus bridge resources (";
        if (dev_info->UniqueId.Length > 0) {
            get_klogger() << dev_info->UniqueId.String;
        }
        get_klogger() << ") ";
        get_klogger() << pciId.Segment << ":" << pciId.Bus << ":" << pciId.Device << ":" << pciId.Function;
        get_klogger() << ": \n";
        if (!get_acpica().find_resources(handle, [] (ACPI_RESOURCE *resource) {
            get_klogger() << " - ACPICA resource " << resource->Type << "\n";
        })) {
            get_klogger() << "error: read acpi PCI bridge resources failed\n";
        }
        ReadIrqRouting(handle);
        for (auto &irqent : SourceMap) {
            InstallIrqHandler(std::get<1>(irqent));
        }
        {
            std::vector<uint8_t> irqns{};
            for (auto &irq : irqr) {
                if (irq.Source.size() == 0) {
                    bool hit{false};
                    for (uint8_t n : irqns) {
                        if (n == irq.SourceIndex) {
                            hit = true;
                            break;
                        }
                    }
                    if (!hit && GetIrq(irq.SourceIndex) == nullptr) {
                        irqns.push_back(irq.SourceIndex);
                    }
                }
            }
            for (uint8_t irq : irqns) {
                IRQLink link{
                    .ProducerConsumer = 1,
                    .Triggering = 1, // level
                    .Polarity = 1, // low
                    .Shareable = 1,
                    .WakeCapable = 0,
                    .InterruptCount = 1,
                    .ResourceSource = "",
                    .Interrupts = {}
                };
                link.Interrupts.push_back(irq);
                InstallIrqHandler(link);
            }
        }
    })) {
        get_klogger() << "error: read acpi PCI bridges failed\n";
    }
}

void pci::ProbeDevices() {
    Bus::ProbeDevices();
    for (uint8_t i = 0; i < 0x20; i++) {
        std::optional<PciDeviceInformation> opt = probeDevice(i);
        if (opt) {
            if (br_bus != bus || br_slot != i || br_func != 0) {
                if (!get_drivers().probe(*this, *opt)) {
                    get_klogger() << "PCI device " << opt->vendor_id << ":" << opt->device_id << " CL "
                                  << opt->device_class
                                  << ":" << opt->device_subclass << ":" << opt->prog_if << " rev " << opt->revision_id
                                  << " header  " << opt->header_type << (opt->multifunction ? " MFD\n" : "\n");
                }
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

void pci::ReadIrqRouting(void *acpi_handle) {
    auto &acpica = get_acpica();
    bool using_source{false};
    irqr = acpica.get_irq_routing(acpi_handle);
    for (auto &rt : irqr) {
        if (!using_source && rt.Source.size() > 0) {
            using_source = true;
        }
        get_klogger() << "IRQr " << rt.Address << " P=" << rt.Pin << " Si=" << rt.SourceIndex << " S="
                      << rt.Source.c_str() <<"\n";
    }
    if (using_source) {
        get_klogger() << "Namespace walk:\n";
        acpica.walk_namespace([this, &acpica](std::string path, uint32_t objectType, void *handle) {
            bool found{false};
            for (auto &rt : irqr) {
                if (rt.Source.size() > 0 && rt.Source == path) {
                    get_klogger() << " - Found " << path.c_str() << " type " << objectType << "\n";
                    auto irq = acpica.get_extended_irq(handle);
                    if (irq) {
                        get_klogger() << "   - IRQ# " << (uint8_t) (*irq)[0];
                        for (int i = 1; i < irq->size(); i++) {
                            get_klogger() << ", " << (uint8_t) (*irq)[i];
                        }
                        get_klogger() << " PC=" << irq->ProducerConsumer << " Trig=" << irq->Triggering
                        << " Pol=" << irq->Polarity << " Sh=" << irq->Shareable << " Wak="
                        << irq->WakeCapable << " " << irq->ResourceSource.c_str() << "\n";

                        SourceMap.push_back(std::make_tuple<std::string,IRQLink>(path, *irq));
                    }
                    found = true;
                    break;
                }
            }
        });
    }
}

pci_irq *pci::GetIrq(uint8_t irqn) {
    for (auto *irq : irqs) {
        if (irq->Irq() == irqn) {
            return irq;
        }
    }
    return nullptr;
}

void pci::InstallIrqHandler(const IRQLink &link) {
    for (int i = 0; i < link.Interrupts.size(); i++) {
        uint8_t irqn = (link.Interrupts.data())[i];
        if (GetIrq(irqn) == nullptr) {
            irqs.push_back(new pci_irq(*this, link, i));
            std::stringstream msg{};
            msg << "Installed IRQ " << irqn << "\n";
            get_klogger() << msg.str().c_str();
        } else {
            //std::stringstream msg{};
            //msg << "Info: Duplicate IRQ def for " << irqn << "\n";
            //get_klogger() << msg.str().c_str();
        }
    }
}

const IRQLink *pci::GetLink(const std::string &name) {
    for (auto &tuple : SourceMap) {
        std::string SourceName{std::get<0>(tuple)};
        if (name == SourceName) {
            return &(std::get<1>(tuple));
        }
    }
    return nullptr;
}

const PciIRQRouting *pci::GetRouting(const PciDeviceInformation &deviceInformation, uint8_t pin_03) {
    if (deviceInformation.bus != bus) {
        return nullptr;
    }
    PciIRQRouting *withoutFunc{nullptr};
    for (auto &irq : irqr) {
        if (irq.Slot == deviceInformation.slot) {
            if (irq.Func == deviceInformation.func && irq.Pin == pin_03) {
                return &irq;
            } else if (irq.Func == 0xFFFF && irq.Pin == pin_03) {
                withoutFunc = &irq;
            }
        }
    }
    return withoutFunc;
}

std::optional<uint8_t> pci::GetIrqNumber(const PciDeviceInformation &deviceInformation, uint8_t pin_03) {
    {
        const PciIRQRouting *IrqRouting{GetRouting(deviceInformation, pin_03)};
        if (IrqRouting != nullptr) {
            uint8_t irq{0};
            if (IrqRouting->Source.size() > 0) {
                const IRQLink *Source = GetLink(IrqRouting->Source);
                irq = (Source->Interrupts.data())[IrqRouting->SourceIndex];
            } else {
                irq = IrqRouting->SourceIndex;
            }
            return irq;
        }
    }
    return {};
}

bool pci::AddIrqHandler(uint8_t irq, std::function<bool()> h) {
    {
        pci_irq *PciIrq = GetIrq(irq);
        if (PciIrq != nullptr) {
            PciIrq->add_handler(h);
            return true;
        }
    }
    return false;
}

pci::~pci() {
    for (auto *irq : irqs) {
        delete irq;
    }
    irqs.clear();
}

void init_pci() {
    pci_bus_mtx = new std::mutex;
}

void detect_root_pcis() {
#ifdef USE_APIC_FOR_PCI_BUSES
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
#else
    pci *pcibus = new pci(0, 0, 0, 0);
    devices().add(*pcibus);
    std::stringstream msg{};
    msg << pcibus->DeviceType() << (unsigned int) pcibus->DeviceId() << ": root bus\n";
    get_klogger() << msg.str().c_str();
    pcibus->ProbeDevices();
#endif
}

PciDeviceInformation *PciDeviceInformation::GetPciInformation() {
    return this;
}

PciBaseAddressRegister PciDeviceInformation::readBaseAddressRegister(uint8_t index) {
    uint8_t offset{index};
    offset = (offset << 2) + 0x10;
    PciBaseAddressRegister bar{
        .dev_info = *this,
        .value = read_pci_config(this->bus, this->slot, this->func, offset),
        .offset = offset
    };
    return bar;
}

uint32_t PciDeviceInformation::readStatusRegister() {
    uint8_t offset{(1 << 2)};
    return read_pci_config(this->bus, this->slot, this->func, offset);
}

PciRegisterF PciDeviceInformation::readRegF() {
    uint8_t offset{(0xF << 2)};
    return read_pci_config(this->bus, this->slot, this->func, offset);
}

uint64_t PciBaseAddressRegister::addr64() {
    uint64_t addr = (uint64_t) read_pci_config(dev_info.bus, dev_info.slot, dev_info.func, offset + 4);
    addr = addr << 32;
    addr += (uint64_t) (value & 0xFFFFFFF0);
    return addr;
}

uint32_t PciBaseAddressRegister::read32() {
    return read_pci_config(dev_info.bus, dev_info.slot, dev_info.func, offset);
}

void PciBaseAddressRegister::write32(uint32_t value) {
    write_pci_config(dev_info.bus, dev_info.slot, dev_info.func, offset, value);
}

uint32_t PciBaseAddressRegister::memory_size() {
    write32(0xFFFFFFFF);
    uint32_t size = ~(read32() & 0xFFFFFFF0);
    write32(value);
    if (read32() != value) {
        asm("ud2");
    }
    return size + 1;
}
