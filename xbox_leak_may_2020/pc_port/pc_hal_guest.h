#ifndef PC_PORT_PC_HAL_GUEST_H
#define PC_PORT_PC_HAL_GUEST_H

#include <stddef.h>
#include <stdint.h>

/*
 * The PC boot handoff must map the monitor's registered shared-memory window
 * into the guest and call PcHalBindSharedWindow before HalInitSystemPhase0.
 * The pointer is guest-local; only the numeric shared-window offset crosses
 * VMCALL. Never pass this pointer to the host.
 */
void PcHalBindSharedWindow(volatile void *mapping, size_t mapping_bytes,
                           uint64_t call_record_offset);
int PcHalInitializePlatform(void);
int PcHalRegisterInterrupt(uint32_t vector, uint32_t flags);

#endif
