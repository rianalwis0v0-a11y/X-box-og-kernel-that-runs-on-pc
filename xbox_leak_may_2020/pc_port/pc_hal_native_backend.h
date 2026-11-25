#ifndef PC_HAL_NATIVE_BACKEND_H
#define PC_HAL_NATIVE_BACKEND_H

#include "include/pc_hal.h"
#include "pc_paging.h"

/* MMIO is available only after the kernel binds its active page tables. */
void PcHalNativeSetPagingContext(pc_paging_context *paging);
const pc_hal_ops *PcHalNativeGetOps(void);

#endif
