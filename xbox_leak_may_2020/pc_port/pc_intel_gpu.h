#ifndef PC_INTEL_GPU_H
#define PC_INTEL_GPU_H

#include <stdint.h>
#include "pc_pci.h"

/* Intel display controller discovery only. This is not a command-submission
 * backend; unknown device IDs are reported as Intel/unknown and never touched. */
#define PC_INTEL_PCI_VENDOR_ID 0x8086u
#define PC_INTEL_CLASS_DISPLAY 0x03u
#define PC_INTEL_SUBCLASS_VGA  0x00u
#define PC_INTEL_MAX_RESOURCES PC_PCI_MAX_BARS

typedef enum pc_intel_gpu_generation {
    PC_INTEL_GEN_UNKNOWN = 0,
    PC_INTEL_GEN2 = 2,
    PC_INTEL_GEN3 = 3,
    PC_INTEL_GEN4 = 4,
    PC_INTEL_GEN5 = 5,
    PC_INTEL_GEN6 = 6,
    PC_INTEL_GEN7 = 7,
    PC_INTEL_GEN8 = 8,
    PC_INTEL_GEN9 = 9,
    PC_INTEL_GEN11 = 11,
    PC_INTEL_GEN12 = 12
} pc_intel_gpu_generation;

typedef struct pc_intel_gpu_info {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t revision;
    uint8_t programming_interface;
    uint8_t subclass;
    uint8_t class_code;
    uint8_t generation;
    uint8_t resource_count;
    pc_pci_bar resources[PC_INTEL_MAX_RESOURCES];
} pc_intel_gpu_info;

/* Classifies known PCI device IDs conservatively. Returns UNKNOWN for
 * unrecognised IDs; it never guesses a generation from marketing names. */
pc_intel_gpu_generation PcIntelGpuClassifyDevice(uint16_t device_id);

/* Decode a standard PCI config header (six BAR dwords). */
int PcIntelGpuBuildInfo(uint16_t vendor_id, uint16_t device_id,
                        uint8_t revision, uint32_t class_register,
                        const uint32_t bars[PC_PCI_MAX_BARS],
                        pc_intel_gpu_info *output);

#endif
