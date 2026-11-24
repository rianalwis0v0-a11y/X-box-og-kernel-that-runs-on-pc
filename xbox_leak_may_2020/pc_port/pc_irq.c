#include "pc_irq.h"
#include "pc_hal_native.h"

#define PIC1_CMD 0x20u
#define PIC1_DATA 0x21u
#define PIC2_CMD 0xa0u
#define PIC2_DATA 0xa1u
#define PIT_CH0 0x40u
#define PIT_CMD 0x43u
#define PIT_INPUT_HZ 1193182u

static volatile uint32_t g_ticks;
static uint32_t g_tick_period_ns;
static uint8_t g_pic_master_mask = 0xffu;
static void (*g_irq0_handler)(void *);
static void *g_irq0_context;
extern uint32_t PcIrqStubTable[];

void PcIrqInstallPit(uint32_t frequency_hz)
{
    uint32_t divisor;
    if (frequency_hz < 19u) frequency_hz = 19u;
    if (frequency_hz > PIT_INPUT_HZ) frequency_hz = PIT_INPUT_HZ;
    divisor = PIT_INPUT_HZ / frequency_hz;
    if (!divisor) divisor = 1u;

    /*
     * Reuse the native HAL's single PIC initialization path. It remaps the
     * 8259s to the Xbox-compatible 0x30/0x38 vectors and leaves all IRQs
     * masked while the IDT is prepared.
     */
    PcHalNativeDisableInterrupts();
    PcHalNativePicRemapMasked();

    /* PIT channel 0, lobyte/hibyte, mode 3, binary. */
    PcHalNativeOut8(PIT_CMD, 0x36u);
    PcHalNativeOut8(PIT_CH0, (uint8_t)(divisor & 0xffu));
    PcHalNativeOut8(PIT_CH0, (uint8_t)((divisor >> 8) & 0xffu));
    g_ticks = 0;
    g_tick_period_ns = 1000000000u / frequency_hz;
    g_irq0_handler = 0;
    g_irq0_context = 0;

    /* Enable only IRQ0; all other legacy IRQs remain masked. */
    g_pic_master_mask = 0xfeu;
    PcHalNativeOut8(PIC1_DATA, g_pic_master_mask);
    PcHalNativeOut8(PIC2_DATA, 0xffu);
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
    if (!g_tick_period_ns) return 0;
    start = PcIrqMonotonicTimeNs();
    while (PcIrqMonotonicTimeNs() - start < duration_ns)
        __asm__ volatile ("hlt" ::: "memory");
    return 1;
}

int PcIrqRegisterHandler(uint32_t gsi, uint32_t trigger,
                         void (*handler)(void *), void *context)
{
    if (gsi != 0u || trigger != 0u || !handler) return 0;
    PcHalNativeDisableInterrupts();
    g_irq0_context = context;
    g_irq0_handler = handler;
    PcHalNativeEnableInterrupts();
    return 1;
}

int PcIrqMask(uint32_t gsi, int masked)
{
    if (gsi != 0u) return 0;
    if (masked) g_pic_master_mask |= 0x01u;
    else g_pic_master_mask &= (uint8_t)~0x01u;
    PcHalNativeOut8(PIC1_DATA, g_pic_master_mask);
    return 1;
}

void PcIrqStop(void)
{
    PcHalNativeDisableInterrupts();
    g_pic_master_mask = 0xffu;
    PcHalNativeOut8(PIC1_DATA, g_pic_master_mask);
    PcHalNativeOut8(PIC2_DATA, 0xffu);
}

void PcIrqDispatch(const uint32_t *r)
{
    uint32_t vector = r[8];
    if (vector == 0x30u) {
        ++g_ticks;
        if (g_irq0_handler) g_irq0_handler(g_irq0_context);
    }
    if (vector >= 0x38u) PcHalNativeOut8(PIC2_CMD, 0x20u);
    PcHalNativeOut8(PIC1_CMD, 0x20u);
}
