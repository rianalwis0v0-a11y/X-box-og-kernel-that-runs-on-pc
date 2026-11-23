#include <stdint.h>
#include "pc_idt.h"
typedef struct __attribute__((packed)) { uint16_t offset_low, selector; uint8_t zero, type_attr; uint16_t offset_high; } PcIdtGate;
typedef struct __attribute__((packed)) { uint16_t limit; uint32_t base; } PcIdtr;
static PcIdtGate g_idt[256];
extern uint32_t PcIsrStubTable[];
static void PcSerialPutc(char c) {
    volatile uint8_t *lsr=(volatile uint8_t *)0x3fdu, *data=(volatile uint8_t *)0x3f8u;
    uint32_t timeout=1000000u;
    while (((*lsr)&0x20u)==0u && --timeout) { }
    if(timeout) *data=(uint8_t)c;
}
static void PcSerialText(const char *s) { while(*s) { if(*s=='\n') PcSerialPutc('\r'); PcSerialPutc(*s++); } }
static void PcSerialHex(uint32_t v) {
    static const char d[]="0123456789ABCDEF"; int shift;
    for(shift=28;shift>=0;shift-=4) PcSerialPutc(d[(v>>shift)&15u]);
}
static void PcIdtSetGate(uint32_t v, uintptr_t h) {
    g_idt[v].offset_low=(uint16_t)(h&0xffffu); g_idt[v].selector=0x08u;
    g_idt[v].zero=0; g_idt[v].type_attr=0x8eu;
    g_idt[v].offset_high=(uint16_t)((h>>16)&0xffffu);
}
void PcIdtInstallExceptions(void) {
    PcIdtr idtr; uint32_t i;
    for(i=0;i<256u;++i) { g_idt[i].offset_low=0; g_idt[i].selector=0; g_idt[i].zero=0; g_idt[i].type_attr=0; g_idt[i].offset_high=0; }
    for(i=0;i<32u;++i) PcIdtSetGate(i,(uintptr_t)PcIsrStubTable[i]);
    idtr.limit=(uint16_t)(sizeof(g_idt)-1u); idtr.base=(uint32_t)(uintptr_t)g_idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr) : "memory");
}
void PcIdtExceptionDispatch(const uint32_t *r) {
    /* pushal frame: edi,esi,ebp,oldesp,ebx,edx,ecx,eax,vector,error,eip,cs,eflags */
    uint32_t vector=r[8], error=r[9], eip=r[10];
    __asm__ volatile ("cli" ::: "memory");
    PcSerialText("\nPC-HAL: FATAL CPU EXCEPTION vector=0x"); PcSerialHex(vector);
    PcSerialText(" error=0x"); PcSerialHex(error);
    PcSerialText(" eip=0x"); PcSerialHex(eip);
    PcSerialText("\nPC-HAL: halted; exception is not recoverable\n");
    for(;;) __asm__ volatile ("hlt");
}
