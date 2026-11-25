#include "pc_intel_driver.h"
#include <assert.h>
#include <stdio.h>

static uint32_t config[64];
static pc_status map_no(void*c,uint64_t b,uint64_t n,volatile void**p){(void)c;(void)b;(void)n;*p=0;return PC_STATUS_UNSUPPORTED;}
static pc_status find(void*c,uint16_t v,uint16_t d,pc_pci_device*out){(void)c;if(v!=0x8086||d!=0x3ea1)return PC_STATUS_DEVICE_NOT_FOUND;*out=(pc_pci_device){v,d,0,0,2,0,1};return PC_STATUS_OK;}
static pc_status rd(void*c,const pc_pci_device*d,uint16_t off,uint32_t*out){(void)c;(void)d;if(off>0xfcu||(off&3u))return PC_STATUS_INVALID_ARGUMENT;*out=config[off/4u];return PC_STATUS_OK;}
static pc_status wr(void*c,const pc_pci_device*d,uint16_t off,uint32_t v){(void)c;(void)d;(void)off;(void)v;return PC_STATUS_OK;}
static pc_status irqreg(void*c,uint32_t g,pc_irq_trigger t,void(*h)(void*),void*x){(void)c;(void)g;(void)t;(void)h;(void)x;return PC_STATUS_UNSUPPORTED;}
static pc_status irqmask(void*c,uint32_t g,int m){(void)c;(void)g;(void)m;return PC_STATUS_UNSUPPORTED;}
static pc_status timefn(void*c,uint64_t*t){(void)c;*t=0;return PC_STATUS_OK;}
static pc_status sleepfn(void*c,uint64_t n){(void)c;(void)n;return PC_STATUS_OK;}
int main(void){
 pc_hal_ops ops={PC_HAL_ABI_VERSION,sizeof(pc_hal_ops),0,map_no,find,rd,wr,irqreg,irqmask,timefn,sleepfn};
 pc_intel_driver d;
 config[2]=0x03000001u; /* class 03, subclass 00, VGA */
 config[4]=0xF0000000u; /* assigned MMIO BAR */
 assert(PcIntelDriverInitialize(&ops,&d)==PC_STATUS_UNSUPPORTED);
 assert(d.state==PC_INTEL_DRIVER_DISCOVERED);
 assert(d.gpu.vendor_id==0x8086 && d.gpu.device_id==0x3ea1);
 config[2]=0x02000001u; /* not a VGA display controller */
 assert(PcIntelDriverInitialize(&ops,&d)==PC_STATUS_UNSUPPORTED);
 assert(d.state==PC_INTEL_DRIVER_NOT_STARTED);
 config[2]=0x03000001u;
 config[9]=0x00000006u; /* reserved memory BAR encoding */
 assert(PcIntelDriverInitialize(&ops,&d)==PC_STATUS_IO_ERROR);
 assert(d.state==PC_INTEL_DRIVER_NOT_STARTED);
 assert(d.gpu.vendor_id==0 && d.pci.vendor_id==0);
 puts("Intel built-in driver discovery/load tests PASS");return 0;
}
