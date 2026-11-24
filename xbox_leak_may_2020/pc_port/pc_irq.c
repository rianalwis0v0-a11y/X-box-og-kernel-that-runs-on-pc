#include "pc_irq.h"
#include "pc_hal_native.h"

#define PIC1_CMD 0x20u
#define PIC1_DATA 0x21u
#define PIC2_CMD 0xa0u
#define PIC2_DATA 0xa1u
#define PIT_CH0 0x40u
#define PIT_CMD 0x43u
#define PIT_INPUT_HZ 1193182u
#define PIC_IRQ_COUNT 16u
#define PIC_MASTER_CASCADE_IRQ 2u

typedef struct pc_irq_slot {
    void (*handler)(void *);
    void *context;
} pc_irq_slot;

static volatile uint32_t g_ticks;
static uint32_t g_tick_period_ns;
static uint8_t g_pic_master_mask = 0xffu;
static uint8_t g_pic_slave_mask = 0xffu;
static pc_irq_slot g_handlers[PIC_IRQ_COUNT];
extern uint32_t PcIrqStubTable[];

static uint32_t irq_save_disable(void)
{
    uint32_t flags;
    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static void irq_restore(uint32_t flags)
{
    if (flags & 0x200u) PcHalNativeEnableInterrupts();
}

static void pic_write_masks(void)
{
    /* A slave IRQ requires the master's cascade input to be unmasked. */
    if (g_pic_slave_mask != 0xffu)
        g_pic_master_mask &= (uint8_t)~(1u << PIC_MASTER_CASCADE_IRQ);
    else
        g_pic_master_mask |= (uint8_t)(1u << PIC_MASTER_CASCADE_IRQ);
    PcHalNativeOut8(PIC1_DATA, g_pic_master_mask);
    PcHalNativeOut8(PIC2_DATA, g_pic_slave_mask);
}

void PcIrqInstallPit(uint32_t frequency_hz)
{
    uint32_t divisor, i;
    if (frequency_hz < 19u) frequency_hz = 19u;
    if (frequency_hz > PIT_INPUT_HZ) frequency_hz = PIT_INPUT_HZ;
    divisor = PIT_INPUT_HZ / frequency_hz;
    if (!divisor) divisor = 1u;

    PcHalNativeDisableInterrupts();
    PcHalNativePicRemapMasked();
    PcHalNativeOut8(PIT_CMD, 0x36u);
    PcHalNativeOut8(PIT_CH0, (uint8_t)(divisor & 0xffu));
    PcHalNativeOut8(PIT_CH0, (uint8_t)((divisor >> 8) & 0xffu));

    g_ticks = 0;
    g_tick_period_ns = 1000000000u / (PIT_INPUT_HZ / divisor);
    g_pic_master_mask = 0xffu;
    g_pic_slave_mask = 0xffu;
    for (i = 0; i < PIC_IRQ_COUNT; ++i) {
        g_handlers[i].handler = 0;
        g_handlers[i].context = 0;
    }
    /* Start the clock; all other device IRQs stay masked until registered. */
    g_pic_master_mask &= (uint8_t)~1u;
    pic_write_masks();
    PcHalNativeEnableInterrupts();
}

uint32_t PcIrqTicks(void) { return g_ticks; }

void PcIrqWaitForTicks(uint32_t target)
{
    while ((int32_t)(g_ticks - target) < 0)
        __asm__ volatile ("hlt" ::: "memory");
}

uint32_t PcIrqFrequencyHz(void)
{
    return g_tick_period_ns ? 1000000000u / g_tick_period_ns : 0u;
}

uint64_t PcIrqMonotonicTimeNs(void)
{
    return (uint64_t)g_ticks * g_tick_period_ns;
}

int PcIrqSleepNs(uint64_t duration_ns)
{
    uint64_t start;
    uint32_t flags;
    if (!g_tick_period_ns) return 0;
    __asm__ volatile ("pushfl; popl %0" : "=r"(flags));
    if (!(flags & 0x200u)) return 0;
    start = PcIrqMonotonicTimeNs();
    while (PcIrqMonotonicTimeNs() - start < duration_ns)
        __asm__ volatile ("hlt" ::: "memory");
    return 1;
}

int PcIrqRegisterHandler(uint32_t gsi, uint32_t trigger,
                         void (*handler)(void *), void *context)
{
    uint32_t flags;
    if (gsi >= PIC_IRQ_COUNT || trigger != 0u || !handler)
        return 0; /* This backend currently exposes edge-triggered 8259 IRQs. */
    flags = irq_save_disable();
    g_handlers[gsi].context = context;
    g_handlers[gsi].handler = handler;
    if (gsi < 8u)
        g_pic_master_mask &= (uint8_t)~(1u << gsi);
    else
        g_pic_slave_mask &= (uint8_t)~(1u << (gsi - 8u));
    pic_write_masks();
    irq_restore(flags);
    return 1;
}

int PcIrqMask(uint32_t gsi, int masked)
{
    uint32_t flags;
    if (gsi >= PIC_IRQ_COUNT) return 0;
    flags = irq_save_disable();
    if (gsi < 8u) {
        if (masked) g_pic_master_mask |= (uint8_t)(1u << gsi);
        else g_pic_master_mask &= (uint8_t)~(1u << gsi);
    } else {
        uint32_t bit = 1u << (gsi - 8u);
        if (masked) g_pic_slave_mask |= (uint8_t)bit;
        else g_pic_slave_mask &= (uint8_t)~bit;
    }
    pic_write_masks();
    irq_restore(flags);
    return 1;
}

void PcIrqStop(void)
{
    PcHalNativeDisableInterrupts();
    g_pic_master_mask = 0xffu;
    g_pic_slave_mask = 0xffu;
    pic_write_masks();
    g_tick_period_ns = 0u;
}

void PcIrqDispatch(const uint32_t *r)
{
    uint32_t vector = r[8];
    if (vector >= 0x30u && vector < 0x40u) {
        uint32_t irq = vector - 0x30u;
        if (irq == 0u) ++g_ticks;
        if (g_handlers[irq].handler)
            g_handlers[irq].handler(g_handlers[irq].context);
        /* 8259 requires slave EOI before master EOI for IRQ8-15. */
        if (irq >= 8u) PcHalNativeOut8(PIC2_CMD, 0x20u);
        PcHalNativeOut8(PIC1_CMD, 0x20u);
    }
}
