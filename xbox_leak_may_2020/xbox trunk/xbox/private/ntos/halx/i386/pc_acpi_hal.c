/*++

Module Name: pc_acpi_hal.c

Abstract:
    Native-PC HAL ACPI discovery and safe MMIO mapping.  This stage only
    discovers the MADT and maps controller windows; it deliberately does not
    enable the LAPIC or program IOAPIC routes.  The current NT interrupt
    entry/exit assembly is still 8259-specific.

--*/
#include "halp.h"
#include "pc_acpi_hal.h"

#define HALP_ACPI_MAX_MAPPINGS 40
#define HALP_PAGE_MASK (~((ULONG)PAGE_SIZE - 1))

static PVOID HalpAcpiMappings[HALP_ACPI_MAX_MAPPINGS];
static ULONG HalpAcpiMappingCount;
pc_acpi_madt_info HalpPcAcpiMadtInfo;
PVOID HalpPcLocalApicMapping;
PVOID HalpPcIoApicMappings[PC_ACPI_MAX_IOAPICS];

static PVOID
HalpMapPhysicalRange(ULONG Address, SIZE_T Length)
{
    PHYSICAL_ADDRESS Physical;
    ULONG Offset;
    SIZE_T MapLength;
    PVOID Mapping;

    if (Address == 0 || Length == 0 || Length > (1024 * 1024) ||
        Length > 0xffffffffu - Address) return NULL;
    Offset = Address & (PAGE_SIZE - 1);
    if (Length > ((SIZE_T)-1) - Offset - (PAGE_SIZE - 1)) return NULL;
    MapLength = (Length + Offset + PAGE_SIZE - 1) & HALP_PAGE_MASK;
    Physical.QuadPart = (ULONGLONG)(Address & HALP_PAGE_MASK);
    Mapping = MmMapIoSpace(Physical, MapLength, MmNonCached);
    if (Mapping == NULL) return NULL;
    if (HalpAcpiMappingCount >= HALP_ACPI_MAX_MAPPINGS) {
        MmUnmapIoSpace(Mapping, MapLength);
        return NULL;
    }
    HalpAcpiMappings[HalpAcpiMappingCount++] = Mapping;
    return (PVOID)((PUCHAR)Mapping + Offset);
}

static int
HalpAcpiMapPhysical(void *Context, uint32_t Address,
                    size_t MinimumBytes, const void **MappedAddress)
{
    PVOID Mapping;
    UNREFERENCED_PARAMETER(Context);
    if (MappedAddress == NULL || Address == 0 || MinimumBytes == 0 ||
        MinimumBytes > 0xffffffffu - Address) return 0;
    Mapping = HalpMapPhysicalRange((ULONG)Address, (SIZE_T)MinimumBytes);
    if (Mapping == NULL) return 0;
    *MappedAddress = Mapping;
    return 1;
}

BOOLEAN
HalpPcAcpiInitialize(VOID)
{
    PHYSICAL_ADDRESS Physical;
    PUCHAR LowMemory;
    PUCHAR Ebda;
    PUCHAR Bios;
    USHORT EbdaSegment;
    pc_acpi_rsdp_info Rsdp;
    ULONG Index;

    RtlZeroMemory(&HalpPcAcpiMadtInfo, sizeof(HalpPcAcpiMadtInfo));
    HalpPcLocalApicMapping = NULL;
    for (Index = 0; Index < PC_ACPI_MAX_IOAPICS; ++Index)
        HalpPcIoApicMappings[Index] = NULL;
    HalpAcpiMappingCount = 0;

    /* The EBDA segment pointer is stored at physical 0x40E. */
    Physical.QuadPart = 0x400;
    LowMemory = (PUCHAR)MmMapIoSpace(Physical, PAGE_SIZE, MmNonCached);
    if (LowMemory != NULL) {
        HalpAcpiMappings[HalpAcpiMappingCount++] = LowMemory;
        EbdaSegment = *(volatile PUSHORT)(LowMemory + 0x0e);
        if (EbdaSegment >= 0x8000 && EbdaSegment < 0xa000) {
            Ebda = (PUCHAR)HalpMapPhysicalRange((ULONG)EbdaSegment << 4, 1024);
            if (Ebda != NULL && PcAcpiFindRsdp(Ebda, 1024, &Rsdp, NULL))
                goto FoundRsdp;
        }
    }

    /* BIOS firmware area is searched on the ACPI-required 16-byte boundary. */
    Bios = (PUCHAR)HalpMapPhysicalRange(0x000e0000, 0x20000);
    if (Bios == NULL || !PcAcpiFindRsdp(Bios, 0x20000, &Rsdp, NULL))
        return FALSE;

FoundRsdp:
    if (!PcAcpiFindMadtMapped(&Rsdp, HalpAcpiMapPhysical, NULL,
                              &HalpPcAcpiMadtInfo))
        return FALSE;

    /*
     * Retain uncached mappings for later APIC integration.  Do not enable
     * the local APIC or unmask IOAPIC inputs here: ixsysint.asm currently
     * performs PIC-specific EOI and IRQL masking operations.
     */
    if (HalpPcAcpiMadtInfo.local_apic_address == 0 ||
        (HalpPcAcpiMadtInfo.local_apic_address & (PAGE_SIZE - 1)) != 0)
        return FALSE;
    HalpPcLocalApicMapping = HalpMapPhysicalRange(
        HalpPcAcpiMadtInfo.local_apic_address, PAGE_SIZE);
    if (HalpPcLocalApicMapping == NULL) return FALSE;

    for (Index = 0; Index < HalpPcAcpiMadtInfo.ioapic_count; ++Index) {
        if (HalpPcAcpiMadtInfo.ioapics[Index].address == 0 ||
            (HalpPcAcpiMadtInfo.ioapics[Index].address & (PAGE_SIZE - 1)) != 0)
            return FALSE;
        HalpPcIoApicMappings[Index] = HalpMapPhysicalRange(
            HalpPcAcpiMadtInfo.ioapics[Index].address, PAGE_SIZE);
        if (HalpPcIoApicMappings[Index] == NULL) return FALSE;
    }
    return TRUE;
}
