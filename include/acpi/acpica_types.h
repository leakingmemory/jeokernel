//
// Created by sigsegv on 7/15/21.
//

#ifndef JEOKERNEL_ACPICA_TYPES_H
#define JEOKERNEL_ACPICA_TYPES_H

extern "C" {

#ifndef ACPI_MACHINE_WIDTH
#define ACPI_MACHINE_WIDTH 64
#define _LINUX
#define ACPI_USE_SYSTEM_CLIBRARY
#define ACPI_DISASSEMBLER
#define ACPI_ASL_COMPILER
#endif

#include <platform/acgcc.h>
#include <aclinux.h>
#include <acrestyp.h>

};

#endif //JEOKERNEL_ACPICA_TYPES_H
