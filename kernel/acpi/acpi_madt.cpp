//
// Created by sigsegv on 12/24/21.
//

#include <cstdint>
#include <acpi/acpi_madt.h>
#include <string>
#include <sstream>
#include <klogger.h>

struct acpi_madt_table;

struct acpi_table_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemid[6];
    char oemtableid[8];
    uint32_t oemrev;
    uint32_t creatorid;
    uint32_t creatorrev;

    std::string Signature() {
        std::string str{&(signature[0]), 4};
        return str;
    }
    uint32_t RemainingLength() {
        return length > sizeof(*this) ? length - sizeof(*this) : 0;
    }
    const acpi_madt_table *MADT(uint32_t &remaining) const;
};

static_assert(sizeof(acpi_table_header) == 0x24);

struct acpi_madt_entry;

struct acpi_madt_table {
    uint32_t LocalApicAddress;
    uint32_t Flags;

    const acpi_madt_entry *First(uint32_t &remaining) const;
    void Visit(uint32_t remainingLength, acpi_madt_visitor &visitor) const;
};

const acpi_madt_table *acpi_table_header::MADT(uint32_t &remaining) const {
    uint8_t *ptr = (uint8_t *) (void *) this;
    ptr += sizeof(*this);
    if (remaining >= sizeof(acpi_madt_table)) {
        remaining -= sizeof(acpi_madt_table);
        return (acpi_madt_table *) (void *) ptr;
    } else {
        return nullptr;
    }
}

const acpi_madt_entry *acpi_madt_table::First(uint32_t &remaining) const {
    if (remaining < sizeof(acpi_madt_entry)) {
        return nullptr;
    }
    uint8_t *ptr = (uint8_t *) (void *) this;
    ptr += sizeof(*this);
    auto *entry = (acpi_madt_entry *) (void *) ptr;
    if (entry->RecordLength >= sizeof(acpi_madt_entry) && remaining >= entry->RecordLength) {
        remaining -= entry->RecordLength;
        return entry;
    } else {
        return nullptr;
    }
}

void acpi_madt_table::Visit(uint32_t remainingLength, acpi_madt_visitor &visitor) const {
    auto *entry = First(remainingLength);
    while (entry != nullptr) {
        switch (entry->EntryType) {
            case 0:
                visitor.Visit(*(entry->Processor()));
                break;
            case 1:
                visitor.Visit(*(entry->IoApic()));
                break;
            case 2:
                visitor.Visit(*(entry->IoApicInterruptSourceOverride()));
                break;
            case 3:
                visitor.Visit(*(entry->IoApicNmiSource()));
                break;
            case 4:
                visitor.Visit(*(entry->LapicNmi()));
                break;
            case 5:
                visitor.Visit(*(entry->LapicAddressOverride()));
                break;
            case 9:
                visitor.Visit(*(entry->ProcessorLocalX2Apic()));
                break;
        }
        entry = entry->Next(remainingLength);
    }
}

const acpi_madt_entry *acpi_madt_entry::Next(uint32_t &remaining) const {
    if (remaining < sizeof(acpi_madt_entry)) {
        return nullptr;
    }
    uint8_t *ptr = (uint8_t *) (void *) this;
    ptr += RecordLength;
    auto *entry = (acpi_madt_entry *) (void *) ptr;
    if (entry->RecordLength >= sizeof(acpi_madt_entry) && remaining >= entry->RecordLength) {
        remaining -= entry->RecordLength;
        return entry;
    } else {
        return nullptr;
    }
}

acpi_madt_info::acpi_madt_info(void *pointer) : ioapicAddrs() {
    acpi_table_header *madt_header = (acpi_table_header *) pointer;
    uint32_t remaining{madt_header->RemainingLength()};
    const acpi_madt_entry *entry{nullptr};
    {
        const acpi_madt_table *table = madt_header->MADT(remaining);
        if (table != nullptr) {
            table->Visit(remaining, *this);
        }
    }
}

void acpi_madt_info::Visit(const acpi_madt_processor &cpu) {
    if ((cpu.Flags & 1) != 0 || (cpu.Flags & 2) != 0) {
        localApicIds.push_back(cpu.ApicId);
    }
    std::stringstream str{};
    str << std::hex << "ACPI CPU " << cpu.AcpiProcessorId << " APIC-ID " << cpu.ApicId << " flags " << cpu.Flags << "\n";
    get_klogger() << str.str().c_str();
}

void acpi_madt_info::Visit(const acpi_madt_ioapic &ioapic) {
    ioapicAddrs.push_back(ioapic.IoApicAddress);
    std::stringstream str{};
    str << std::hex << "ACPI IO-APIC " << ioapic.IoApicId << " addr " << ioapic.IoApicAddress << " interrupt base " << ioapic.GlobalSystemInterruptBase << "\n";
    get_klogger() << str.str().c_str();
}

void acpi_madt_info::Visit(const acpi_madt_ioapic_interrupt_source_override &intr) {
    std::stringstream str{};
    str << std::hex << "ACPI IO-APIC irq source " << intr.IrqSource << " bus " << intr.BusSource << " flags " << intr.Flags
        << " global system int " << intr.GlobalSystemInterrupt << "\n";
    get_klogger() << str.str().c_str();
}

void acpi_madt_info::Visit(const acpi_madt_ioapic_nmi_source &nmi) {
    std::stringstream str{};
    str << std::hex << "ACPI IO-apic NMI " << nmi.NmiSource << " flags " << nmi.Flags << " global system int " << nmi.GlobalSystemInterrupt << "\n";
    get_klogger() << str.str().c_str();
}

void acpi_madt_info::Visit(const acpi_madt_lapic_nmi &nmi) {
    std::stringstream str{};
    str << std::hex << "ACPI L-APIC NMI processor " << nmi.AcpiProcessorId << " LINTn " << nmi.LINTn << " flags " << nmi.Flags << "\n";
    get_klogger() << str.str().c_str();
}

void acpi_madt_info::Visit(const acpi_madt_lapic_address_override &lapic) {
    std::stringstream str{};
    str << std::hex << "ACPI L-APIC override " << lapic.PhysAddr << "\n";
    get_klogger() << str.str().c_str();
}

void acpi_madt_info::Visit(const acpi_madt_processor_local_x2apic &x2apic) {
    std::stringstream str{};
    str << std::hex << "ACPI processor " << x2apic.AcpiId << " X2APIC ID " << x2apic.ProcessorX2ApicId << " flags " << x2apic.Flags << "\n";
    get_klogger() << str.str().c_str();
}

int acpi_madt_info::GetNumberOfIoapics() const {
    return ioapicAddrs.size();
}

uint64_t acpi_madt_info::GetIoapicAddr(int index) const {
    return ioapicAddrs.at(index);
}

int acpi_madt_info::GetNumberOfCpus() const {
    return localApicIds.size();
}

int acpi_madt_info::GetLocalApicId(int cpu) const {
    return localApicIds.at(cpu);
}
