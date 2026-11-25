#include "i915_winhal_compat.h"

#include <stddef.h>
#include <stdint.h>

pc_status I915WinHalInit(i915_winhal_context *context, const pc_hal_ops *hal)
{
    pc_status status;
    if (!context) return PC_STATUS_INVALID_ARGUMENT;
    context->hal = 0;
    status = pc_hal_validate(hal);
    if (status != PC_STATUS_OK) return status;
    context->hal = hal;
    return PC_STATUS_OK;
}

#define VALID(c) ((c) != 0 && (c)->hal != 0)

pc_status I915WinHalPciFind(i915_winhal_context *c, uint16_t vendor,
                            uint16_t device, pc_pci_device *out)
{
    if (!VALID(c) || !out || vendor == 0xffffu || device == 0xffffu)
        return PC_STATUS_INVALID_ARGUMENT;
    if (!c->hal->pci_find) return PC_STATUS_UNSUPPORTED;
    return c->hal->pci_find(c->hal->context, vendor, device, out);
}

pc_status I915WinHalPciRead32(i915_winhal_context *c,
                              const pc_pci_device *device,
                              uint16_t offset, uint32_t *value)
{
    if (!VALID(c) || !device || !value || (offset & 3u) || offset > 0xfcu)
        return PC_STATUS_INVALID_ARGUMENT;
    if (!c->hal->pci_read32) return PC_STATUS_UNSUPPORTED;
    return c->hal->pci_read32(c->hal->context, device, offset, value);
}

pc_status I915WinHalPciWrite32(i915_winhal_context *c,
                               const pc_pci_device *device,
                               uint16_t offset, uint32_t value)
{
    if (!VALID(c) || !device || (offset & 3u) || offset > 0xfcu)
        return PC_STATUS_INVALID_ARGUMENT;
    if (!c->hal->pci_write32) return PC_STATUS_UNSUPPORTED;
    return c->hal->pci_write32(c->hal->context, device, offset, value);
}

pc_status I915WinHalMapMmio(i915_winhal_context *c, uint64_t base,
                            uint64_t length, volatile void **address)
{
    pc_status status;
    if (address) *address = 0;
    if (!VALID(c) || !address || !length || base > UINT64_MAX - length)
        return PC_STATUS_INVALID_ARGUMENT;
    if (!c->hal->map_mmio) return PC_STATUS_UNSUPPORTED;
    status = c->hal->map_mmio(c->hal->context, base, length, address);
    if (status != PC_STATUS_OK) {
        *address = 0;
        return status;
    }
    if (!*address) return PC_STATUS_IO_ERROR;
    return PC_STATUS_OK;
}

pc_status I915WinHalIrqRegister(i915_winhal_context *c, uint32_t gsi,
                                pc_irq_trigger trigger,
                                void (*handler)(void *), void *handler_context)
{
    if (!VALID(c) || !handler ||
        (trigger != PC_IRQ_EDGE && trigger != PC_IRQ_LEVEL))
        return PC_STATUS_INVALID_ARGUMENT;
    if (!c->hal->irq_register) return PC_STATUS_UNSUPPORTED;
    return c->hal->irq_register(c->hal->context, gsi, trigger,
                                handler, handler_context);
}

pc_status I915WinHalIrqMask(i915_winhal_context *c, uint32_t gsi, int masked)
{
    if (!VALID(c) || (masked != 0 && masked != 1))
        return PC_STATUS_INVALID_ARGUMENT;
    if (!c->hal->irq_mask) return PC_STATUS_UNSUPPORTED;
    return c->hal->irq_mask(c->hal->context, gsi, masked);
}

pc_status I915WinHalTimeNs(i915_winhal_context *c, uint64_t *time_ns)
{
    if (!VALID(c) || !time_ns) return PC_STATUS_INVALID_ARGUMENT;
    if (!c->hal->monotonic_time_ns) return PC_STATUS_UNSUPPORTED;
    return c->hal->monotonic_time_ns(c->hal->context, time_ns);
}

pc_status I915WinHalSleepNs(i915_winhal_context *c, uint64_t duration_ns)
{
    if (!VALID(c)) return PC_STATUS_INVALID_ARGUMENT;
    if (!c->hal->sleep_ns) return PC_STATUS_UNSUPPORTED;
    return c->hal->sleep_ns(c->hal->context, duration_ns);
}
