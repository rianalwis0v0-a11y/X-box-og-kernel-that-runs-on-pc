#include "i915_pc_driver.h"
#include "pc_pci.h"

pc_status I915PcDriverStart(i915_winhal_context *hal,
                            uint16_t device_id,
                            i915_pc_device *device)
{
    uint32_t class_revision = 0;
    uint32_t raw_bars[PC_PCI_MAX_BARS] = {0, 0, 0, 0, 0, 0};
    uint32_t header = 0;
    uint8_t bar_count;
    uint8_t i;
    pc_status status;

    if (!hal || !device || !device_id)
        return PC_STATUS_INVALID_ARGUMENT;

    device->state = I915_PC_NOT_STARTED;
    device->gpu = (pc_intel_gpu_info){0};
    device->pci = (pc_pci_device){0};

    status = I915WinHalPciFind(hal, PC_INTEL_PCI_VENDOR_ID,
                               device_id, &device->pci);
    if (status != PC_STATUS_OK) {
        device->state = I915_PC_DEVICE_NOT_FOUND;
        return status;
    }

    status = I915WinHalPciRead32(hal, &device->pci, 0x08u, &class_revision);
    if (status != PC_STATUS_OK) return status;
    status = I915WinHalPciRead32(hal, &device->pci, 0x0cu, &header);
    if (status != PC_STATUS_OK) return status;

    bar_count = (uint8_t)(((header >> 16) & 0x7fu) == 0u ? 6u :
                          (((header >> 16) & 0x7fu) == 1u ? 2u : 0u));
    for (i = 0; i < bar_count; ++i) {
        status = I915WinHalPciRead32(hal, &device->pci,
                                     (uint16_t)(0x10u + 4u * i),
                                     &raw_bars[i]);
        if (status != PC_STATUS_OK) return status;
    }

    if (!PcIntelGpuBuildInfo(device->pci.vendor_id, device->pci.device_id,
            (uint8_t)class_revision, class_revision, raw_bars, &device->gpu) ||
        device->gpu.generation != PC_INTEL_GEN9) {
        device->state = I915_PC_UNSUPPORTED_GPU;
        return PC_STATUS_UNSUPPORTED;
    }

    /* The PCI BAR values are assigned addresses, not resource lengths.
     * Until firmware resource lengths and safe MMIO mapping are available,
     * do not map a guessed 4 KiB window or touch GT registers. */
    device->state = I915_PC_DETECTED_MMIO_DEFERRED;
    return PC_STATUS_UNSUPPORTED;
}
