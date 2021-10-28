//
// Created by sigsegv on 5/17/21.
//

#include <acpi/ACPI.h>
#include <thread>

ACPI::ACPI(const RSDPv1descriptor *rsdpdesc) : root(rsdpdesc), fadt{nullptr}, fadt2{nullptr} {
    for (int i = 0; i < root.size(); i++) {
        std::unique_ptr<acpi_table<RSDPv1descriptor,SystemDescriptionTable,uint8_t>> subtable{
            new acpi_table<RSDPv1descriptor,SystemDescriptionTable,uint8_t>(root[i])
        };
        char buf[5];
        for (int i = 0; i < 4; i++) {
            buf[i] = subtable->sdt->signature[i];
        }
        buf[4] = 0;
        get_klogger() << "ACPI record " << &(buf[0]) << "\n";
        if (buf[0] == 'F' && buf[1] == 'A' && buf[2] == 'C' && buf[3] == 'P') {
            if (sizeof(FADT) > (subtable->sdt->length - sizeof(subtable->sdt))) {
                get_klogger() << "FADT is shorter than expected structure\n";
            }
            fadt = (const FADT *) (const void *) &(subtable->sdt[0]);
            if (sizeof(FADT2) <= (subtable->sdt->length - sizeof(subtable->sdt))) {
                fadt2 = (const FADT2 *) (const void *) &(subtable->sdt[0]);
            }
            fadt_subtable = std::move(subtable);
        }
    }
}
