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

    /* Enable only IRQ0; all other legacy IRQs remain masked. */
    PcHalNativeOut8(PIC1_DATA, 0xfeu);
    PcHalNativeOut8(PIC2_DATA, 0xffu);
    PcHalNativeEnableInterrupts();
}

uint32_t PcIrqTicks(void) { return g_ticks; }

void PcIrqWaitForTicks(uint32_t target)
{
    while (g_ticks < target) __asm__ volatile ("hlt" ::: "memory");
}

void PcIrqStop(void)
{
    PcHalNativeDisableInterrupts();
    PcHalNativeOut8(PIC1_DATA, 0xffu);
    PcHalNativeOut8(PIC2_DATA, 0xffu);
}

void PcIrqDispatch(const uint32_t *r)
{
    uint32_t vector = r[8];
    if (vector == 0x30u) ++g_ticks;
    if (vector >= 0x38u) PcHalNativeOut8(PIC2_CMD, 0x20u);
    PcHalNativeOut8(PIC1_CMD, 0x20u);
}
