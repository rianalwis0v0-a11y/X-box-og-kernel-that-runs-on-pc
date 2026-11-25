#include "i915_pc_driver.h"
#include <assert.h>
#include <stdio.h>

static unsigned map_calls;
static pc_status map_mmio(void*c,uint64_t b,uint64_t n,volatile void**p)
{(void)c;(void)b;(void)n;(void)p;++map_calls;return PC_STATUS_UNSUPPORTED;}
static pc_status pci_find(void*c,uint16_t v,uint16_t d,pc_pci_device*out)
{(void)c;out->vendor_id=v;out->device_id=d;out->bus=0;out->device=2;out->function=0;return PC_STATUS_OK;}
static pc_status pci_read(void*c,const pc_pci_device*d,uint16_t off,uint32_t*out)
{(void)c;(void)d;if(off==8)*out=0x03000000u;else if(off==12)*out=0;else if(off==0x10)*out=0xf0000000u;else *out=0;return PC_STATUS_OK;}
static pc_status pci_write(void*c,const pc_pci_device*d,uint16_t o,uint32_t v)
{(void)c;(void)d;(void)o;(void)v;return PC_STATUS_OK;}
static pc_status irq_reg(void*c,uint32_t g,pc_irq_trigger t,void(*h)(void*),void*x)
{(void)c;(void)g;(void)t;(void)h;(void)x;return PC_STATUS_UNSUPPORTED;}
static pc_status irq_mask(void*c,uint32_t g,int m)
{(void)c;(void)g;(void)m;return PC_STATUS_UNSUPPORTED;}
static pc_status clock_ns(void*c,uint64_t*t){(void)c;*t=1;return PC_STATUS_OK;}
static pc_status sleep_ns(void*c,uint64_t n){(void)c;(void)n;return PC_STATUS_OK;}

int main(void)
{
 pc_hal_ops ops={PC_HAL_ABI_VERSION,sizeof(pc_hal_ops),0,map_mmio,pci_find,
   pci_read,pci_write,irq_reg,irq_mask,clock_ns,sleep_ns};
 i915_winhal_context hal;
 i915_pc_device device;
 assert(I915WinHalInit(&hal,&ops)==PC_STATUS_OK);
 assert(I915PcDriverStart(&hal,0x3ea1,&device)==PC_STATUS_UNSUPPORTED);
 assert(device.state==I915_PC_DETECTED_MMIO_DEFERRED);
 assert(device.gpu.generation==PC_INTEL_GEN9);
 assert(device.gpu.resource_count==1);
 assert(map_calls==0);
 assert(I915PcDriverStart(&hal,0,&device)==PC_STATUS_INVALID_ARGUMENT);
 puts("i915 PC driver safe startup tests PASS");
 return 0;
}
