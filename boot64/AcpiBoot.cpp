//
// Created by sigsegv on 5/17/21.
//

#include "AcpiBoot.h"

AcpiBoot::AcpiBoot(const MultibootInfoHeader &multiboot) : acpi_thread([this, &multiboot] () {
    if (!multiboot.has_parts()) {
        return;
    }
    const MultibootRsdp1 *rsdp1{nullptr};
    const MultibootRsdp2 *rsdp2{nullptr};
    {
        const auto *part = multiboot.first_part();
        do {
            if (part->type == 14) {
                rsdp1 = &(part->get_type14());
            }
            if (part->type == 15) {
                rsdp2 = &(part->get_type15());
            }
            part = part->hasNext(multiboot) ? part->next() : nullptr;
        } while (part != nullptr);
    }
    if (rsdp2 != nullptr) {
        get_klogger() << "ACPI v2 tables through multiboot2\n";
        acpi_boot(&(rsdp2->rsdp));
    } else if (rsdp1 != nullptr) {
        get_klogger() << "ACPI v1 tables through multiboot2\n";
        acpi_boot(&(rsdp1->rsdp));
    } else {
        get_klogger() << "ACPI not available through multiboot2\n";
    }
}) {
}

void AcpiBoot::acpi_boot(const RSDPv1descriptor *rsdp1) {
    ACPI acpi{rsdp1};
    bool has8042{false};
    if (acpi.fadt2 != nullptr) {
        if ((acpi.fadt2->BootArchitectureFlags & 2) != 0) {
            get_klogger() << "ACPI indicates PS/2 8042 chip available\n";
            has8042 = true;
        } else {
            get_klogger() << "ACPI indicates no PS/2 on this board\n";
        }
    } else if (acpi.fadt != nullptr) {
        get_klogger() << "ACPI FADT v1\n";
        has8042 = true;
    }
}

AcpiBoot::~AcpiBoot() {
    acpi_thread.join();
}
