#include "xbox_pc_boot.h"

#define MB_INFO_MODS       (1u << 3)
#define PE_MACHINE_I386    0x014cu
#define PE32_MAGIC         0x010bu
#define PE_SECTION_EXECUTE 0x20000000u
#define MAX_SECTIONS       96u
#define MAX_MODULE_NAME    64u
#define MAX_MODULES        128u

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

/* Multiboot module strings are untrusted input: never scan without a limit. */
static int name_is_kernel(const char *s) {
    static const char wanted[] = "xboxkrnl.exe";
    uint32_t i;
    if (!s) return 0;
    for (i = 0; i < MAX_MODULE_NAME; ++i) {
        char a = s[i], b = wanted[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a + ('a' - 'A'));
        if (a != b) return 0;
        if (b == 0) return 1;
    }
    return 0;
}

int xbox_pc_validate_pe32_file(const uint8_t *f, uint32_t n,
                         xbox_pc_kernel_image *out) {
    uint32_t pe, opt, sec, i, image_size, headers_size, entry, base;
    uint32_t section_table_size, section_table_end, directory_count;
    uint32_t directory_bytes, section_alignment, file_alignment;
    uint16_t sections, opt_size;
    int entry_executable = 0;

    if (!f || !out || n < 0x40u || rd16(f) != 0x5a4du) return 0;
    pe = rd32(f + 0x3cu);
    if (!span_ok(pe, 24u, n) || rd32(f + pe) != 0x00004550u) return 0;
    if (rd16(f + pe + 4u) != PE_MACHINE_I386) return 0;

    sections = rd16(f + pe + 6u);
    opt_size = rd16(f + pe + 20u);
    if (!sections || sections > MAX_SECTIONS ||
        !span_ok(pe + 24u, opt_size, n) || opt_size < 96u) return 0;

    opt = pe + 24u;
    if (rd16(f + opt) != PE32_MAGIC) return 0;
    directory_count = rd32(f + opt + 92u);
    directory_bytes = opt_size - 96u;
    if (directory_count > directory_bytes / 8u) return 0;
    /*
     * This first loader has no import resolver, TLS callback runner, or
     * delay-import support. Reject these directories rather than allowing
     * an image with unresolved startup dependencies to be entered.
     */
    if (directory_count > 1u &&
        (rd32(f + opt + 96u + 8u) || rd32(f + opt + 96u + 12u)))
        return 0; /* IMAGE_DIRECTORY_ENTRY_IMPORT */
    if (directory_count > 9u &&
        (rd32(f + opt + 96u + 9u * 8u) ||
         rd32(f + opt + 96u + 9u * 8u + 4u)))
        return 0; /* IMAGE_DIRECTORY_ENTRY_TLS */
    if (directory_count > 13u &&
        (rd32(f + opt + 96u + 13u * 8u) ||
         rd32(f + opt + 96u + 13u * 8u + 4u)))
        return 0; /* IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT */
    entry = rd32(f + opt + 16u);
    base = rd32(f + opt + 28u);
    section_alignment = rd32(f + opt + 32u);
    file_alignment = rd32(f + opt + 36u);
    image_size = rd32(f + opt + 56u);
    headers_size = rd32(f + opt + 60u);
    if (!image_size || !headers_size || headers_size > n ||
        headers_size > image_size || !entry || entry >= image_size ||
        !section_alignment || !file_alignment ||
        (section_alignment & (section_alignment - 1u)) ||
        (file_alignment & (file_alignment - 1u)) ||
        file_alignment < 0x200u || file_alignment > 0x10000u ||
        section_alignment < file_alignment ||
        (section_alignment < 0x1000u &&
         section_alignment != file_alignment) ||
        (image_size & (section_alignment - 1u)) ||
        (headers_size & (file_alignment - 1u))) return 0;

    sec = opt + opt_size;
    section_table_size = (uint32_t)sections * 40u;
    if (!span_ok(sec, section_table_size, n)) return 0;
    section_table_end = sec + section_table_size;
    if (section_table_end > headers_size) return 0;

    for (i = 0; i < sections; ++i) {
        const uint8_t *s = f + sec + i * 40u;
        uint32_t vsize = rd32(s + 8u), va = rd32(s + 12u);
        uint32_t raw_size = rd32(s + 16u), raw = rd32(s + 20u);
        uint32_t characteristics = rd32(s + 36u);
        uint32_t extent = vsize > raw_size ? vsize : raw_size;

        uint32_t j;
        if (raw_size &&
            (raw < headers_size || !span_ok(raw, raw_size, n) ||
             (raw & (file_alignment - 1u)) ||
             (raw_size & (file_alignment - 1u)))) return 0;
        if (extent && (va > image_size || extent > image_size - va ||
                       (va & (section_alignment - 1u)) ||
                       va < headers_size)) return 0;
        for (j = 0; j < i; ++j) {
            const uint8_t *p = f + sec + j * 40u;
            uint32_t pva = rd32(p + 12u), pv = rd32(p + 8u);
            uint32_t pr = rd32(p + 20u), prs = rd32(p + 16u);
            uint32_t pext = pv > prs ? pv : prs;
            if (extent && pext && va < pva + pext && pva < va + extent)
                return 0;
            if (raw_size && prs && raw < pr + prs && pr < raw + raw_size)
                return 0;
        }
        if (entry >= va && entry - va < extent &&
            (characteristics & PE_SECTION_EXECUTE))
            entry_executable = 1;
    }
    if (!entry_executable) return 0;

    out->file = f;
    out->file_size = n;
    out->preferred_base = base;
    out->entry_rva = entry;
    out->image_size = image_size;
    out->headers_size = headers_size;
    out->section_count = sections;
    return 1;
}


