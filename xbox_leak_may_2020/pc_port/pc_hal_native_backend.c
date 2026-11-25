#include "pc_hal_native_backend.h"

#include "pc_hal_native.h"
#include "pc_irq.h"
#include "pc_paging.h"

static pc_paging_context *native_paging;

#define PC_PCI_MAX_BUSES 256u
#define PC_PCI_MAX_DEVICES 32u
#define PC_PCI_MAX_FUNCTIONS 8u

static pc_status native_map_mmio(void *context, uint64_t physical_base,
                                 uint64_t length, volatile void **virtual_address)
{
    uint32_t mapped_base, mapped_length, page, cr0;
    uint64_t end;
    (void)context;
    if (!virtual_address) return PC_STATUS_INVALID_ARGUMENT;
    *virtual_address = 0;
    if (!native_paging || !length ||
        physical_base >= 0x100000000ULL ||
        length > UINT64_MAX - physical_base)
        return PC_STATUS_UNSUPPORTED;
    end = physical_base + length;
    if (end > 0x100000000ULL) return PC_STATUS_UNSUPPORTED;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    if (!(cr0 & 0x80000000u)) return PC_STATUS_UNSUPPORTED;
    if (!pc_paging_map_mmio_identity(native_paging, physical_base, length,
                                     &mapped_base, &mapped_length))
        return PC_STATUS_OUT_OF_RESOURCES;
    for (page = 0; page < mapped_length; page += PC_PAGING_PAGE_SIZE) {
        uintptr_t address = (uintptr_t)mapped_base + page;
        __asm__ volatile ("invlpg (%0)" : : "r"(address) : "memory");
    }
    *virtual_address = (volatile void *)(uintptr_t)physical_base;
    return PC_STATUS_OK;
}

static pc_status native_pci_find(void *context, uint16_t vendor_id,
                                 uint16_t device_id, pc_pci_device *out)
{
    uint32_t bus, device, function;
    (void)context;
    if (!out || vendor_id == 0xffffu || device_id == 0xffffu)
        return PC_STATUS_INVALID_ARGUMENT;

    for (bus = 0; bus < PC_PCI_MAX_BUSES; ++bus) {
        for (device = 0; device < PC_PCI_MAX_DEVICES; ++device) {
            uint32_t id0 = PcHalNativePciRead32((uint8_t)bus,
                (uint8_t)device, 0u, 0u);
            uint32_t header;
            uint32_t functions;
            if ((id0 & 0xffffu) == 0xffffu) continue;
            header = PcHalNativePciRead32((uint8_t)bus,
                (uint8_t)device, 0u, 0x0cu);
            functions = ((header >> 23) & 1u) ? PC_PCI_MAX_FUNCTIONS : 1u;
            for (function = 0; function < functions; ++function) {
                uint32_t id = function == 0u ? id0 :
                    PcHalNativePciRead32((uint8_t)bus, (uint8_t)device,
                                         (uint8_t)function, 0u);
                uint32_t class_revision;
                if ((id & 0xffffu) != vendor_id ||
                    (id >> 16) != device_id)
                    continue;
                class_revision = PcHalNativePciRead32((uint8_t)bus,
                    (uint8_t)device, (uint8_t)function, 0x08u);
                out->vendor_id = vendor_id;
                out->device_id = device_id;
                out->segment = 0u;
                out->bus = (uint8_t)bus;
                out->device = (uint8_t)device;
                out->function = (uint8_t)function;
                out->revision = (uint8_t)class_revision;
                return PC_STATUS_OK;
            }
        }
    }
    return PC_STATUS_DEVICE_NOT_FOUND;
}

static int valid_pci_device(const pc_pci_device *device, uint16_t offset)
{
    return device && device->segment == 0u && device->device < 32u &&
           device->function < 8u && (offset & 3u) == 0u && offset <= 0xfcu;
}

static pc_status native_pci_read32(void *context, const pc_pci_device *device,
                                   uint16_t offset, uint32_t *value)
{
    (void)context;
    if (!value || !valid_pci_device(device, offset))
        return PC_STATUS_INVALID_ARGUMENT;
    *value = PcHalNativePciRead32(device->bus, device->device,
                                  device->function, (uint8_t)offset);
    return *value == 0xffffffffu ? PC_STATUS_IO_ERROR : PC_STATUS_OK;
}

static pc_status native_pci_write32(void *context, const pc_pci_device *device,
                                    uint16_t offset, uint32_t value)
{
    (void)context;
    if (!valid_pci_device(device, offset))
        return PC_STATUS_INVALID_ARGUMENT;
    PcHalNativePciWrite32(device->bus, device->device,
                          device->function, (uint8_t)offset, value);
    return PC_STATUS_OK;
}

static pc_status native_irq_register(void *context, uint32_t gsi,
                                     pc_irq_trigger trigger,
                                     void (*handler)(void *),
                                     void *handler_context)
{
    (void)context;
    return PcIrqRegisterHandler(gsi, (uint32_t)trigger,
                                handler, handler_context) ?
           PC_STATUS_OK : PC_STATUS_UNSUPPORTED;
}

static pc_status native_irq_mask(void *context, uint32_t gsi, int masked)
{
    (void)context;
    return PcIrqMask(gsi, masked) ? PC_STATUS_OK : PC_STATUS_UNSUPPORTED;
}

static pc_status native_monotonic_time_ns(void *context, uint64_t *time_ns)
{
    (void)context;
    if (!time_ns || !PcIrqFrequencyHz()) return PC_STATUS_UNSUPPORTED;
    *time_ns = PcIrqMonotonicTimeNs();
    return PC_STATUS_OK;
}

static pc_status native_sleep_ns(void *context, uint64_t duration_ns)
{
    (void)context;
    return PcIrqSleepNs(duration_ns) ?
           PC_STATUS_OK : PC_STATUS_UNSUPPORTED;
}

void PcHalNativeSetPagingContext(pc_paging_context *paging)
{
    native_paging = paging;
}

const pc_hal_ops *PcHalNativeGetOps(void)
{
    static const pc_hal_ops operations = {
        PC_HAL_ABI_VERSION,
        sizeof(pc_hal_ops),
        0,
        native_map_mmio,
        native_pci_find,
        native_pci_read32,
        native_pci_write32,
        native_irq_register,
        native_irq_mask,
        native_monotonic_time_ns,
        native_sleep_ns
    };
    return &operations;
}
