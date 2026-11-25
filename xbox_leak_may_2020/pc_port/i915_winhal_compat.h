#ifndef I915_WINHAL_COMPAT_H
#define I915_WINHAL_COMPAT_H
#include "include/pc_hal.h"

/* Porting shim: selected Linux-i915-facing operations translated to PC HAL.
 * This is not a Linux kernel ABI emulator or a drop-in i915 module loader. */
typedef struct i915_winhal_context { const pc_hal_ops *hal; } i915_winhal_context;
pc_status I915WinHalInit(i915_winhal_context *, const pc_hal_ops *);
pc_status I915WinHalPciFind(i915_winhal_context *, uint16_t, uint16_t, pc_pci_device *);
pc_status I915WinHalPciRead32(i915_winhal_context *, const pc_pci_device *, uint16_t, uint32_t *);
pc_status I915WinHalPciWrite32(i915_winhal_context *, const pc_pci_device *, uint16_t, uint32_t);
pc_status I915WinHalMapMmio(i915_winhal_context *, uint64_t, uint64_t, volatile void **);
pc_status I915WinHalIrqRegister(i915_winhal_context *, uint32_t, pc_irq_trigger, void (*)(void *), void *);
pc_status I915WinHalIrqMask(i915_winhal_context *, uint32_t, int);
pc_status I915WinHalTimeNs(i915_winhal_context *, uint64_t *);
pc_status I915WinHalSleepNs(i915_winhal_context *, uint64_t);
#endif
