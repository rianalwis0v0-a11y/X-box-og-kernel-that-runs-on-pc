#include "pc_intel_gen9.h"

int PcIntelGen9Attach(const pc_intel_gpu_info *gpu,
                      const volatile void *mmio_mapping,
                      uint32_t mapping_length,
                      pc_intel_gen9_device *output)
{
    pc_intel_gen9_device empty = {0};
    if (!output) return 0;
    *output = empty;
    if (!gpu || !mmio_mapping ||
        gpu->vendor_id != PC_INTEL_PCI_VENDOR_ID ||
        gpu->class_code != PC_INTEL_CLASS_DISPLAY ||
        gpu->subclass != PC_INTEL_SUBCLASS_VGA ||
        gpu->generation != PC_INTEL_GEN9 ||
        mapping_length < PC_INTEL_GEN9_MMIO_MIN_BYTES)
        return 0;

    output->mmio = (const volatile uint8_t *)mmio_mapping;
    output->mmio_length = mapping_length;
    output->device_id = gpu->device_id;
    output->generation = gpu->generation;
    output->attached = 1u;
    return 1;
}

int PcIntelGen9Read32(const pc_intel_gen9_device *device,
                      uint32_t register_offset, uint32_t *value)
{
    const volatile uint32_t *register_address;
    if (!device || !value || !device->attached || !device->mmio ||
        (register_offset & 3u) != 0u ||
        register_offset > device->mmio_length ||
        device->mmio_length - register_offset < sizeof(uint32_t))
        return 0;

    register_address = (const volatile uint32_t *)(const volatile void *)
        (device->mmio + register_offset);
    *value = *register_address;
    return 1;
}