/*
 * Validate every relocation before changing any target. The directory must
 * reside wholly inside raw-backed bytes of one mapped section.
 */
static int process_relocations(uint8_t *image, uint32_t image_size,
                               uint32_t reloc_rva, uint32_t reloc_size,
                               uint32_t delta, int apply) {
    uint32_t cursor = 0;
    while (cursor < reloc_size) {
        uint32_t block_rva, block_size, count, j;
        if (reloc_size - cursor < 8u) return 0;
        block_rva = rd32(image + reloc_rva + cursor);
        block_size = rd32(image + reloc_rva + cursor + 4u);
        if (block_size < 8u || block_size > reloc_size - cursor ||
            ((block_size - 8u) & 1u) != 0 ||
            block_rva >= image_size || (block_rva & 0xfffu) != 0)
            return 0;
        count = (block_size - 8u) / 2u;
        for (j = 0; j < count; ++j) {
            uint16_t item = rd16(image + reloc_rva + cursor + 8u + j * 2u);
            uint16_t type = item >> 12;
            uint32_t target, value;
            if (type == 0u) continue;
            if (type != 3u || block_rva > UINT32_MAX - (item & 0x0fffu))
                return 0;
            target = block_rva + (item & 0x0fffu);
            if (!span_ok(target, 4u, image_size)) return 0;
            if (apply) {
                value = rd32(image + target) + delta;
                image[target] = (uint8_t)value;
                image[target + 1u] = (uint8_t)(value >> 8);
                image[target + 2u] = (uint8_t)(value >> 16);
                image[target + 3u] = (uint8_t)(value >> 24);
            }
        }
        cursor += block_size;
    }
    return cursor == reloc_size;
}

static int rva_range_is_raw_backed(const uint8_t *f, uint32_t sec,
                                   uint16_t sections, uint32_t rva,
                                   uint32_t length) {
    uint32_t i;
    for (i = 0; i < sections; ++i) {
        const uint8_t *s = f + sec + i * 40u;
        uint32_t va = rd32(s + 12u), raw_size = rd32(s + 16u);
        if (rva >= va && rva - va <= raw_size &&
            length <= raw_size - (rva - va))
            return 1;
    }
    return 0;
}

/*
 * Copy headers/sections to a caller-owned image buffer and apply PE32
 * IMAGE_REL_BASED_HIGHLOW relocations. Unsupported relocation types fail
 * closed. This is deliberately not an executable-memory allocator.
 */
