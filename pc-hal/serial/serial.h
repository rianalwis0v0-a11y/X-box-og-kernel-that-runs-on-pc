#ifndef PC_HAL_SERIAL_H
#define PC_HAL_SERIAL_H

#include <stdint.h>

void pc_hal_serial_init(void);
void pc_hal_serial_putc(char c);
void pc_hal_serial_write(const char *s);
void pc_hal_serial_write_hex32(uint32_t value);

#endif
