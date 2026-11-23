#include "pc_irq.h"
#define PIC1_CMD 0x20u
#define PIC1_DATA 0x21u
#define PIC2_CMD 0xa0u
#define PIC2_DATA 0xa1u
#define PIT_CH0 0x40u
#define PIT_CMD 0x43u
#define PIT_INPUT_HZ 1193182u
static volatile uint32_t g_ticks;
extern uint32_t PcIrqStubTable[];
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
static void io_wait(void) { outb(0x80u, 0u); }
void PcIrqInstallPit(uint32_t frequency_hz)
{
    uint32_t divisor;
    if (frequency_hz < 19u) frequency_hz = 19u;
    if (frequency_hz > PIT_INPUT_HZ) frequency_hz = PIT_INPUT_HZ;
    divisor = PIT_INPUT_HZ / frequency_hz;
    if (!divisor) divisor = 1u;
    __asm__ volatile ("cli" ::: "memory");
    /* ICW1: initialize cascaded 8259 PICs. */
    outb(PIC1_DATA, 0xffu); outb(PIC2_DATA, 0xffu);
    outb(PIC1_CMD, 0x11u); io_wait();
    outb(PIC2_CMD, 0x11u); io_wait();
    outb(PIC1_DATA, 0x30u); io_wait();
    outb(PIC2_DATA, 0x38u); io_wait();
    outb(PIC1_DATA, 0x04u); io_wait();
    outb(PIC2_DATA, 0x02u); io_wait();
    outb(PIC1_DATA, 0x01u); io_wait();
    outb(PIC2_DATA, 0x01u); io_wait();
    /* PIT channel 0, lobyte/hibyte, mode 3, binary. */
    outb(PIT_CMD, 0x36u);
    outb(PIT_CH0, (uint8_t)(divisor & 0xffu));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xffu));
    g_ticks = 0;
    /* Only IRQ0 is enabled; all other legacy IRQs stay masked. */
    outb(PIC1_DATA, 0xfeu);
    outb(PIC2_DATA, 0xffu);
    __asm__ volatile ("sti" ::: "memory");
}
uint32_t PcIrqTicks(void) { return g_ticks; }
void PcIrqWaitForTicks(uint32_t target)
{
    while (g_ticks < target) __asm__ volatile ("hlt" ::: "memory");
}
void PcIrqStop(void)
{
    __asm__ volatile ("cli" ::: "memory");
    outb(PIC1_DATA, 0xffu);
    outb(PIC2_DATA, 0xffu);
}
void PcIrqDispatch(const uint32_t *r)
{
    uint32_t vector = r[8];
    if (vector == 0x30u) ++g_ticks;
    if (vector >= 0x38u) outb(PIC2_CMD, 0x20u);
    outb(PIC1_CMD, 0x20u);
}
