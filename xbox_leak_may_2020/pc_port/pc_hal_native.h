#ifndef PC_HAL_NATIVE_H
#define PC_HAL_NATIVE_H

/*
 * Bare-metal x86 primitives for the PC HAL port.
 * These routines execute in ring 0. They are not a VMCALL/guest interface.
 */
#include <stdint.h>

#define PC_HAL_PIC_MASTER_VECTOR 0x30u
#define PC_HAL_PIC_SLAVE_VECTOR  0x38u

uint8_t PcHalNativeIn8(uint16_t port);
void PcHalNativeOut8(uint16_t port, uint8_t value);
void PcHalNativeDisableInterrupts(void);
void PcHalNativeEnableInterrupts(void);

/* Remap the dual 8259 PICs and leave every IRQ masked. */
void PcHalNativePicRemapMasked(void);

/*
 * PCI Configuration Mechanism #1 (CF8/CFC).
 * Invalid BDF/alignment reads return all-ones; invalid writes are ignored.
 * These functions preserve the caller's IF state around each config cycle.
 */
uint8_t PcHalNativePciRead8(uint8_t bus, uint8_t device,
                            uint8_t function, uint8_t offset);
uint16_t PcHalNativePciRead16(uint8_t bus, uint8_t device,
                              uint8_t function, uint8_t offset);
uint32_t PcHalNativePciRead32(uint8_t bus, uint8_t device,
                              uint8_t function, uint8_t offset);
void PcHalNativePciWrite8(uint8_t bus, uint8_t device, uint8_t function,
                          uint8_t offset, uint8_t value);
void PcHalNativePciWrite16(uint8_t bus, uint8_t device, uint8_t function,
                           uint8_t offset, uint16_t value);
void PcHalNativePciWrite32(uint8_t bus, uint8_t device, uint8_t function,
                           uint8_t offset, uint32_t value);

#endif
