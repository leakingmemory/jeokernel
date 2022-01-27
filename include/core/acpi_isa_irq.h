//
// Created by sigsegv on 1/26/22.
//

#ifndef JEOKERNEL_ACPI_ISA_IRQ_H
#define JEOKERNEL_ACPI_ISA_IRQ_H

#include <cstdint>

class acpi_isa_irq {
public:
    virtual uint16_t GetIsaIoapicPin(uint8_t isaIrq) const = 0;
};

#endif //JEOKERNEL_ACPI_ISA_IRQ_H
