#include "i915_winhal_compat.h"
pc_status I915WinHalInit(i915_winhal_context *c, const pc_hal_ops *h) {
 if (!c || pc_hal_validate(h)!=PC_STATUS_OK) return PC_STATUS_INVALID_ARGUMENT;
 c->hal=h; return PC_STATUS_OK;
}
#define VALID(c) ((c) && (c)->hal)
pc_status I915WinHalPciFind(i915_winhal_context *c,uint16_t v,uint16_t d,pc_pci_device *o){
 if(!VALID(c)||!c->hal->pci_find) return PC_STATUS_UNSUPPORTED;
 return c->hal->pci_find(c->hal->context,v,d,o);
}
pc_status I915WinHalPciRead32(i915_winhal_context*c,const pc_pci_device*d,uint16_t off,uint32_t*v){
 if(!VALID(c)||!c->hal->pci_read32)return PC_STATUS_UNSUPPORTED;
 return c->hal->pci_read32(c->hal->context,d,off,v);
}
pc_status I915WinHalPciWrite32(i915_winhal_context*c,const pc_pci_device*d,uint16_t off,uint32_t v){
 if(!VALID(c)||!c->hal->pci_write32)return PC_STATUS_UNSUPPORTED;
 return c->hal->pci_write32(c->hal->context,d,off,v);
}
pc_status I915WinHalMapMmio(i915_winhal_context*c,uint64_t b,uint64_t n,volatile void **p){
 if(!VALID(c)||!c->hal->map_mmio){if(p)*p=0;return PC_STATUS_UNSUPPORTED;}
 return c->hal->map_mmio(c->hal->context,b,n,p);
}
pc_status I915WinHalIrqRegister(i915_winhal_context*c,uint32_t g,pc_irq_trigger t,void(*h)(void*),void*x){
 if(!VALID(c)||!c->hal->irq_register)return PC_STATUS_UNSUPPORTED;
 return c->hal->irq_register(c->hal->context,g,t,h,x);
}
pc_status I915WinHalIrqMask(i915_winhal_context*c,uint32_t g,int m){
 if(!VALID(c)||!c->hal->irq_mask)return PC_STATUS_UNSUPPORTED;
 return c->hal->irq_mask(c->hal->context,g,m);
}
pc_status I915WinHalTimeNs(i915_winhal_context*c,uint64_t*t){
 if(!VALID(c)||!c->hal->monotonic_time_ns)return PC_STATUS_UNSUPPORTED;
 return c->hal->monotonic_time_ns(c->hal->context,t);
}
pc_status I915WinHalSleepNs(i915_winhal_context*c,uint64_t n){
 if(!VALID(c)||!c->hal->sleep_ns)return PC_STATUS_UNSUPPORTED;
 return c->hal->sleep_ns(c->hal->context,n);
}
