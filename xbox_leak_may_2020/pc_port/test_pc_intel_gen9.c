#include "pc_intel_gen9.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    pc_intel_gpu_info gpu;
    pc_intel_gen9_device device;
    uint32_t bars[PC_PCI_MAX_BARS] = {0xf0000000u, 0, 0, 0, 0, 0};
    uint32_t registers[1024];
    uint32_t value = 0;
    unsigned i;

    memset(registers, 0, sizeof(registers));
    registers[0] = 0x12345678u;
    registers[4] = 0x9abcdef0u;
    assert(PcIntelGpuBuildInfo(0x8086, 0x3ea1, 1,
        0x03000000u, bars, &gpu));
    assert(PcIntelGen9Attach(&gpu, registers, sizeof(registers), &device));
    assert(device.attached && device.device_id == 0x3ea1u);
    assert(PcIntelGen9Read32(&device, 0, &value) && value == 0x12345678u);
    assert(PcIntelGen9Read32(&device, 16, &value) && value == 0x9abcdef0u);
    assert(!PcIntelGen9Read32(&device, 2, &value));
    assert(!PcIntelGen9Read32(&device, sizeof(registers), &value));
    assert(!PcIntelGen9Read32(&device, 0xfffffffcu, &value));
    assert(!PcIntelGen9Attach(&gpu, registers, 4095u, &device));
    assert(!device.attached);

    /* A recognized Intel device from another generation must not attach. */
    assert(PcIntelGpuBuildInfo(0x8086, 0x9a49, 1,
        0x03000000u, bars, &gpu));
    assert(!PcIntelGen9Attach(&gpu, registers, sizeof(registers), &device));

    /* Non-Intel and non-display functions are rejected by discovery. */
    assert(!PcIntelGpuBuildInfo(0x10de, 0x3ea1, 1,
        0x03000000u, bars, &gpu));
    for (i = 0; i < 1024; ++i) registers[i] = 0;
    puts("Intel Gen9 MMIO access-boundary tests PASS");
    return 0;
}
