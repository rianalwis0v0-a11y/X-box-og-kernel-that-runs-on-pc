#ifndef XBOX_PC_PORT_PC_HAL_H
#define XBOX_PC_PORT_PC_HAL_H

/*
 * Host-PC HAL contract for the native Xbox-kernel port.
 *
 * This is an interface boundary, not a fake implementation. A PC backend
 * must return PC_STATUS_UNSUPPORTED until an operation is genuinely wired
 * to the host platform. Keep Xbox executive code above this boundary.
 */
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum pc_status {
    PC_STATUS_OK = 0,
    PC_STATUS_UNSUPPORTED = 1,
    PC_STATUS_INVALID_ARGUMENT = 2,
    PC_STATUS_DEVICE_NOT_FOUND = 3,
    PC_STATUS_OUT_OF_RESOURCES = 4,
    PC_STATUS_IO_ERROR = 5
} pc_status;

typedef enum pc_irq_trigger {
    PC_IRQ_EDGE = 0,
    PC_IRQ_LEVEL = 1
} pc_irq_trigger;

typedef struct pc_memory_region {
    uint64_t physical_base;
    uint64_t length;
    uint32_t flags;
} pc_memory_region;

typedef struct pc_pci_device {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t segment;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t revision;
} pc_pci_device;

typedef struct pc_hal_ops {
    uint32_t abi_version;
    uint32_t struct_size;
    void *context;

    pc_status (*map_mmio)(void *context, uint64_t physical_base,
                          uint64_t length, volatile void **virtual_address);
    pc_status (*pci_find)(void *context, uint16_t vendor_id,
                          uint16_t device_id, pc_pci_device *out_device);
    pc_status (*pci_read32)(void *context, const pc_pci_device *device,
                            uint16_t offset, uint32_t *value);
    pc_status (*pci_write32)(void *context, const pc_pci_device *device,
                             uint16_t offset, uint32_t value);

    pc_status (*irq_register)(void *context, uint32_t gsi,
                              pc_irq_trigger trigger, void (*handler)(void *),
                              void *handler_context);
    pc_status (*irq_mask)(void *context, uint32_t gsi, int masked);

    pc_status (*monotonic_time_ns)(void *context, uint64_t *time_ns);
    pc_status (*sleep_ns)(void *context, uint64_t duration_ns);
} pc_hal_ops;

#define PC_HAL_ABI_VERSION 1u

/* Validate the ABI and required callbacks before kernel initialization. */
pc_status pc_hal_validate(const pc_hal_ops *ops);

#ifdef __cplusplus
}
#endif

#endif
