#include "i915_winhal_compat.h"
#include <assert.h>
#include <stdio.h>

static int writes;
static pc_status find(void*c,uint16_t v,uint16_t d,pc_pci_device*out){(void)c;out->vendor_id=v;out->device_id=d;out->bus=2;return PC_STATUS_OK;}
static pc_status read32(void*c,const pc_pci_device*d,uint16_t o,uint32_t*v){(void)c;(void)d;(void)o;*v=0x12345678u;return PC_STATUS_OK;}
static pc_status write32(void*c,const pc_pci_device*d,uint16_t o,uint32_t v){(void)c;(void)d;(void)o;(void)v;writes++;return PC_STATUS_OK;}
static pc_status mmio(void*c,uint64_t b,uint64_t n,volatile void**p){(void)c;(void)b;(void)n;if(p)*p=0;return PC_STATUS_UNSUPPORTED;}
static pc_status irqreg(void*c,uint32_t g,pc_irq_trigger t,void(*h)(void*),void*x){(void)c;(void)g;(void)t;(void)h;(void)x;return PC_STATUS_UNSUPPORTED;}
static pc_status irqmask(void*c,uint32_t g,int m){(void)c;(void)g;(void)m;return PC_STATUS_UNSUPPORTED;}
static pc_status timefn(void*c,uint64_t*t){(void)c;*t=42;return PC_STATUS_OK;}
static pc_status sleepfn(void*c,uint64_t n){(void)c;(void)n;return PC_STATUS_OK;}
static void irq_handler(void *context){(void)context;}

int main(void){
 pc_hal_ops ops={PC_HAL_ABI_VERSION,sizeof(pc_hal_ops),0,mmio,find,read32,write32,irqreg,irqmask,timefn,sleepfn};
 i915_winhal_context c;pc_pci_device dev;uint32_t val=0;uint64_t time=0;volatile void*map=(void*)1;
 assert(I915WinHalInit(&c,&ops)==PC_STATUS_OK);
 assert(I915WinHalPciFind(&c,0x8086,0x3ea1,&dev)==PC_STATUS_OK && dev.bus==2);
 assert(I915WinHalPciRead32(&c,&dev,0,&val)==PC_STATUS_OK && val==0x12345678);
 assert(I915WinHalPciRead32(&c,&dev,2,&val)==PC_STATUS_INVALID_ARGUMENT);
 assert(I915WinHalPciRead32(&c,&dev,0,0)==PC_STATUS_INVALID_ARGUMENT);
 assert(I915WinHalPciWrite32(&c,&dev,4,1)==PC_STATUS_OK && writes==1);
 assert(I915WinHalPciWrite32(&c,&dev,3,1)==PC_STATUS_INVALID_ARGUMENT);
 assert(I915WinHalMapMmio(&c,0x1000,0x1000,&map)==PC_STATUS_UNSUPPORTED && map==0);
 map=(void*)1;
 assert(I915WinHalMapMmio(&c,UINT64_MAX-3,8,&map)==PC_STATUS_INVALID_ARGUMENT && map==0);
 assert(I915WinHalMapMmio(&c,0x1000,0, &map)==PC_STATUS_INVALID_ARGUMENT);
 assert(I915WinHalTimeNs(&c,&time)==PC_STATUS_OK && time==42);
 assert(I915WinHalTimeNs(&c,0)==PC_STATUS_INVALID_ARGUMENT);
 assert(I915WinHalSleepNs(&c,100)==PC_STATUS_OK);
 assert(I915WinHalIrqRegister(&c,1,PC_IRQ_EDGE,irq_handler,0)==PC_STATUS_UNSUPPORTED);
 assert(I915WinHalIrqRegister(&c,1,PC_IRQ_EDGE,0,0)==PC_STATUS_INVALID_ARGUMENT);
 assert(I915WinHalIrqRegister(&c,1,(pc_irq_trigger)9,irq_handler,0)==PC_STATUS_INVALID_ARGUMENT);
 assert(I915WinHalIrqMask(&c,1,1)==PC_STATUS_UNSUPPORTED);
 assert(I915WinHalIrqMask(&c,1,2)==PC_STATUS_INVALID_ARGUMENT);
 assert(I915WinHalInit(0,&ops)==PC_STATUS_INVALID_ARGUMENT);
 puts("i915-to-PC-HAL compatibility shim tests PASS");return 0;
}
