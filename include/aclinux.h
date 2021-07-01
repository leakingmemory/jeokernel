//
// Created by sigsegv on 6/23/21.
//

#ifndef JEOKERNEL_ACLINUX_H
#define JEOKERNEL_ACLINUX_H

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define COMPILER_DEPENDENT_UINT64 uint64_t
#define COMPILER_DEPENDENT_INT64 int64_t

#include <acconfig.h>
#include <actypes.h>
#include <actbl.h>
#include <aclocal.h>

extern uint32_t AcpiGbl_NextCmdNum;
extern ACPI_PARSE_OBJECT_LIST *AcpiGbl_TempListHead;
extern ACPI_EXTERNAL_LIST *AcpiGbl_ExternalList;
extern ACPI_EXTERNAL_FILE *AcpiGbl_ExternalFileList;

#endif //JEOKERNEL_ACLINUX_H
