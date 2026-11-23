#ifndef PC_IDT_H
#define PC_IDT_H
#include <stdint.h>
void PcIdtInstallExceptions(void);
void PcIdtExceptionDispatch(const uint32_t *saved_registers);
#endif
