#include "pc_acpi.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void checksum(uint8_t *p, uint32_t n, uint32_t offset)
{
    uint32_t i;
    uint8_t sum = 0;
    p[offset] = 0;
    for (i = 0; i < n; ++i) sum = (uint8_t)(sum + p[i]);
    p[offset] = (uint8_t)(0u - sum);
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8);
    p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}


#define TEST_PHYS_BASE 0x1000u
#define TEST_PHYS_SIZE 0x4000u
static uint8_t test_physical_memory[TEST_PHYS_SIZE];
static int test_map_physical(void *context, uint32_t address,
                             size_t minimum_bytes, const void **out)
{
    uint32_t offset;
    (void)context;
    if (!out || address < TEST_PHYS_BASE) return 0;
    offset = address - TEST_PHYS_BASE;
    if (offset > TEST_PHYS_SIZE || minimum_bytes > TEST_PHYS_SIZE - offset)
        return 0;
    *out = test_physical_memory + offset;
    return 1;
}
static void test_root_madt_walk(void)
{
    pc_acpi_rsdp_info rsdp = {0};
    pc_acpi_madt_info out;
    uint8_t *rsdt = test_physical_memory + (0x1100u - TEST_PHYS_BASE);
    uint8_t *madt = test_physical_memory + (0x1200u - TEST_PHYS_BASE);
    for (uint32_t i=0;i<TEST_PHYS_SIZE;++i) test_physical_memory[i]=0;
    rsdt[0]='R';rsdt[1]='S';rsdt[2]='D';rsdt[3]='T';
    put32(rsdt+4,40u);put32(rsdt+36,0x1200u);checksum(rsdt,40u,9u);
    madt[0]='A';madt[1]='P';madt[2]='I';madt[3]='C';
    put32(madt+4,44u);put32(madt+36,0xfee00000u);put32(madt+40,1u);
    checksum(madt,44u,9u);
    rsdp.revision=0;rsdp.rsdt_address=0x1100u;
    assert(PcAcpiFindMadtMapped(&rsdp,test_map_physical,0,&out));
    assert(out.local_apic_address==0xfee00000u);
    assert(out.ioapic_count==0u);
    /* An invalid root checksum must fail closed. */
    rsdt[9]^=1u;
    assert(!PcAcpiFindMadtMapped(&rsdp,test_map_physical,0,&out));
}

static void test_rsdp_v1(void)
{
    uint8_t rsdp[20] = {'R','S','D',' ','P','T','R',' '};
    pc_acpi_rsdp_info out;
    rsdp[15] = 0;
    put32(rsdp + 16, 0x12345000u);
    checksum(rsdp, 20, 8);
    assert(PcAcpiValidateRsdp(rsdp, sizeof(rsdp), &out));
    assert(out.revision == 0 && out.rsdt_address == 0x12345000u);
    rsdp[9] ^= 1u;
    assert(!PcAcpiValidateRsdp(rsdp, sizeof(rsdp), &out));
}

static void test_rsdp_scan(void)
{
    uint8_t region[96] = {0};
    pc_acpi_rsdp_info out;
    size_t offset = 0;
    uint8_t *rsdp = region + 32u;
    rsdp[0]='R'; rsdp[1]='S'; rsdp[2]='D'; rsdp[3]=' ';
    rsdp[4]='P'; rsdp[5]='T'; rsdp[6]='R'; rsdp[7]=' ';
    rsdp[15] = 0;
    put32(rsdp + 16, 0x00123000u);
    checksum(rsdp, 20u, 8u);
    assert(PcAcpiFindRsdp(region, sizeof(region), &out, &offset));
    assert(offset == 32u && out.rsdt_address == 0x00123000u);
    region[32u + 9u] ^= 1u;
    assert(!PcAcpiFindRsdp(region, sizeof(region), &out, &offset));
    assert(!PcAcpiFindRsdp(region, 19u, &out, &offset));
}

static void test_madt(void)
{
    uint8_t madt[66] = {0};
    pc_acpi_madt_info out;
    /* ACPI SDT header + MADT fixed fields + IOAPIC + ISO. */
    madt[0]='A'; madt[1]='P'; madt[2]='I'; madt[3]='C';
    put32(madt + 4, sizeof(madt));
    put32(madt + 36, 0xFEE00000u);
    put32(madt + 40, 1u);
    madt[44]=1; madt[45]=12; madt[46]=2;
    put32(madt + 48, 0xFEC00000u);
    put32(madt + 52, 0u);
    madt[56]=2; madt[57]=10; madt[58]=0; madt[59]=9;
    put32(madt + 60, 20u);
    madt[64]=0; madt[65]=0;
    checksum(madt, sizeof(madt), 9);
    assert(PcAcpiParseMadt(madt, sizeof(madt), &out));
    assert(out.local_apic_address == 0xFEE00000u);
    assert(out.ioapic_count == 1 && out.ioapics[0].address == 0xFEC00000u);
    assert(out.iso_count == 1 && out.overrides[0].source_irq == 9u);
    madt[45] = 1u;
    checksum(madt, sizeof(madt), 9);
    assert(!PcAcpiParseMadt(madt, sizeof(madt), &out));
}

static void test_sdt_bounds(void)
{
    uint8_t sdt[36] = {'A','P','I','C'};
    uint32_t length = 0;
    put32(sdt + 4, 36u);
    checksum(sdt, sizeof(sdt), 9);
    assert(PcAcpiValidateSdt(sdt, sizeof(sdt), "APIC", &length));
    assert(length == 36u);
    assert(!PcAcpiValidateSdt(sdt, sizeof(sdt)-1u, "APIC", &length));
    assert(!PcAcpiValidateSdt(sdt, sizeof(sdt), "FACP", &length));
}

int main(void)
{
    test_rsdp_v1();
    test_rsdp_scan();
    test_madt();
    test_root_madt_walk();
    test_sdt_bounds();
    puts("ACPI RSDP/SDT/MADT parser tests passed");
    return 0;
}