int xbox_pc_load_pe32_image(const xbox_pc_kernel_image *image,
                            uint8_t *destination, uint32_t destination_size,
                            uint32_t load_base, uint32_t *entry_address) {
    const uint8_t *f;
    uint32_t pe, opt, sec, i, delta, reloc_rva, reloc_size;
    uint16_t sections, opt_size;
    xbox_pc_kernel_image checked;
    if (!image || !image->file || !destination || !entry_address ||
        destination_size < image->image_size ||
        image->image_size > UINT32_MAX - load_base ||
        image->entry_rva >= image->image_size ||
        !xbox_pc_validate_pe32_file(image->file, image->file_size, &checked) ||
        checked.preferred_base != image->preferred_base ||
        checked.entry_rva != image->entry_rva ||
        checked.image_size != image->image_size ||
        checked.headers_size != image->headers_size ||
        checked.section_count != image->section_count)
        return 0;
    f = image->file;
    if (image->file_size < 0x40u) return 0;
    pe = rd32(f + 0x3cu);
    if (!span_ok(pe, 24u, image->file_size)) return 0;
    opt = pe + 24u;
    opt_size = rd16(f + pe + 20u);
    if (!span_ok(opt, opt_size, image->file_size) || opt_size < 96u)
        return 0;
    sections = rd16(f + pe + 6u);
    sec = opt + opt_size;
    if (!sections || sections != image->section_count ||
        !span_ok(sec, (uint32_t)sections * 40u, image->file_size))
        return 0;

    for (i = 0; i < image->image_size; ++i) destination[i] = 0;
    for (i = 0; i < image->headers_size; ++i) destination[i] = f[i];
    for (i = 0; i < sections; ++i) {
        const uint8_t *s = f + sec + i * 40u;
        uint32_t va = rd32(s + 12u), raw_size = rd32(s + 16u);
        uint32_t raw = rd32(s + 20u);
        uint32_t vsize = rd32(s + 8u);
        uint32_t extent = vsize > raw_size ? vsize : raw_size;
        uint32_t j;
        if ((raw_size && (!span_ok(raw, raw_size, image->file_size) ||
                          !span_ok(va, raw_size, image->image_size))) ||
            (extent && !span_ok(va, extent, image->image_size)))
            return 0;
        for (j = 0; j < raw_size; ++j)
            destination[va + j] = f[raw + j];
    }

    delta = load_base - image->preferred_base;
    if (delta) {
        uint32_t directory_offset = 96u + 5u * 8u;
        if (opt_size < directory_offset + 8u) return 0;
        reloc_rva = rd32(f + opt + directory_offset);
        reloc_size = rd32(f + opt + directory_offset + 4u);
        if (!reloc_rva || !reloc_size ||
            !span_ok(reloc_rva, reloc_size, image->image_size))
            return 0;
        if (!rva_range_is_raw_backed(f, sec, sections, reloc_rva,
                                     reloc_size))
            return 0;
        /* First pass is validation-only: malformed later blocks cannot leave
           earlier relocation targets partially modified. */
        if (!process_relocations(destination, image->image_size, reloc_rva,
                                 reloc_size, delta, 0))
            return 0;
        if (!process_relocations(destination, image->image_size, reloc_rva,
                                 reloc_size, delta, 1))
            return 0;
    }
    *entry_address = load_base + image->entry_rva;
    return 1;
}

int xbox_pc_find_kernel_module(uint32_t magic, uint32_t info_address,
                               xbox_pc_kernel_image *out) {
    const mb_info_min *info;
    const mb_module *mods;
    uint32_t i, module_bytes;

    if (!out || magic != XBOX_PC_MULTIBOOT_MAGIC || !info_address)
        return XBOX_PC_BOOT_BAD_HANDOFF;
    info = (const mb_info_min *)(uintptr_t)info_address;
    if (!(info->flags & MB_INFO_MODS) || !info->mods_addr ||
        !info->mods_count || info->mods_count > MAX_MODULES)
        return XBOX_PC_BOOT_NO_KERNEL;

    module_bytes = info->mods_count * (uint32_t)sizeof(mb_module);
    if (info->mods_addr > UINT32_MAX - module_bytes)
        return XBOX_PC_BOOT_BAD_HANDOFF;
    mods = (const mb_module *)(uintptr_t)info->mods_addr;

    for (i = 0; i < info->mods_count; ++i) {
        uint32_t size;
        if (mods[i].end <= mods[i].start || !mods[i].string) continue;
        size = mods[i].end - mods[i].start;
        if (!name_is_kernel((const char *)(uintptr_t)mods[i].string))
            continue;
        if (!xbox_pc_validate_pe32_file((const uint8_t *)(uintptr_t)mods[i].start,
                           size, out))
            return XBOX_PC_BOOT_BAD_PE;
        return XBOX_PC_BOOT_OK;
    }
    return XBOX_PC_BOOT_NO_KERNEL;
}
