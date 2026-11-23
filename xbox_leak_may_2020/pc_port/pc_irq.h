#ifndef PC_IRQ_H
#define PC_IRQ_H
#include <stdint.h>
void PcIrqInstallPit(uint32_t frequency_hz);
uint32_t PcIrqTicks(void);
void PcIrqWaitForTicks(uint32_t target);
void PcIrqStop(void);
void PcIrqDispatch(const uint32_t *saved_registers);
#endif
