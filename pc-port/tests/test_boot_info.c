#include "pc_boot_info.h"

#include <assert.h>
#include <stdio.h>

static const struct pc_memory_region valid_map[] = {
    { UINT64_C(0x100000), UINT64_C(0x3F00000), PC_MEMORY_USABLE },
    { UINT64_C(0), UINT64_C(0x100000), PC_MEMORY_RESERVED }
};

static struct pc_boot_info valid_info(void)
{
    struct pc_boot_info info = {0};
    info.magic = PC_BOOT_INFO_MAGIC;
    info.version = PC_BOOT_INFO_VERSION;
    info.memory_map = valid_map;
    info.memory_region_count = sizeof(valid_map) / sizeof(valid_map[0]);
    return info;
}

int main(void)
{
    struct pc_boot_info info = valid_info();

    assert(pc_boot_info_validate(&info));
    assert(!pc_boot_info_validate(NULL));

    info.magic = 0;
    assert(!pc_boot_info_validate(&info));
    info = valid_info();

    info.memory_region_count = 0;
    assert(!pc_boot_info_validate(&info));
    info = valid_info();

    info.has_framebuffer = 1;
    info.framebuffer.address = UINT64_C(0xE0000000);
    info.framebuffer.width = 800;
    info.framebuffer.height = 600;
    info.framebuffer.pitch = 3200;
    info.framebuffer.bits_per_pixel = 32;
    assert(pc_boot_info_validate(&info));

    info.framebuffer.pitch = 100;
    assert(!pc_boot_info_validate(&info));

    info = valid_info();
    {
        const struct pc_memory_region bad_map[] = {
            { UINT64_MAX - 8u, 32u, PC_MEMORY_USABLE }
        };
        info.memory_map = bad_map;
        info.memory_region_count = 1;
        assert(!pc_boot_info_validate(&info));
    }

    puts("pc-port: boot-info validation tests passed");
    return 0;
}
