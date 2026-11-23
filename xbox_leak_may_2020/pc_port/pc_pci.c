#include "pc_pci.h"

int PcPciDecodeBars(const uint32_t *raw_bars, uint8_t bar_count,
                    pc_pci_bar *decoded, uint8_t decoded_capacity)
{
    uint8_t slot = 0;
    uint8_t count = 0;

    if (!raw_bars || !decoded || !bar_count ||
        bar_count > PC_PCI_MAX_BARS || decoded_capacity > PC_PCI_MAX_BARS)
        return -1;

    while (slot < bar_count) {
        uint32_t low = raw_bars[slot];
        pc_pci_bar bar;

        if (low == 0u) {
            ++slot;
            continue;
        }

        bar.base = 0;
        bar.kind = 0;
        bar.width = 32;
        bar.prefetchable = 0;
        bar.slot = slot;
        bar.registers = 1;

        if (low & 1u) {
            bar.kind = PC_PCI_BAR_IO;
            bar.base = (uint64_t)(low & ~3u);
        } else {
            uint32_t memory_type = (low >> 1) & 3u;
            bar.kind = PC_PCI_BAR_MEMORY;
            bar.prefetchable = (uint8_t)((low >> 3) & 1u);
            if (memory_type == 0u) {
                bar.base = (uint64_t)(low & ~0x0fu);
            } else if (memory_type == 1u) {
                /* Legacy  below-1-MiB memory BAR. */
                bar.base = (uint64_t)(low & ~0x0fu);
            } else if (memory_type == 2u) {
                uint32_t high;
                if ((uint8_t)(slot + 1u) >= bar_count)
                    return -1;
                high = raw_bars[slot + 1u];
                bar.base = ((uint64_t)high << 32) |
                           (uint64_t)(low & ~0x0fu);
                bar.width = 64;
                bar.registers = 2;
            } else {
                /* Reserved memory BAR encoding. */
                return -1;
            }
        }

        if (count >= decoded_capacity)
            return -1;
        decoded[count++] = bar;
        slot = (uint8_t)(slot + bar.registers);
    }

    return (int)count;
}
