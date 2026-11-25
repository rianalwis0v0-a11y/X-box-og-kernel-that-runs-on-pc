#include "pc_intel_driver.h"
#include "i915_winhal_compat.h"
#include "pc_intel_gen9.h"

pc_status PcIntelDriverInitialize(const pc_hal_ops *hal,
                                  pc_intel_driver *driver)
{
    i915_winhal_context compat;
    pc_pci_device pci;
    pc_intel_gpu_info gpu;
    uint32_t class_revision;
    uint32_t bars[PC_PCI_MAX_BARS];
    uint32_t i;
    pc_status status;
    pc_pci_bar resources[PC_PCI_MAX_BARS];
    int count;

    if (!driver) return PC_STATUS_INVALID_ARGUMENT;
    driver->state = PC_INTEL_DRIVER_NOT_STARTED;
    driver->gpu = (pc_intel_gpu_info){0};
    driver->pci = (pc_pci_device){0};

    status = I915WinHalInit(&compat, hal);
    if (status != PC_STATUS_OK) return status;

    /* This first built-in target is the user's UHD 610 (8086:3EA1).
     * Other Intel IDs can be added after their generation tables are tested. */
    status = I915WinHalPciFind(&compat, PC_INTEL_PCI_VENDOR_ID, 0x3ea1u, &pci);
    if (status != PC_STATUS_OK) return status;

    status = I915WinHalPciRead32(&compat, &pci, 0x08u, &class_revision);
    if (status != PC_STATUS_OK) return status;
    for (i = 0; i < PC_PCI_MAX_BARS; ++i) {
        status = I915WinHalPciRead32(&compat, &pci,
                                     (uint16_t)(0x10u + i * 4u), &bars[i]);
        if (status != PC_STATUS_OK) return status;
    }

    if (!PcIntelGpuBuildInfo(pci.vendor_id, pci.device_id,
                             (uint8_t)class_revision, class_revision,
                             bars, &gpu))
        return PC_STATUS_UNSUPPORTED;
    if (gpu.generation != PC_INTEL_GEN9)
        return PC_STATUS_UNSUPPORTED;

    driver->pci = pci;
    driver->gpu = gpu;
    driver->state = PC_INTEL_DRIVER_DISCOVERED;

    count = PcPciDecodeBars(bars, PC_PCI_MAX_BARS, resources,
                            PC_PCI_MAX_BARS);
    if (count < 0 || count != (int)gpu.resource_count) {
        driver->state = PC_INTEL_DRIVER_NOT_STARTED;
        driver->gpu = (pc_intel_gpu_info){0};
        driver->pci = (pc_pci_device){0};
        return PC_STATUS_IO_ERROR;
    }

    /* BAR values identify an address, not the resource's decoded length.
     * Keep discovery state and defer activation until firmware resource
     * lengths, reserved-range validation, and the native MMIO mapper exist.
     * Never guess a 4 KiB aperture or touch GT registers here. */
    return PC_STATUS_UNSUPPORTED;
}
