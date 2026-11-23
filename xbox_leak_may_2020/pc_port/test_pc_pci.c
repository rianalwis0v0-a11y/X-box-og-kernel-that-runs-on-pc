#include "pc_pci.h"

#include <assert.h>
#include <stdio.h>

static void test_io_bar(void)
{
    const uint32_t raw[] = { 0x0000c001u };
    pc_pci_bar bars[6];
    int count = PcPciDecodeBars(raw, 1, bars, 6);
    assert(count == 1);
    assert(bars[0].kind == PC_PCI_BAR_IO);
    assert(bars[0].base == 0x0000c000u);
    assert(bars[0].width == 32);
    assert(bars[0].slot == 0 && bars[0].registers == 1);
}

static void test_memory_bars(void)
{
    const uint32_t raw[] = {
        0xFEBF0008u, /* 32-bit prefetchable memory */
        0x12345004u, 0x00000012u, /* 64-bit memory BAR */
        0x00000000u,
        0x0000d001u
    };
    pc_pci_bar bars[6];
    int count = PcPciDecodeBars(raw, 5, bars, 6);
    assert(count == 3);
    assert(bars[0].kind == PC_PCI_BAR_MEMORY);
    assert(bars[0].base == 0xFEBF0000u);
    assert(bars[0].prefetchable == 1);
    assert(bars[0].width == 32);
    assert(bars[1].base == 0x0000001212345000ull);
    assert(bars[1].width == 64 && bars[1].registers == 2);
    assert(bars[1].slot == 1);
    assert(bars[2].kind == PC_PCI_BAR_IO && bars[2].base == 0xd000u);
    assert(bars[2].slot == 4);
}

static void test_invalid_inputs(void)
{
    const uint32_t last_64[] = { 0x00000004u };
    const uint32_t reserved[] = { 0x00000006u };
    pc_pci_bar bars[6];
    assert(PcPciDecodeBars(last_64, 1, bars, 6) == -1);
    assert(PcPciDecodeBars(reserved, 1, bars, 6) == -1);
    assert(PcPciDecodeBars(last_64, 1, bars, 0) == -1);
    assert(PcPciDecodeBars(last_64, 0, bars, 6) == -1);
    assert(PcPciDecodeBars(last_64, 1, 0, 6) == -1);
}

static void test_capacity_and_unassigned(void)
{
    const uint32_t raw[] = { 0u, 0x0000a001u, 0u };
    pc_pci_bar bars[1];
    assert(PcPciDecodeBars(raw, 3, bars, 1) == 1);
    assert(bars[0].slot == 1 && bars[0].base == 0xa000u);
    assert(PcPciDecodeBars(raw, 3, bars, 0) == -1);
}

int main(void)
{
    test_io_bar();
    test_memory_bars();
    test_invalid_inputs();
    test_capacity_and_unassigned();
    puts("PCI BAR decoder tests passed");
    return 0;
}
