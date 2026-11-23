#ifndef PC_PCI_H
#define PC_PCI_H

#include <stdint.h>

#define PC_PCI_MAX_BARS 6u

typedef enum pc_pci_bar_kind {
    PC_PCI_BAR_IO = 1,
    PC_PCI_BAR_MEMORY = 2
} pc_pci_bar_kind;

typedef struct pc_pci_bar {
    uint64_t base;
    uint8_t kind;
    uint8_t width;       /* 32 or 64 for memory; 32 for I/O */
    uint8_t prefetchable;/* meaningful for memory BARs */
    uint8_t slot;        /* original BAR register index */
    uint8_t registers;   /* 1, or 2 for a 64-bit BAR */
} pc_pci_bar;

/*
 * Decode already-assigned PCI BAR values. This is deliberately read-only:
 * it never writes all-ones into BARs to size them or changes device decode.
 * Returns the number of decoded BARs, or -1 for invalid arguments/encoding.
 * Zero-valued BAR registers are treated as unassigned and omitted.
 */
int PcPciDecodeBars(const uint32_t *raw_bars, uint8_t bar_count,
                    pc_pci_bar *decoded, uint8_t decoded_capacity);

#endif
