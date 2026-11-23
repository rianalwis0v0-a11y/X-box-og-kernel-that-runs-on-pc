#include "pc_acpi.h"

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t read_u64(const uint8_t *p)
{
    return (uint64_t)read_u32(p) | ((uint64_t)read_u32(p + 4) << 32);
}

static int checksum_ok(const uint8_t *p, size_t length)
{
    uint8_t sum = 0;
    size_t i;
    for (i = 0; i < length; ++i) sum = (uint8_t)(sum + p[i]);
    return sum == 0;
}

int PcAcpiValidateRsdp(const void *data, size_t available,
                       pc_acpi_rsdp_info *out)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t length;
    if (!p || !out || available < 20u) return 0;
    if (p[0] != 'R' || p[1] != 'S' || p[2] != 'D' || p[3] != ' ' ||
        p[4] != 'P' || p[5] != 'T' || p[6] != 'R' || p[7] != ' ')
        return 0;
    if (!checksum_ok(p, 20u)) return 0;

    out->revision = p[15];
    out->rsdt_address = read_u32(p + 16);
    out->xsdt_address = 0;
    out->length = 20u;
    if (out->revision >= 2u) {
        if (available < 36u) return 0;
        length = read_u32(p + 20);
        if (length < 36u || (size_t)length > available) return 0;
        if (!checksum_ok(p, length)) return 0;
        out->length = length;
        out->xsdt_address = read_u64(p + 24);
    }
    return 1;
}

int PcAcpiValidateSdt(const void *data, size_t available,
                      const char expected_signature[4], uint32_t *out_length)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t length;
    if (!p || available < 36u) return 0;
    length = read_u32(p + 4);
    if (length < 36u || (size_t)length > available) return 0;
    if (expected_signature &&
        (p[0] != (uint8_t)expected_signature[0] ||
         p[1] != (uint8_t)expected_signature[1] ||
         p[2] != (uint8_t)expected_signature[2] ||
         p[3] != (uint8_t)expected_signature[3]))
        return 0;
    if (!checksum_ok(p, length)) return 0;
    if (out_length) *out_length = length;
    return 1;
}

int PcAcpiParseMadt(const void *data, size_t available, pc_acpi_madt_info *out)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t length, offset;
    pc_acpi_madt_info parsed;
    if (!out || !PcAcpiValidateSdt(data, available, "APIC", &length) ||
        length < 44u)
        return 0;

    parsed.local_apic_address = read_u32(p + 36);
    parsed.flags = read_u32(p + 40);
    parsed.ioapic_count = 0;
    parsed.iso_count = 0;
    offset = 44u;

    while (offset < length) {
        uint8_t type, record_length;
        const uint8_t *r;
        if (length - offset < 2u) return 0;
        r = p + offset;
        type = r[0];
        record_length = r[1];
        if (record_length < 2u || (uint32_t)record_length > length - offset)
            return 0;

        if (type == 1u) {
            pc_acpi_ioapic *io;
            if (record_length < 12u || parsed.ioapic_count >= PC_ACPI_MAX_IOAPICS)
                return 0;
            io = &parsed.ioapics[parsed.ioapic_count++];
            io->id = r[2];
            io->address = read_u32(r + 4);
            io->gsi_base = read_u32(r + 8);
        } else if (type == 2u) {
            pc_acpi_iso *iso;
            if (record_length < 10u || parsed.iso_count >= PC_ACPI_MAX_ISO)
                return 0;
            iso = &parsed.overrides[parsed.iso_count++];
            iso->bus = r[2];
            iso->source_irq = r[3];
            iso->gsi = read_u32(r + 4);
            iso->flags = (uint16_t)((uint16_t)r[8] | ((uint16_t)r[9] << 8));
        } else if (type == 5u) {
            uint64_t address;
            if (record_length < 12u) return 0;
            address = read_u64(r + 4);
            /* This port is 32-bit; never silently truncate a firmware address. */
            if (address > UINT32_MAX) return 0;
            parsed.local_apic_address = (uint32_t)address;
        }
        offset += record_length;
    }

    *out = parsed;
    return 1;
}
