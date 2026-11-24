#ifndef PC_HAL_NATIVE_BACKEND_H
#define PC_HAL_NATIVE_BACKEND_H

#include "include/pc_hal.h"

/* Native ring-0 PC backend. Unsupported services return UNSUPPORTED. */
const pc_hal_ops *PcHalNativeGetOps(void);

#endif
