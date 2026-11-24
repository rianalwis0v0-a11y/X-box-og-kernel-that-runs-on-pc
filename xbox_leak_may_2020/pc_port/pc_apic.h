#ifndef PC_APIC_H
#define PC_APIC_H

#include <stdint.h>
#include "pc_acpi.h"

/* Caller must map these MMIO pages uncached before passing virtual addresses. */
typedef struct pc_apic_mmio {
    volatile uint32_t *local_apic;
    volatile uint32_t *ioapic[PC_ACPI_MAX_IOAPICS];
} pc_apic_mmio;

/* Enable the BSP local APIC and mask its local interrupt sources. */
int PcApicEnableLocal(pc_apic_mmio *mmio, uint32_t local_apic_virtual);
/* Attach already-mapped IOAPIC windows in MADT order; no physical mapping is done. */
int PcApicAttachIoApics(pc_apic_mmio *mmio,
                        const pc_acpi_madt_info *madt,
                        const uint32_t *ioapic_virtual);
/* Route one GSI to an IDT vector, with ACPI polarity/trigger flags. */
int PcApicRouteGsi(pc_apic_mmio *mmio, uint32_t gsi, uint8_t vector,
                   uint16_t acpi_flags, int masked);
void PcApicLocalEoi(const pc_apic_mmio *mmio);

#endif
