#ifndef PC_APIC_H
#define PC_APIC_H
#include <stdint.h>
#include "pc_acpi.h"
/* Caller maps MMIO pages uncached and supplies their virtual addresses. */
typedef struct pc_apic_mmio {
 volatile uint32_t *local_apic;
 volatile uint32_t *ioapic[PC_ACPI_MAX_IOAPICS];
 uint32_t ioapic_gsi_base[PC_ACPI_MAX_IOAPICS];
 uint8_t ioapic_count;
} pc_apic_mmio;
int PcApicEnableLocal(pc_apic_mmio *, uint32_t);
int PcApicAttachIoApics(pc_apic_mmio *, const pc_acpi_madt_info *, const uint32_t *);
int PcApicRouteGsi(pc_apic_mmio *, uint32_t, uint8_t, uint16_t, int);
void PcApicLocalEoi(const pc_apic_mmio *);
#endif
