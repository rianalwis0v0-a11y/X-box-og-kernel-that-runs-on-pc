#include "include/pc_hal.h"
#include <stddef.h>

pc_status pc_hal_validate(const pc_hal_ops *ops)
{
    if (ops == 0) return PC_STATUS_INVALID_ARGUMENT;
    if (ops->abi_version != PC_HAL_ABI_VERSION ||
        ops->struct_size < sizeof(pc_hal_ops))
        return PC_STATUS_UNSUPPORTED;
    if (!ops->map_mmio || !ops->pci_find || !ops->pci_read32 ||
        !ops->pci_write32 || !ops->irq_register || !ops->irq_mask ||
        !ops->monotonic_time_ns || !ops->sleep_ns)
        return PC_STATUS_UNSUPPORTED;
    return PC_STATUS_OK;
}
