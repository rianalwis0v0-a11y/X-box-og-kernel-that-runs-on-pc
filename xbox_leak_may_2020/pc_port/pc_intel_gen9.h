#ifndef PC_INTEL_GEN9_H
#define PC_INTEL_GEN9_H

#include <stdint.h>
#include "pc_intel_gpu.h"

/* First-generation-specific backend boundary for Intel Gen9/9.5.
 * This module is intentionally read-only until a real MMIO mapper and
 * platform power-management sequencing are available. */
#define PC_INTEL_GEN9_MMIO_MIN_BYTES 0x1000u

typedef struct pc_intel_gen9_device {
    const volatile uint8_t *mmio;
    uint32_t mmio_length;
    uint16_t device_id;
    uint8_t generation;
    uint8_t attached;
} pc_intel_gen9_device;

/* Attach only to a known Gen9-family Intel VGA device and a caller-validated
 * MMIO mapping. The mapping must cover at least 4 KiB. No hardware is touched. */
int PcIntelGen9Attach(const pc_intel_gpu_info *gpu,
                      const volatile void *mmio_mapping,
                      uint32_t mapping_length,
                      pc_intel_gen9_device *output);

/* Bounds-checked aligned MMIO read. Caller must ensure the mapping is live
 * and required GT power/forcewake domains are awake before reading GT regs. */
int PcIntelGen9Read32(const pc_intel_gen9_device *device,
                      uint32_t register_offset, uint32_t *value);

#endif
