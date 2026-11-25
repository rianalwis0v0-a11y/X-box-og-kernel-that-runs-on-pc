#ifndef PC_INTEL_DRIVER_H
#define PC_INTEL_DRIVER_H

#include "include/pc_hal.h"
#include "pc_intel_gpu.h"

typedef enum pc_intel_driver_state {
    PC_INTEL_DRIVER_NOT_STARTED = 0,
    PC_INTEL_DRIVER_DISCOVERED = 1,
    PC_INTEL_DRIVER_READY = 2
} pc_intel_driver_state;

typedef struct pc_intel_driver {
    pc_intel_gpu_info gpu;
    pc_pci_device pci;
    pc_intel_driver_state state;
} pc_intel_driver;

/* Built-in driver entry point. Discovery is read-only; READY is not reported
 * until MMIO mapping and Gen9 attachment are genuinely available. */
pc_status PcIntelDriverInitialize(const pc_hal_ops *hal,
                                  pc_intel_driver *driver);

#endif
