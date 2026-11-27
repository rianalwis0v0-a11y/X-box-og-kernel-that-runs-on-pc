#ifndef _PC_ACPI_HAL_H_
#define _PC_ACPI_HAL_H_

#include "pc_acpi.h"

/* ACPI data discovered by the native PC HAL during phase 1. */
extern pc_acpi_madt_info HalpPcAcpiMadtInfo;
extern PVOID HalpPcLocalApicMapping;
extern PVOID HalpPcIoApicMappings[PC_ACPI_MAX_IOAPICS];
BOOLEAN HalpPcAcpiInitialize(VOID);

#endif
