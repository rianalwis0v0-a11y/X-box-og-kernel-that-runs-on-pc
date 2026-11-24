#include "pc_intel_gpu.h"

/* Conservative, well-known desktop/mobile Intel graphics PCI IDs.
 * Families intentionally map to architecture generations, not exact SKUs. */
pc_intel_gpu_generation PcIntelGpuClassifyDevice(uint16_t id)
{
    switch (id) {
    /* Gen2 */
    case 0x2562: case 0x2572: case 0x3582:
        return PC_INTEL_GEN2;
    /* Gen3 */
    case 0x2582: case 0x2592: case 0x2772: case 0x27a2:
        return PC_INTEL_GEN3;
    /* Gen4 */
    case 0x2a02: case 0x2a12: case 0x2e02: case 0x2e22:
        return PC_INTEL_GEN4;
    /* Gen5 */
    case 0x0042: case 0x0046: case 0x004a:
        return PC_INTEL_GEN5;
    /* Gen6 */
    case 0x0102: case 0x0112: case 0x0122: case 0x0106:
        return PC_INTEL_GEN6;
    /* Gen7 / Ivy Bridge */
    case 0x0152: case 0x0162: case 0x0156: case 0x0166:
        return PC_INTEL_GEN7;
    /* Gen7.5 / Haswell */
    case 0x0402: case 0x0412: case 0x0422: case 0x0a16:
        return PC_INTEL_GEN7;
    /* Gen8 / Broadwell */
    case 0x1602: case 0x1612: case 0x1622: case 0x1616:
        return PC_INTEL_GEN8;
    /* Gen9 / Skylake, Kaby Lake, Coffee Lake */
    case 0x1902: case 0x1912: case 0x1926: case 0x5912:
    case 0x5916: case 0x3e92: case 0x3e9b:
        return PC_INTEL_GEN9;
    /* Gen9.5 / UHD 610 commonly 3E91; keep family in Gen9 bucket. */
    case 0x3e91: case 0x3e90: case 0x3ea1: case 0x9bc8: case 0x9ba8:
        return PC_INTEL_GEN9;
    /* Gen11 / Ice Lake */
    case 0x8a52: case 0x8a5a: case 0x8a56:
        return PC_INTEL_GEN11;
    /* Gen12 / Xe-LP and later integrated display IDs */
    case 0x9a49: case 0x9a40: case 0x9a60:
    case 0x46a6: case 0x46a8: case 0x4680:
    case 0xa7a0: case 0xa720: case 0x7d55:
        return PC_INTEL_GEN12;
    default:
        return PC_INTEL_GEN_UNKNOWN;
    }
}

int PcIntelGpuBuildInfo(uint16_t vendor, uint16_t device,
                        uint8_t revision, uint32_t class_register,
                        const uint32_t bars[PC_PCI_MAX_BARS],
                        pc_intel_gpu_info *output)
{
    int count;
    pc_intel_gpu_info empty = {0};
    if (!bars || !output || vendor != PC_INTEL_PCI_VENDOR_ID)
        return 0;
    if (((class_register >> 24) & 0xffu) != PC_INTEL_CLASS_DISPLAY ||
        ((class_register >> 16) & 0xffu) != PC_INTEL_SUBCLASS_VGA)
        return 0;

    *output = empty;
    output->vendor_id = vendor;
    output->device_id = device;
    output->revision = revision;
    output->programming_interface = (uint8_t)(class_register >> 8);
    output->subclass = (uint8_t)(class_register >> 16);
    output->class_code = (uint8_t)(class_register >> 24);
    output->generation = (uint8_t)PcIntelGpuClassifyDevice(device);

    count = PcPciDecodeBars(bars, PC_PCI_MAX_BARS, output->resources,
                            PC_INTEL_MAX_RESOURCES);
    if (count < 0) {
        *output = empty;
        return 0;
    }
    output->resource_count = (uint8_t)count;
    return 1;
}
