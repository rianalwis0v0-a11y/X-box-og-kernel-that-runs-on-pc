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
#define PIC_OCW3_READ_ISR 0x0bu

typedef struct pc_irq_slot {
    void (*handler)(void *);
    void *context;
} pc_irq_slot;

static volatile uint32_t g_ticks;
static uint32_t g_pit_divisor;
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
    /* Preserve the caller's IF state; never enable interrupts accidentally. */
    if (flags & 0x200u) PcHalNativeEnableInterrupts();
}

static void pic_write_masks(void)
{
    /* IRQ2 is the physical cascade; it follows whether any slave line is live. */
    if (g_pic_slave_mask != 0xffu)
        g_pic_master_mask &= (uint8_t)~(1u << PIC_MASTER_CASCADE_IRQ);
    else
        g_pic_master_mask |= (uint8_t)(1u << PIC_MASTER_CASCADE_IRQ);
    PcHalNativeOut8(PIC1_DATA, g_pic_master_mask);
    PcHalNativeOut8(PIC2_DATA, g_pic_slave_mask);
}

void PcIrqInstallPit(uint32_t frequency_hz)
{
    uint32_t divisor, i, flags;
    if (frequency_hz < 19u) frequency_hz = 19u;
    if (frequency_hz > PIT_INPUT_HZ) frequency_hz = PIT_INPUT_HZ;
    divisor = PIT_INPUT_HZ / frequency_hz;
    if (!divisor) divisor = 1u;
    if (divisor > 65535u) divisor = 65535u;

    flags = irq_save_disable();
    PcHalNativePicRemapMasked();
    PcHalNativeOut8(PIT_CMD, 0x36u);
    PcHalNativeOut8(PIT_CH0, (uint8_t)(divisor & 0xffu));
    PcHalNativeOut8(PIT_CH0, (uint8_t)((divisor >> 8) & 0xffu));

    g_ticks = 0;
    g_pit_divisor = divisor;
    g_pic_master_mask = 0xffu;
    g_pic_slave_mask = 0xffu;
    for (i = 0; i < PIC_IRQ_COUNT; ++i) {
        g_handlers[i].handler = 0;
        g_handlers[i].context = 0;
    }
    /* Start the clock; all other device IRQs stay masked until registered. */
    g_pic_master_mask &= (uint8_t)~1u;
    pic_write_masks();
    irq_restore(flags);
}

uint32_t PcIrqTicks(void) { return g_ticks; }

void PcIrqWaitForTicks(uint32_t target)
{
    while ((int32_t)(g_ticks - target) < 0)
        __asm__ volatile ("hlt" ::: "memory");
}

uint32_t PcIrqFrequencyHz(void)
{
    return g_pit_divisor ? PIT_INPUT_HZ / g_pit_divisor : 0u;
}

uint64_t PcIrqMonotonicTimeNs(void)
{
    if (!g_pit_divisor) return 0u;
    /* Keep the fractional PIT period; avoid accumulating rounded-ns drift. */
    return ((uint64_t)g_ticks * (uint64_t)g_pit_divisor * 1000000000ull) /
           PIT_INPUT_HZ;
}

int PcIrqSleepNs(uint64_t duration_ns)
{
    uint64_t start;
    uint32_t flags;
    if (!g_pit_divisor) return 0;
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
    /* IRQ2 is reserved for the 8259 master/slave cascade. */
    if (gsi >= PIC_IRQ_COUNT || gsi == PIC_MASTER_CASCADE_IRQ ||
        trigger != 0u || !handler)
        return 0;
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
    if (gsi >= PIC_IRQ_COUNT || gsi == PIC_MASTER_CASCADE_IRQ) return 0;
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
    g_pit_divisor = 0u;
}

/* Read the in-service register to distinguish real IRQ7/IRQ15 from spurious IRQs. */
static int pic_irq_is_spurious(uint32_t irq)
{
    uint8_t isr;
    if (irq == 7u) {
        PcHalNativeOut8(PIC1_CMD, PIC_OCW3_READ_ISR);
        isr = PcHalNativeIn8(PIC1_CMD);
        return (isr & 0x80u) == 0u;
    }
    if (irq == 15u) {
        PcHalNativeOut8(PIC2_CMD, PIC_OCW3_READ_ISR);
        isr = PcHalNativeIn8(PIC2_CMD);
        if ((isr & 0x80u) == 0u) {
            /* A spurious slave interrupt still requires EOI to the master. */
            PcHalNativeOut8(PIC1_CMD, 0x20u);
            return 1;
        }
    }
    return 0;
}

void PcIrqDispatch(const uint32_t *r)
{
    uint32_t vector = r[8];
    if (vector >= 0x30u && vector < 0x40u) {
        uint32_t irq = vector - 0x30u;
        if (pic_irq_is_spurious(irq)) return;
        if (irq == 0u) ++g_ticks;
        if (g_handlers[irq].handler)
            g_handlers[irq].handler(g_handlers[irq].context);
        /* 8259 requires slave EOI before master EOI for IRQ8-15. */
        if (irq >= 8u) PcHalNativeOut8(PIC2_CMD, 0x20u);
        PcHalNativeOut8(PIC1_CMD, 0x20u);
    }
}
