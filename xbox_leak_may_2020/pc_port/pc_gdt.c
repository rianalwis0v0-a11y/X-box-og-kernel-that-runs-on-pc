#include <stdint.h>
#include "pc_gdt.h"
typedef struct __attribute__((packed)) { uint16_t limit; uint32_t base; } PcGdtr;
static uint64_t g_gdt[3] __attribute__((aligned(8))) = {
    0x0000000000000000ULL,
    0x00cf9a000000ffffULL,
    0x00cf92000000ffffULL
};
void PcGdtInstallFlatSegments(void)
{
    PcGdtr gdtr;
    gdtr.limit=(uint16_t)(sizeof(g_gdt)-1u);
    gdtr.base=(uint32_t)(uintptr_t)g_gdt;
    __asm__ volatile (
        "lgdt %0\n\t"
        "ljmp $0x08, $1f\n"
        "1:\n\t"
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        : : "m"(gdtr) : "eax", "memory");
}
