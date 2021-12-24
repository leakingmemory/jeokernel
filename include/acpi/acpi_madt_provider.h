//
// Created by sigsegv on 12/24/21.
//

#ifndef JEOKERNEL_ACPI_MADT_PROVIDER_H
#define JEOKERNEL_ACPI_MADT_PROVIDER_H

class acpi_madt_info;

class acpi_madt_provider {
public:
    virtual std::shared_ptr<acpi_madt_info> get_madt() = 0;
};

acpi_madt_provider &get_acpi_madt_provider();

#endif //JEOKERNEL_ACPI_MADT_PROVIDER_H
