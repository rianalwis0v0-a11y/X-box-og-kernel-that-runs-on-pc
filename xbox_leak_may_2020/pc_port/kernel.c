#include <stdint.h>

#define COM1 0x3F8u

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
static void serial_init(void) {
    outb(COM1 + 1, 0x00); /* Disable UART interrupts. */
    outb(COM1 + 3, 0x80); /* Enable divisor latch. */
    outb(COM1 + 0, 0x03); /* 38400 baud divisor. */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03); /* 8N1. */
    outb(COM1 + 2, 0xC7); /* FIFO. */
    outb(COM1 + 4, 0x0B);
}
static void serial_putc(char c) {
    unsigned int timeout = 1000000;
    while (!(inb(COM1 + 5) & 0x20u) && --timeout) { }
    if (timeout) outb(COM1, (uint8_t)c);
}
static void serial_write(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}
void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info) {
    (void)multiboot_info;
    serial_init();
    serial_write("PC-HARNESS: entered 32-bit kernel\n");
    if (multiboot_magic == 0x2BADB002u) {
        serial_write("PC-HARNESS: Multiboot magic PASS\n");
        serial_write("PC-HARNESS: serial PASS\n");
    } else {
        serial_write("PC-HARNESS: ERROR invalid Multiboot magic\n");
    }
    serial_write("PC-HARNESS: halting (Xbox kernel not loaded)\n");
    for (;;) __asm__ volatile ("cli; hlt");
}
