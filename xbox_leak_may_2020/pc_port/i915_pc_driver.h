#ifndef I915_PC_DRIVER_H
#define I915_PC_DRIVER_H
#include <stdint.h>
#include "i915_winhal_compat.h"
#include "pc_intel_gpu.h"

typedef enum i915_pc_state {
    I915_PC_NOT_STARTED = 0,
    I915_PC_DEVICE_NOT_FOUND = 1,
    I915_PC_UNSUPPORTED_GPU = 2,
    I915_PC_DETECTED_MMIO_DEFERRED = 3,
    I915_PC_READY = 4
} i915_pc_state;

typedef struct i915_pc_device {
    i915_pc_state state;
    pc_pci_device pci;
    pc_intel_gpu_info gpu;
} i915_pc_device;

/* Safe discovery-only startup. Does not enable PCI decode, map guessed BAR
 * lengths, wake GT power domains, or write GPU registers. */
pc_status I915PcDriverStart(i915_winhal_context *hal,
                            uint16_t device_id,
                            i915_pc_device *device);
#endif
