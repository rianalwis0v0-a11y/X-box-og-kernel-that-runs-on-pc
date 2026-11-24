#include "pc_intel_gpu.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    pc_intel_gpu_info info;
    uint32_t bars[PC_PCI_MAX_BARS] = {
        0xf0000000u, 0x00000001u, 0, 0, 0, 0
    };
    assert(PcIntelGpuClassifyDevice(0x3e91) == PC_INTEL_GEN9);
    assert(PcIntelGpuClassifyDevice(0x9a49) == PC_INTEL_GEN12);
    assert(PcIntelGpuClassifyDevice(0xffff) == PC_INTEL_GEN_UNKNOWN);
    assert(PcIntelGpuBuildInfo(0x8086, 0x3e91, 1,
        0x03000000u, bars, &info));
    assert(info.generation == PC_INTEL_GEN9);
    assert(info.resource_count == 2);
    assert(info.resources[0].kind == PC_PCI_BAR_MEMORY);
    assert(info.resources[0].base == 0xf0000000ull);
    assert(info.resources[1].kind == PC_PCI_BAR_IO);
    assert(!PcIntelGpuBuildInfo(0x10de, 0x3e91, 1,
        0x03000000u, bars, &info));
    assert(!PcIntelGpuBuildInfo(0x8086, 0x3e91, 1,
        0x02000000u, bars, &info));
    puts("Intel GPU discovery/classification tests PASS");
    return 0;
}
