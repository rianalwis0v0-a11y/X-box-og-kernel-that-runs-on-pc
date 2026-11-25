#ifndef XBOX_PC_I915_DRIVER_H
#define XBOX_PC_I915_DRIVER_H

#include <stdint.h>
#include "include/pc_hal.h"

typedef enum i915_driver_state {
    I915_DRIVER_NOT_LOADED = 0,
    I915_DRIVER_NO_DEVICE = 1,
    I915_DRIVER_DEVICE_IDENTIFIED = 2,
    I915_DRIVER_DEFERRED_NO_MMIO = 3,
    I915_DRIVER_READY = 4,
    I915_DRIVER_FAILED = 5
} i915_driver_state;

typedef struct i915_driver_device {
    pc_pci_device pci;
    uint32_t class_revision;
    uint32_t bar0;
    uint8_t generation;
    i915_driver_state state;
} i915_driver_device;

/* Statically linked built-in driver entry. It only probes PCI configuration;
 * it does not write GPU registers or claim that Linux i915 has been ported. */
pc_status I915DriverLoad(const pc_hal_ops *hal, i915_driver_device *device);

#endif
