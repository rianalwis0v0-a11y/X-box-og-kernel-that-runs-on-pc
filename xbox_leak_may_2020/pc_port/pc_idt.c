#include <stdint.h>
#include "pc_idt.h"

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_high;
} PcIdtGate;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} PcIdtr;

static PcIdtGate g_idt[256];
extern uint32_t PcIsrStubTable[];

static void PcIdtSetGate(uint32_t vector, uintptr_t handler)
{
    g_idt[vector].offset_low = (uint16_t)(handler & 0xffffu);
    g_idt[vector].selector = 0x08u;
    g_idt[vector].zero = 0;
    g_idt[vector].type_attr = 0x8eu;
    g_idt[vector].offset_high = (uint16_t)((handler >> 16) & 0xffffu);
}

void PcIdtInstallExceptions(void)
{
    PcIdtr idtr;
    uint32_t i;
    for (i = 0; i < 256u; ++i) {
        g_idt[i].offset_low = 0;
        g_idt[i].selector = 0;
        g_idt[i].zero = 0;
        g_idt[i].type_attr = 0;
        g_idt[i].offset_high = 0;
    }
    for (i = 0; i < 32u; ++i)
        PcIdtSetGate(i, (uintptr_t)PcIsrStubTable[i]);
    idtr.limit = (uint16_t)(sizeof(g_idt) - 1u);
    idtr.base = (uint32_t)(uintptr_t)g_idt;
    __asm__ volatile ("lidt %0" : : "m"(idtr) : "memory");
}

void PcIdtExceptionDispatch(const uint32_t *saved_registers)
{
    (void)saved_registers;
    /* Fatal CPU exceptions are deliberately fail-stop; never resume blindly. */
    __asm__ volatile ("cli" ::: "memory");
    for (;;) __asm__ volatile ("hlt");
}
