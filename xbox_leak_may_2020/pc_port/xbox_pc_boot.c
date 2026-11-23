#include "xbox_pc_boot.h"

#define MB_INFO_MODS       (1u << 3)
#define MB_INFO_MMAP       (1u << 6)
#define MAX_SECTIONS       96u
#define PE_MACHINE_I386    0x014cu
#define PE32_MAGIC         0x010bu

typedef struct mb_info_min {
    uint32_t flags, mem_lower, mem_upper, boot_device, cmdline;
    uint32_t mods_count, mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length, mmap_addr;
} mb_info_min;

typedef struct mb_module {
    uint32_t start, end, string, reserved;
} mb_module;

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int span_ok(uint32_t offset, uint32_t length, uint32_t total) {
    return offset <= total && length <= total - offset;
}
static int name_is_kernel(const char *s) {
    static const char wanted[] = "xboxkrnl.exe";
    uint32_t i;
    if (!s) return 0;
    for (i = 0; i < sizeof(wanted); ++i) {
        char a = s[i], b = wanted[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a + ('a' - 'A'));
        if (a != b) return 0;
        if (b == 0) return 1;
    }
    return 0;
}
static int validate_pe32(const uint8_t *f, uint32_t n,
                         xbox_pc_kernel_image *out) {
    uint32_t pe, opt, sec, i, image_size, headers_size, entry, base;
    uint16_t sections, opt_size;
    if (!f || n < 0x40u || rd16(f) != 0x5A4Du) return 0;
    pe = rd32(f + 0x3c);
    if (!span_ok(pe, 24u, n) || rd32(f + pe) != 0x00004550u) return 0;
    if (rd16(f + pe + 4) != PE_MACHINE_I386) return 0;
    sections = rd16(f + pe + 6);
    opt_size = rd16(f + pe + 20);
    if (!sections || sections > MAX_SECTIONS ||
        !span_ok(pe + 24u, opt_size, n) || opt_size < 96u) return 0;
    opt = pe + 24u;
    if (rd16(f + opt) != PE32_MAGIC || opt_size < 96u) return 0;
    entry = rd32(f + opt + 16u);
    base = rd32(f + opt + 28u);
    image_size = rd32(f + opt + 56u);
    headers_size = rd32(f + opt + 60u);
    if (!image_size || !headers_size || headers_size > n ||
        headers_size > image_size || entry >= image_size) return 0;
    sec = opt + opt_size;
    if (!span_ok(sec, (uint32_t)sections * 40u, n)) return 0;
    for (i = 0; i < sections; ++i) {
        const uint8_t *s = f + sec + i * 40u;
        uint32_t vsize = rd32(s + 8u), va = rd32(s + 12u);
        uint32_t raw_size = rd32(s + 16u), raw = rd32(s + 20u);
        uint32_t extent = vsize > raw_size ? vsize : raw_size;
        if (raw_size && !span_ok(raw, raw_size, n)) return 0;
        if (extent && (va > image_size || extent > image_size - va)) return 0;
    }
    out->file = f;
    out->file_size = n;
    out->preferred_base = base;
    out->entry_rva = entry;
    out->image_size = image_size;
    out->headers_size = headers_size;
    out->section_count = sections;
    return 1;
}

int xbox_pc_find_kernel_module(uint32_t magic, uint32_t info_address,
                               xbox_pc_kernel_image *out) {
    const mb_info_min *info;
    const mb_module *mods;
    uint32_t i;
    if (!out || magic != XBOX_PC_MULTIBOOT_MAGIC || !info_address)
        return XBOX_PC_BOOT_BAD_HANDOFF;
    info = (const mb_info_min *)(uintptr_t)info_address;
    if (!(info->flags & MB_INFO_MODS) || !info->mods_addr ||
        !info->mods_count || info->mods_count > 128u)
        return XBOX_PC_BOOT_NO_KERNEL;
    mods = (const mb_module *)(uintptr_t)info->mods_addr;
    for (i = 0; i < info->mods_count; ++i) {
        uint32_t size;
        if (mods[i].end <= mods[i].start) continue;
        size = mods[i].end - mods[i].start;
        if (!name_is_kernel((const char *)(uintptr_t)mods[i].string)) continue;
        if (!validate_pe32((const uint8_t *)(uintptr_t)mods[i].start, size, out))
            return XBOX_PC_BOOT_BAD_PE;
        return XBOX_PC_BOOT_OK;
    }
    return XBOX_PC_BOOT_NO_KERNEL;
}
