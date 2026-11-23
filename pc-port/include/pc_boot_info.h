#ifndef PC_PORT_BOOT_INFO_H
#define PC_PORT_BOOT_INFO_H

#include <stddef.h>
#include <stdint.h>

#define PC_BOOT_INFO_MAGIC   UINT32_C(0x50434249) /* "PCBI" */
#define PC_BOOT_INFO_VERSION UINT32_C(1)
#define PC_BOOT_MAX_MEMORY_REGIONS 256u

enum pc_memory_kind {
    PC_MEMORY_USABLE = 1,
    PC_MEMORY_RESERVED = 2,
    PC_MEMORY_ACPI_RECLAIMABLE = 3,
    PC_MEMORY_NVS = 4,
    PC_MEMORY_BAD = 5
};

struct pc_memory_region {
    uint64_t base;
    uint64_t length;
    uint32_t kind;
};

struct pc_framebuffer {
    uint64_t address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bits_per_pixel;
};

struct pc_boot_info {
    uint32_t magic;
    uint32_t version;
    const struct pc_memory_region *memory_map;
    size_t memory_region_count;
    uint32_t has_framebuffer;
    struct pc_framebuffer framebuffer;
};

/* Return zero only when the loader supplied a structurally valid contract. */
int pc_boot_info_validate(const struct pc_boot_info *info);

#endif
