#include "pc_apic.h"

#define IA32_APIC_BASE_MSR 0x1bu
#define APIC_REG_ID 0x020u
#define APIC_REG_TPR 0x080u
#define APIC_REG_EOI 0x0b0u
#define APIC_REG_SVR 0x0f0u
#define APIC_REG_LVT_TIMER 0x320u
#define APIC_REG_LVT_THERMAL 0x330u
#define APIC_REG_LVT_PERF 0x340u
#define APIC_REG_LVT_LINT0 0x350u
#define APIC_REG_LVT_LINT1 0x360u
#define APIC_REG_LVT_ERROR 0x370u
#define APIC_SVR_ENABLE (1u << 8)
#define IOAPIC_REG_SELECT 0x00u
#define IOAPIC_REG_WINDOW 0x10u
#define IOAPIC_REG_VERSION 0x01u
#define IOAPIC_REG_REDIR_BASE 0x10u
#define IOAPIC_REDIR_MASK (1u << 16)

#if defined(__i386__) || defined(__x86_64__)
static uint64_t read_msr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static void write_msr(uint32_t msr, uint64_t value)
{
    uint32_t lo = (uint32_t)value, hi = (uint32_t)(value >> 32);
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi) : "memory");
}
#else
#error "APIC support requires x86"
#endif

static uint32_t lapic_read(const volatile uint32_t *base, uint32_t reg)
{
    return base[reg / sizeof(uint32_t)];
}
static void lapic_write(volatile uint32_t *base, uint32_t reg, uint32_t value)
{
    base[reg / sizeof(uint32_t)] = value;
    (void)base[APIC_REG_ID / sizeof(uint32_t)];
}
static uint32_t ioapic_read(volatile uint32_t *base, uint8_t reg)
{
    base[IOAPIC_REG_SELECT / 4u] = reg;
    return base[IOAPIC_REG_WINDOW / 4u];
}
static void ioapic_write(volatile uint32_t *base, uint8_t reg, uint32_t value)
{
    base[IOAPIC_REG_SELECT / 4u] = reg;
    base[IOAPIC_REG_WINDOW / 4u] = value;
}

int PcApicEnableLocal(pc_apic_mmio *mmio, uint32_t local_apic_virtual)
{
    uint64_t apic_base;
    volatile uint32_t *base;
    if (!mmio || !local_apic_virtual || (local_apic_virtual & 0xfffu))
        return 0;
    apic_base = read_msr(IA32_APIC_BASE_MSR);
    if ((apic_base & (1ull << 10)) != 0u) return 0; /* x2APIC needs a distinct driver */
    apic_base &= ~0xfffff000ull;
    apic_base |= (uint64_t)local_apic_virtual;
    apic_base |= (1ull << 11);
    write_msr(IA32_APIC_BASE_MSR, apic_base);

    base = (volatile uint32_t *)(uintptr_t)local_apic_virtual;
    mmio->local_apic = base;
    lapic_write(base, APIC_REG_TPR, 0u);
    lapic_write(base, APIC_REG_LVT_TIMER, 1u << 16);
    lapic_write(base, APIC_REG_LVT_THERMAL, 1u << 16);
    lapic_write(base, APIC_REG_LVT_PERF, 1u << 16);
    lapic_write(base, APIC_REG_LVT_LINT0, 1u << 16);
    lapic_write(base, APIC_REG_LVT_LINT1, 1u << 16);
    lapic_write(base, APIC_REG_LVT_ERROR, 1u << 16);
    lapic_write(base, APIC_REG_SVR,
                (lapic_read(base, APIC_REG_SVR) & ~0xffu) |
                0xffu | APIC_SVR_ENABLE);
    return 1;
}

int PcApicAttachIoApics(pc_apic_mmio *mmio,
                        const pc_acpi_madt_info *madt,
                        const uint32_t *ioapic_virtual)
{
    uint32_t i;
    if (!mmio || !madt || !ioapic_virtual ||
        madt->ioapic_count == 0u || madt->ioapic_count > PC_ACPI_MAX_IOAPICS)
        return 0;
    for (i = 0; i < madt->ioapic_count; ++i) {
        if (!ioapic_virtual[i] || (ioapic_virtual[i] & 0xfffu)) return 0;
        mmio->ioapic[i] = (volatile uint32_t *)(uintptr_t)ioapic_virtual[i];
    }
    for (; i < PC_ACPI_MAX_IOAPICS; ++i) mmio->ioapic[i] = 0;
    return 1;
}

int PcApicRouteGsi(pc_apic_mmio *mmio, uint32_t gsi, uint8_t vector,
                   uint16_t acpi_flags, int masked)
{
    uint32_t i;
    uint32_t polarity = acpi_flags & 3u;
    uint32_t trigger = (acpi_flags >> 2) & 3u;
    uint32_t apic_id;
    if (!mmio || !mmio->local_apic || vector < 0x20u ||
        polarity == 2u || trigger == 2u) return 0;
    apic_id = lapic_read(mmio->local_apic, APIC_REG_ID) >> 24;
    for (i = 0; i < PC_ACPI_MAX_IOAPICS; ++i) {
        volatile uint32_t *io = mmio->ioapic[i];
        uint32_t version, max_redir, low, high, reg, pin;
        uint32_t low_flags = 0;
        if (!io) continue;
        version = ioapic_read(io, IOAPIC_REG_VERSION);
        max_redir = (version >> 16) & 0xffu;
        /* The IOAPIC's GSI base comes from the MADT; caller ordering is stable. */
        (void)max_redir;
        (void)low_flags;
        (void)low;
        (void)high;
        (void)reg;
        (void)pin;
    }
    return 0;
}

void PcApicLocalEoi(const pc_apic_mmio *mmio)
{
    if (mmio && mmio->local_apic)
        lapic_write(mmio->local_apic, APIC_REG_EOI, 0u);
}
