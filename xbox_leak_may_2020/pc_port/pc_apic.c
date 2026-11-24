#include "pc_apic.h"
#define APIC_BASE_MSR 0x1bu
#define LAPIC_ID 0x020u
#define LAPIC_TPR 0x080u
#define LAPIC_EOI 0x0b0u
#define LAPIC_SVR 0x0f0u
#define IOREGSEL 0x00u
#define IOWIN 0x10u
#define IOAPIC_VERSION 0x01u
#define IOAPIC_REDIR 0x10u
#define REDIR_MASK (1u << 16)

int PcApicSupported(void)
{
 uint32_t eax=1u, ebx, ecx=0u, edx;
 __asm__ volatile ("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx));
 (void)ebx; (void)ecx;
 return (edx & (1u << 9)) != 0u;
}
static uint64_t rdmsr(uint32_t n){uint32_t l,h;__asm__ volatile("rdmsr":"=a"(l),"=d"(h):"c"(n));return ((uint64_t)h<<32)|l;}
static void wrmsr(uint32_t n,uint64_t v){__asm__ volatile("wrmsr"::"c"(n),"a"((uint32_t)v),"d"((uint32_t)(v>>32)):"memory");}
static uint32_t lr(const volatile uint32_t *b,uint32_t r){return b[r/4u];}
static void lw(volatile uint32_t *b,uint32_t r,uint32_t v){b[r/4u]=v;(void)b[LAPIC_ID/4u];}
static uint32_t ir(volatile uint32_t *b,uint8_t r){b[IOREGSEL/4u]=r;return b[IOWIN/4u];}
static void iw(volatile uint32_t *b,uint8_t r,uint32_t v){b[IOREGSEL/4u]=r;b[IOWIN/4u]=v;}
int PcApicEnableLocal(pc_apic_mmio *m,uint32_t physical,uint32_t va){
 if(!m||!PcApicSupported()||!physical||!va||(physical&0xfffu)||(va&0xfffu))return 0;
 uint64_t base=rdmsr(APIC_BASE_MSR);
 if(base&(1ull<<10))return 0; /* x2APIC is deliberately unsupported */
 base=(base&~0xfffff000ull)|(uint64_t)physical|(1ull<<11);wrmsr(APIC_BASE_MSR,base);
 volatile uint32_t *b=(volatile uint32_t *)(uintptr_t)va;m->local_apic=b;
 lw(b,LAPIC_TPR,0); 
 /* Mask all local LVT sources before enabling the software APIC. */
 const uint32_t lvt[]={0x320u,0x330u,0x340u,0x350u,0x360u,0x370u};
 for(uint32_t i=0;i<sizeof(lvt)/sizeof(lvt[0]);++i)lw(b,lvt[i],1u<<16);
 lw(b,LAPIC_SVR,(lr(b,LAPIC_SVR)&~0xffu)|0xffu|(1u<<8));return 1;
}
int PcApicAttachIoApics(pc_apic_mmio *m,const pc_acpi_madt_info *a,const uint32_t *va){
 if(!m||!a||!va||!a->ioapic_count||a->ioapic_count>PC_ACPI_MAX_IOAPICS)return 0;
 for(uint32_t i=0;i<a->ioapic_count;++i)if(!va[i]||(va[i]&0xfffu)||!a->ioapics[i].address)return 0;
 for(uint32_t i=0;i<a->ioapic_count;++i){m->ioapic[i]=(volatile uint32_t *)(uintptr_t)va[i];m->ioapic_gsi_base[i]=a->ioapics[i].gsi_base;}
 m->ioapic_count=a->ioapic_count;
 for(uint32_t i=a->ioapic_count;i<PC_ACPI_MAX_IOAPICS;++i){m->ioapic[i]=0;m->ioapic_gsi_base[i]=0;}
 /* Never inherit firmware redirections: mask every pin before routing. */
 if(!PcApicMaskAllIoApics(m)){m->ioapic_count=0;return 0;}
 return 1;
}
int PcApicMaskAllIoApics(pc_apic_mmio *m){
 if(!m||!m->ioapic_count||m->ioapic_count>PC_ACPI_MAX_IOAPICS)return 0;
 for(uint32_t i=0;i<m->ioapic_count;++i){
  volatile uint32_t *io=m->ioapic[i];if(!io)return 0;
  uint32_t max=(ir(io,IOAPIC_VERSION)>>16)&0xffu;
  for(uint32_t pin=0;pin<=max;++pin){
   uint32_t reg=IOAPIC_REDIR+pin*2u;
   if(reg+1u>0xffu)return 0;
   iw(io,(uint8_t)reg,ir(io,(uint8_t)reg)|REDIR_MASK);
  }
 }
 return 1;
}
int PcApicRouteGsi(pc_apic_mmio *m,uint32_t gsi,uint8_t vector,uint16_t flags,int masked){
 if(!m||!m->local_apic||vector<0x20u||m->ioapic_count>PC_ACPI_MAX_IOAPICS)return 0;
 uint32_t pol=flags&3u,trig=(flags>>2)&3u;if(pol==2u||trig==2u)return 0;
 if(!pol)pol=1u;if(!trig)trig=1u; /* ACPI conforming ISA defaults: high/edge */
 uint32_t dest=lr(m->local_apic,LAPIC_ID)&0xff000000u;
 for(uint32_t i=0;i<m->ioapic_count;++i){
  volatile uint32_t *io=m->ioapic[i];if(!io||gsi<m->ioapic_gsi_base[i])continue;
  uint32_t ver=ir(io,IOAPIC_VERSION),max=(ver>>16)&0xffu,pin=gsi-m->ioapic_gsi_base[i];
  if(pin>max)continue;
  uint32_t reg=IOAPIC_REDIR+pin*2u;if(reg+1u>0xffu)return 0;
  uint32_t low=vector;if(pol==3u)low|=1u<<13;if(trig==3u)low|=1u<<15;if(masked)low|=REDIR_MASK;
  iw(io,(uint8_t)reg,low|REDIR_MASK);iw(io,(uint8_t)(reg+1u),dest);iw(io,(uint8_t)reg,low);return 1;
 }
 return 0;
}
void PcApicLocalEoi(const pc_apic_mmio *m){if(m&&m->local_apic)lw(m->local_apic,LAPIC_EOI,0);}
