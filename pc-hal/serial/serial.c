#include "serial.h"

/* Standalone, clean-room 16550-compatible COM1 console for x86.
 * Call only in privileged execution with I/O-port access enabled.
 * This module is intentionally independent of any Xbox kernel code.
 */
#if defined(__i386__) || defined(__x86_64__)
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
#else
#error "PC HAL serial console requires an x86 target"
#endif

#define COM1 0x3F8u

void pc_hal_serial_init(void) {
    outb(COM1 + 1, 0x00); /* Disable UART interrupts */
    outb(COM1 + 3, 0x80); /* Enable divisor latch */
    outb(COM1 + 0, 0x03); /* Divisor low: 38400 baud */
    outb(COM1 + 1, 0x00); /* Divisor high */
    outb(COM1 + 3, 0x03); /* 8N1 */
    outb(COM1 + 2, 0xC7); /* FIFO */
    outb(COM1 + 4, 0x0B); /* DTR, RTS, OUT2 */
}

void pc_hal_serial_putc(char c) {
    /* Bounded wait: avoid hanging forever if UART is absent. */
    unsigned int spins = 1000000u;
    while (spins-- && !(inb(COM1 + 5) & 0x20u)) {
        __asm__ volatile ("pause");
    }
    if (spins) {
        outb(COM1, (uint8_t)c);
    }
}

void pc_hal_serial_write(const char *s) {
    if (!s) return;
    while (*s) {
        if (*s == '\n') pc_hal_serial_putc('\r');
        pc_hal_serial_putc(*s++);
    }
}

void pc_hal_serial_write_hex32(uint32_t value) {
    static const char digits[] = "0123456789ABCDEF";
    pc_hal_serial_write("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        pc_hal_serial_putc(digits[(value >> shift) & 0xFu]);
    }
}
