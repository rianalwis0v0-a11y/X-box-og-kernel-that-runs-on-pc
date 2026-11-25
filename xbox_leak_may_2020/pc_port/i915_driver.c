#include "i915_driver.h"

#include "i915_winhal_compat.h"
#include "pc_intel_gpu.h"

static void clear_device(i915_driver_device *device)
{
    i915_driver_device empty = {0};
    if (device) *device = empty;
}

pc_status I915DriverLoad(const pc_hal_ops *hal, i915_driver_device *device)
{
    i915_winhal_context compatibility;
    pc_status status;
    uint32_t class_revision = 0;
    uint32_t bar0 = 0;

    if (!device) return PC_STATUS_INVALID_ARGUMENT;
    clear_device(device);
    device->state = I915_DRIVER_FAILED;

    status = I915WinHalInit(&compatibility, hal);
    if (status != PC_STATUS_OK) return status;

    /* This first built-in target is the user's Intel UHD 610 (8086:3EA1).
     * Extend the PCI-ID table only alongside verified generation support. */
    status = I915WinHalPciFind(&compatibility, PC_INTEL_PCI_VENDOR_ID,
                               0x3ea1u, &device->pci);
    if (status == PC_STATUS_DEVICE_NOT_FOUND) {
        device->state = I915_DRIVER_NO_DEVICE;
        return status;
    }
    if (status != PC_STATUS_OK) return status;

    status = I915WinHalPciRead32(&compatibility, &device->pci, 0x08u,
                                &class_revision);
    if (status != PC_STATUS_OK) return status;
    status = I915WinHalPciRead32(&compatibility, &device->pci, 0x10u, &bar0);
    if (status != PC_STATUS_OK) return status;

    if (((class_revision >> 24) & 0xffu) != PC_INTEL_CLASS_DISPLAY ||
        ((class_revision >> 16) & 0xffu) != PC_INTEL_SUBCLASS_VGA ||
        PcIntelGpuClassifyDevice(device->pci.device_id) != PC_INTEL_GEN9) {
        device->state = I915_DRIVER_FAILED;
        return PC_STATUS_UNSUPPORTED;
    }

    device->class_revision = class_revision;
    device->bar0 = bar0;
    device->generation = PC_INTEL_GEN9;
    device->state = I915_DRIVER_DEVICE_IDENTIFIED;

    /* A BAR is not a usable mapping until its type, size, reserved-memory
     * ownership, and page-table mapping have all been validated. The native
     * PC HAL deliberately reports MMIO unsupported for now. Do not touch it. */
    if (!hal->map_mmio) {
        device->state = I915_DRIVER_DEFERRED_NO_MMIO;
        return PC_STATUS_UNSUPPORTED;
    }

    device->state = I915_DRIVER_DEFERRED_NO_MMIO;
    return PC_STATUS_UNSUPPORTED;
}
