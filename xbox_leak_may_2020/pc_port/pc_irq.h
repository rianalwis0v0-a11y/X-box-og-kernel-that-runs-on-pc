#ifndef PC_IRQ_H
#define PC_IRQ_H
#include <stdint.h>
void PcIrqInstallPit(uint32_t frequency_hz);
uint32_t PcIrqTicks(void);
uint32_t PcIrqFrequencyHz(void);
uint64_t PcIrqMonotonicTimeNs(void);
int PcIrqSleepNs(uint64_t duration_ns);
int PcIrqRegisterHandler(uint32_t gsi, uint32_t trigger,
                         void (*handler)(void *), void *context);
int PcIrqMask(uint32_t gsi, int masked);
void PcIrqWaitForTicks(uint32_t target);
void PcIrqStop(void);
void PcIrqDispatch(const uint32_t *saved_registers);
#endif
