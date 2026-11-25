#include "i915_driver.h"
#include <assert.h>
#include <stdio.h>

static pc_status find_device(void *ctx, uint16_t vendor, uint16_t device,
                             pc_pci_device *out)
{
    (void)ctx;
    if (vendor != 0x8086u || device != 0x3ea1u)
        return PC_STATUS_DEVICE_NOT_FOUND;
    out->vendor_id = vendor;
    out->device_id = device;
    out->segment = 0;
    out->bus = 0;
    out->device = 2;
    out->function = 0;
    out->revision = 1;
    return PC_STATUS_OK;
}
static pc_status read_config(void *ctx, const pc_pci_device *dev,
                             uint16_t offset, uint32_t *value)
{
    (void)ctx; (void)dev;
    if (offset == 0x08u) *value = 0x03000001u;
    else if (offset == 0x10u) *value = 0xf0000000u;
    else return PC_STATUS_INVALID_ARGUMENT;
    return PC_STATUS_OK;
}
static pc_status write_config(void *ctx, const pc_pci_device *dev,
                              uint16_t offset, uint32_t value)
{ (void)ctx; (void)dev; (void)offset; (void)value; return PC_STATUS_OK; }
static pc_status map_mmio(void *ctx, uint64_t base, uint64_t length,
                          volatile void **address)
{ (void)ctx; (void)base; (void)length; if (address) *address = 0; return PC_STATUS_UNSUPPORTED; }
static pc_status irq_register(void *ctx, uint32_t gsi, pc_irq_trigger trigger,
                              void (*handler)(void *), void *handler_context)
{ (void)ctx; (void)gsi; (void)trigger; (void)handler; (void)handler_context; return PC_STATUS_UNSUPPORTED; }
static pc_status irq_mask(void *ctx, uint32_t gsi, int masked)
{ (void)ctx; (void)gsi; (void)masked; return PC_STATUS_UNSUPPORTED; }
static pc_status time_ns(void *ctx, uint64_t *value)
{ (void)ctx; *value = 1; return PC_STATUS_OK; }
static pc_status sleep_ns(void *ctx, uint64_t value)
{ (void)ctx; (void)value; return PC_STATUS_OK; }

int main(void)
{
    const pc_hal_ops ops = {
        PC_HAL_ABI_VERSION, sizeof(pc_hal_ops), 0,
        map_mmio, find_device, read_config, write_config,
        irq_register, irq_mask, time_ns, sleep_ns
    };
    i915_driver_device device;
    pc_status status = I915DriverLoad(&ops, &device);
    assert(status == PC_STATUS_UNSUPPORTED);
    assert(device.state == I915_DRIVER_DEFERRED_NO_MMIO);
    assert(device.pci.vendor_id == 0x8086u);
    assert(device.pci.device_id == 0x3ea1u);
    assert(device.generation == PC_INTEL_GEN9);
    assert(device.bar0 == 0xf0000000u);
    assert(I915DriverLoad(&ops, 0) == PC_STATUS_INVALID_ARGUMENT);
    puts("built-in Intel UHD 610 driver probe tests PASS");
    return 0;
}
