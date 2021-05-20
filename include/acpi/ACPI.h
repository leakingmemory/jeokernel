//
// Created by sigsegv on 5/17/21.
//

#ifndef JEOKERNEL_ACPI_H
#define JEOKERNEL_ACPI_H

#include <cstdint>
#include <core/vmem.h>
#include <memory>

struct RSDPv1descriptor {
    char signature[8];
    uint8_t checksum;
    char oemsig[6];
    uint8_t revision;
    uint32_t rsdt_ptr;
} __attribute__((__packed__));

struct RSDPv2descriptor : RSDPv1descriptor {
    uint32_t length;
    uint64_t xsdt_ptr;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((__packed__));

struct SystemDescriptionTable {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oemid[6];
    char oemtblid[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((__packed__));

struct GenericAddressStructure
{
    uint8_t AddressSpace;
    uint8_t BitWidth;
    uint8_t BitOffset;
    uint8_t AccessSize;
    uint64_t Address;
} __attribute__((__packed__));

struct FADT
{
    uint32_t FirmwareCtrl;
    uint32_t Dsdt;

    // field used in ACPI 1.0; no longer in use, for compatibility only
    uint8_t  Reserved;

    uint8_t  PreferredPowerManagementProfile;
    uint16_t SCI_Interrupt;
    uint32_t SMI_CommandPort;
    uint8_t  AcpiEnable;
    uint8_t  AcpiDisable;
    uint8_t  S4BIOS_REQ;
    uint8_t  PSTATE_Control;
    uint32_t PM1aEventBlock;
    uint32_t PM1bEventBlock;
    uint32_t PM1aControlBlock;
    uint32_t PM1bControlBlock;
    uint32_t PM2ControlBlock;
    uint32_t PMTimerBlock;
    uint32_t GPE0Block;
    uint32_t GPE1Block;
    uint8_t  PM1EventLength;
    uint8_t  PM1ControlLength;
    uint8_t  PM2ControlLength;
    uint8_t  PMTimerLength;
    uint8_t  GPE0Length;
    uint8_t  GPE1Length;
    uint8_t  GPE1Base;
    uint8_t  CStateControl;
    uint16_t WorstC2Latency;
    uint16_t WorstC3Latency;
    uint16_t FlushSize;
    uint16_t FlushStride;
    uint8_t  DutyOffset;
    uint8_t  DutyWidth;
    uint8_t  DayAlarm;
    uint8_t  MonthAlarm;
    uint8_t  Century;

    // reserved in ACPI 1.0; used since ACPI 2.0+
    uint16_t BootArchitectureFlags;

    uint8_t  Reserved2;
    uint32_t Flags;

    // 12 byte structure; see below for details
    GenericAddressStructure ResetReg;

    uint8_t  ResetValue;
    uint8_t  Reserved3[3];

} __attribute__((__packed__));

struct FADT2 : FADT {
    // 64bit pointers - Available on ACPI 2.0+
    uint64_t                X_FirmwareControl;
    uint64_t                X_Dsdt;

    GenericAddressStructure X_PM1aEventBlock;
    GenericAddressStructure X_PM1bEventBlock;
    GenericAddressStructure X_PM1aControlBlock;
    GenericAddressStructure X_PM1bControlBlock;
    GenericAddressStructure X_PM2ControlBlock;
    GenericAddressStructure X_PMTimerBlock;
    GenericAddressStructure X_GPE0Block;
    GenericAddressStructure X_GPE1Block;
} __attribute__((__packed__));

template <class Desc, class Table, class Item> class acpi_table {
private:
    const Desc *desc;
    std::unique_ptr<vmem> vm_sdt;
public:
    const Table *sdt;
    explicit acpi_table(uint64_t physaddr) : desc(desc), vm_sdt(new vmem(8192)), sdt(nullptr) {
        init(physaddr);
    }
    explicit acpi_table(const Desc *desc) : desc(desc), vm_sdt(new vmem(8192)), sdt(nullptr) {
        init(desc);
    }
    void init(const Desc *desc) {
        uint64_t physaddr = desc->rsdt_ptr;
        init(physaddr);
    }
    void init(uint64_t physaddr) {
        {
            uint64_t offset{0};
            {
                offset = physaddr & 0x0000000000000FFF;
                physaddr = physaddr & 0xFFFFFFFFFFFFF000;
                vm_sdt->page(0).rmap(physaddr);
                vm_sdt->page(1).rmap(physaddr + 0x1000);
                sdt = (const Table *) (void *) (((uint8_t *) vm_sdt->pointer()) + offset);
            }
            uint64_t length = sdt->length;
            vm_sdt = std::unique_ptr<vmem>(new vmem(offset + length));
            for (int i = 0; i < vm_sdt->npages(); i++) {
                vm_sdt->page(i).rmap(physaddr);
                physaddr += 0x1000;
            }
            sdt = (const Table *) (void *) (((uint8_t *) vm_sdt->pointer()) + offset);
        }
    }

    uint64_t size() {
        return (sdt->length - sizeof(*sdt)) / sizeof(Item);
    }

    const Item &operator [](uint64_t i) {
        const uint8_t *ptr = (const uint8_t *) (const void *) sdt;
        ptr += sizeof(*sdt);
        const Item *tbl = (Item *) (void *) ptr;
        return tbl[i];
    }
};

class ACPI {
private:
    acpi_table<RSDPv1descriptor, SystemDescriptionTable, uint32_t> root;
    std::unique_ptr<acpi_table<RSDPv1descriptor,SystemDescriptionTable,uint8_t>> fadt_subtable;
public:
    const FADT *fadt;
    const FADT2 *fadt2;
    ACPI(const RSDPv1descriptor *rsdpdesc);
};

class ACPIv2 : public ACPI {
public:
    ACPIv2(const RSDPv2descriptor *rsdpdesc) : ACPI(rsdpdesc) {}
};

#endif //JEOKERNEL_ACPI_H
