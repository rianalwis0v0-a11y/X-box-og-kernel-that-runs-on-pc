#ifndef PC_ACPI_H
#define PC_ACPI_H

#include <stddef.h>
#include <stdint.h>

#define PC_ACPI_MAX_IOAPICS 8u
#define PC_ACPI_MAX_ISO 16u

typedef struct pc_acpi_rsdp_info {
    uint8_t revision;
    uint32_t rsdt_address;
    uint64_t xsdt_address;
    uint32_t length;
} pc_acpi_rsdp_info;

typedef struct pc_acpi_ioapic {
    uint8_t id;
    uint32_t address;
    uint32_t gsi_base;
} pc_acpi_ioapic;

typedef struct pc_acpi_iso {
    uint8_t bus;
    uint8_t source_irq;
    uint32_t gsi;
    uint16_t flags;
} pc_acpi_iso;

typedef struct pc_acpi_madt_info {
    uint32_t local_apic_address;
    uint32_t flags;
    uint8_t ioapic_count;
    uint8_t iso_count;
    pc_acpi_ioapic ioapics[PC_ACPI_MAX_IOAPICS];
    pc_acpi_iso overrides[PC_ACPI_MAX_ISO];
} pc_acpi_madt_info;

/* Search a firmware-provided region on 16-byte boundaries for a valid RSDP. */
int PcAcpiFindRsdp(const void *region, size_t available,
                   pc_acpi_rsdp_info *out, size_t *out_offset);

/* Validate checksums and lengths before exposing firmware-provided addresses. */
int PcAcpiValidateRsdp(const void *data, size_t available,
                       pc_acpi_rsdp_info *out);
int PcAcpiValidateSdt(const void *data, size_t available,
                      const char expected_signature[4], uint32_t *out_length);
/* Parse MADT type 1 (I/O APIC), type 2 (interrupt source override),
   and type 5 (local APIC address override). Unknown records are skipped. */
int PcAcpiParseMadt(const void *data, size_t available, pc_acpi_madt_info *out);

#endif
