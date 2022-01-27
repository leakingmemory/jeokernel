//
// Created by sigsegv on 1/25/22.
//

#ifndef JEOKERNEL_ACPI_VISITORS_H
#define JEOKERNEL_ACPI_VISITORS_H

struct acpi_device_info;

class AcpiDeviceVisitor {
public:
    virtual void Visit(acpi_device_info &devInfo) = 0;
};

#endif //JEOKERNEL_ACPI_VISITORS_H
