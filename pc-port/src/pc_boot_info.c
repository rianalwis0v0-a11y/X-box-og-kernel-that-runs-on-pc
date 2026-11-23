#include "pc_boot_info.h"

#include <limits.h>

static int add_overflows_u64(uint64_t a, uint64_t b)
{
    return b > UINT64_MAX - a;
}

static int framebuffer_is_valid(const struct pc_framebuffer *fb)
{
    uint64_t minimum_pitch;
    uint64_t required_bytes;

    if (fb->address == 0 || fb->width == 0 || fb->height == 0)
        return 0;
    if (fb->bits_per_pixel != 24 && fb->bits_per_pixel != 32)
        return 0;

    minimum_pitch = ((uint64_t)fb->width * fb->bits_per_pixel + 7u) / 8u;
    if ((uint64_t)fb->pitch < minimum_pitch)
        return 0;

    required_bytes = (uint64_t)fb->pitch * fb->height;
    if (required_bytes > UINT64_MAX - fb->address)
        return 0;

    return 1;
}

int pc_boot_info_validate(const struct pc_boot_info *info)
{
    size_t i;
    int has_usable_memory = 0;

    if (info == NULL || info->magic != PC_BOOT_INFO_MAGIC ||
        info->version != PC_BOOT_INFO_VERSION)
        return 0;

    if (info->memory_map == NULL || info->memory_region_count == 0 ||
        info->memory_region_count > PC_BOOT_MAX_MEMORY_REGIONS)
        return 0;

    for (i = 0; i < info->memory_region_count; ++i) {
        const struct pc_memory_region *region = &info->memory_map[i];

        if (region->length == 0 || add_overflows_u64(region->base, region->length))
            return 0;
        if (region->kind < PC_MEMORY_USABLE || region->kind > PC_MEMORY_BAD)
            return 0;
        if (region->kind == PC_MEMORY_USABLE)
            has_usable_memory = 1;
    }

    if (!has_usable_memory)
        return 0;

    if (info->has_framebuffer && !framebuffer_is_valid(&info->framebuffer))
        return 0;

    return 1;
}
