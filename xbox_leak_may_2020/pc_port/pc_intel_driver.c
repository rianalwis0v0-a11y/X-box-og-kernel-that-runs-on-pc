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
    if (count < 0) return PC_STATUS_IO_ERROR;

    /* The native HAL currently returns UNSUPPORTED for MMIO. Do not attach
     * to a guessed BAR or touch GPU registers until mapping is implemented. */
    for (i = 0; i < (uint32_t)count; ++i) {
        volatile void *mapping = 0;
        if (resources[i].kind != PC_PCI_BAR_MEMORY) continue;
        status = I915WinHalMapMmio(&compat, resources[i].base,
                                   PC_INTEL_GEN9_MMIO_MIN_BYTES, &mapping);
        if (status != PC_STATUS_OK) return status;
        if (mapping) {
            pc_intel_gen9_device gen9;
            if (PcIntelGen9Attach(&gpu, mapping,
                                  PC_INTEL_GEN9_MMIO_MIN_BYTES, &gen9)) {
                driver->state = PC_INTEL_DRIVER_READY;
                return PC_STATUS_OK;
            }
        }
    }
    return PC_STATUS_UNSUPPORTED;
}
