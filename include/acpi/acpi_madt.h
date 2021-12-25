//
// Created by sigsegv on 12/24/21.
//

#ifndef JEOKERNEL_ACPI_MADT_H
#define JEOKERNEL_ACPI_MADT_H

#include <vector>
#include <core/apics_info.h>

struct acpi_madt_processor;
struct acpi_madt_ioapic;
struct acpi_madt_ioapic_interrupt_source_override;
struct acpi_madt_ioapic_nmi_source;
struct acpi_madt_lapic_nmi;
struct acpi_madt_lapic_address_override;
struct acpi_madt_processor_local_x2apic;

struct acpi_madt_entry {
    uint8_t EntryType;
    uint8_t RecordLength;

    const acpi_madt_processor *Processor() const {
        return (const acpi_madt_processor *) (const void *) this;
    }
    const acpi_madt_ioapic *IoApic() const {
        return (const acpi_madt_ioapic *) (const void *) this;
    }
    const acpi_madt_ioapic_interrupt_source_override *IoApicInterruptSourceOverride() const {
        return (const acpi_madt_ioapic_interrupt_source_override *) (const void *) this;
    }
    const acpi_madt_ioapic_nmi_source *IoApicNmiSource() const {
        return (const acpi_madt_ioapic_nmi_source *) (const void *) this;
    }
    const acpi_madt_lapic_nmi *LapicNmi() const {
        return (const acpi_madt_lapic_nmi *) (const void *) this;
    }
    const acpi_madt_lapic_address_override *LapicAddressOverride() const {
        return (const acpi_madt_lapic_address_override *) (const void *) this;
    }
    const acpi_madt_processor_local_x2apic *ProcessorLocalX2Apic() const {
        return (const acpi_madt_processor_local_x2apic *) (const void *) this;
    }

    const acpi_madt_entry *Next(uint32_t &remaining) const;
} __attribute__((__packed__));

struct acpi_madt_processor : acpi_madt_entry {
    uint8_t AcpiProcessorId;
    uint8_t ApicId;
    uint32_t Flags;
} __attribute__((__packed__));

struct acpi_madt_ioapic : acpi_madt_entry {
    uint8_t IoApicId;
    uint8_t Reserved;
    uint32_t IoApicAddress;
    uint32_t GlobalSystemInterruptBase;
} __attribute__((__packed__));

struct acpi_madt_ioapic_interrupt_source_override : acpi_madt_entry {
    uint8_t BusSource;
    uint8_t IrqSource;
    uint32_t GlobalSystemInterrupt;
    uint16_t Flags;
} __attribute__((__packed__));

struct acpi_madt_ioapic_nmi_source : acpi_madt_entry {
    uint8_t NmiSource;
    uint8_t Reserved;
    uint16_t Flags;
    uint32_t GlobalSystemInterrupt;
} __attribute__((__packed__));

struct acpi_madt_lapic_nmi : acpi_madt_entry {
    uint8_t AcpiProcessorId;
    uint16_t Flags;
    uint8_t LINTn;
} __attribute__((__packed__));

struct acpi_madt_lapic_address_override : acpi_madt_entry {
    uint16_t Reserved;
    uint64_t PhysAddr;
} __attribute__((__packed__));

struct acpi_madt_processor_local_x2apic : acpi_madt_entry {
    uint16_t Reserved;
    uint32_t ProcessorX2ApicId;
    uint32_t Flags;
    uint32_t AcpiId;
} __attribute__((__packed__));

class acpi_madt_visitor {
public:
    virtual void Visit(const acpi_madt_processor &) = 0;
    virtual void Visit(const acpi_madt_ioapic &) = 0;
    virtual void Visit(const acpi_madt_ioapic_interrupt_source_override &) = 0;
    virtual void Visit(const acpi_madt_ioapic_nmi_source &) = 0;
    virtual void Visit(const acpi_madt_lapic_nmi &) = 0;
    virtual void Visit(const acpi_madt_lapic_address_override &) = 0;
    virtual void Visit(const acpi_madt_processor_local_x2apic &) = 0;
};

class acpi_madt_info : private acpi_madt_visitor, public apics_info {
private:
    std::vector<uint64_t> ioapicAddrs;
    std::vector<int> localApicIds;
public:
    acpi_madt_info(void *);
    int GetNumberOfIoapics() const override;
    uint64_t GetIoapicAddr(int) const override;
    int GetNumberOfCpus() const override;
    int GetLocalApicId(int cpu) const override;
private:
    void Visit(const acpi_madt_processor &) override;
    void Visit(const acpi_madt_ioapic &) override;
    void Visit(const acpi_madt_ioapic_interrupt_source_override &) override;
    void Visit(const acpi_madt_ioapic_nmi_source &) override;
    void Visit(const acpi_madt_lapic_nmi &) override;
    void Visit(const acpi_madt_lapic_address_override &) override;
    void Visit(const acpi_madt_processor_local_x2apic &) override;
};

#endif //JEOKERNEL_ACPI_MADT_H
