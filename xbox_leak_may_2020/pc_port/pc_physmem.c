#include "pc_physmem.h"

#define PHYS_LIMIT 0x100000000ULL

static uint32_t read32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t read64(const uint8_t *p) {
    return (uint64_t)read32(p) | ((uint64_t)read32(p + 4) << 32);
}
static int bit_get(const pc_physmem *m, uint32_t page) {
    return (m->bitmap[page >> 3] >> (page & 7u)) & 1u;
}
static void bit_set(pc_physmem *m, uint32_t page, int unavailable) {
    uint8_t mask = (uint8_t)(1u << (page & 7u));
    uint8_t *byte = &m->bitmap[page >> 3];
    int old = ((*byte & mask) != 0);
    if (old == unavailable) return;
    if (unavailable) {
        *byte |= mask;
        --m->free_pages;
    } else {
        *byte &= (uint8_t)~mask;
        ++m->free_pages;
    }
}
static void mark_pages(pc_physmem *m, uint64_t first, uint64_t end,
                       int unavailable) {
    uint32_t p;
    if (first >= PHYS_LIMIT || end <= first) return;
    if (end > PHYS_LIMIT) end = PHYS_LIMIT;
    p = (uint32_t)(first / PC_PHYSMEM_PAGE_SIZE);
    while ((uint64_t)p * PC_PHYSMEM_PAGE_SIZE < end &&
           p < PC_PHYSMEM_PAGE_COUNT) {
        bit_set(m, p, unavailable);
        ++p;
    }
}
static int walk_map(const uint8_t *bytes, uint32_t length,
                    pc_physmem *m, int reserved_pass) {
    uint32_t offset = 0;
    while (offset < length) {
        uint32_t payload, step, type;
        uint64_t base, size, end;
        if (length - offset < 4u) return 0;
        payload = read32(bytes + offset);
        if (payload < 20u || payload > UINT32_MAX - 4u) return 0;
        step = payload + 4u;
        if (step > length - offset) return 0;
        base = read64(bytes + offset + 4u);
        size = read64(bytes + offset + 12u);
        type = read32(bytes + offset + 20u);
        if (size && base > UINT64_MAX - size) return 0;
        if (size) {
            end = base + size;
            /*
             * This allocator only represents physical addresses below 4 GiB.
             * Clamp before page rounding so a high 64-bit region cannot wrap
             * its aligned start back to page zero.
             */
            if (base < PHYS_LIMIT) {
                if (end > PHYS_LIMIT) end = PHYS_LIMIT;
                if (!reserved_pass && type == 1u) {
                    /* Only whole pages inside usable RAM are candidates. */
                    uint64_t first = (base + PC_PHYSMEM_PAGE_SIZE - 1u) &
                                     ~(uint64_t)(PC_PHYSMEM_PAGE_SIZE - 1u);
                    uint64_t last = end &
                                    ~(uint64_t)(PC_PHYSMEM_PAGE_SIZE - 1u);
                    mark_pages(m, first, last, 0);
                } else if (reserved_pass && type != 1u) {
                    uint64_t first = base &
                                     ~(uint64_t)(PC_PHYSMEM_PAGE_SIZE - 1u);
                    uint64_t last = (end + PC_PHYSMEM_PAGE_SIZE - 1u) &
                                    ~(uint64_t)(PC_PHYSMEM_PAGE_SIZE - 1u);
                    mark_pages(m, first, last, 1);
                }
            }
        }
        offset += step;
    }
    return offset == length;
}

int pc_physmem_init(pc_physmem *m, uint8_t *bitmap,
                    uint32_t bitmap_bytes, const void *mmap,
                    uint32_t mmap_bytes) {
    uint32_t i;
    if (!m || !bitmap || bitmap_bytes < PC_PHYSMEM_BITMAP_BYTES ||
        !mmap || !mmap_bytes) return 0;
    m->bitmap = bitmap;
    m->bitmap_bytes = bitmap_bytes;
    m->free_pages = 0;
    m->search_hint = 0;
    for (i = 0; i < PC_PHYSMEM_BITMAP_BYTES; ++i) bitmap[i] = 0xffu;
    if (!walk_map((const uint8_t *)mmap, mmap_bytes, m, 0) ||
        !walk_map((const uint8_t *)mmap, mmap_bytes, m, 1)) {
        for (i = 0; i < PC_PHYSMEM_BITMAP_BYTES; ++i) bitmap[i] = 0xffu;
        m->free_pages = 0;
        return 0;
    }
    /* Never hand physical address zero to a caller as an allocated page. */
    bit_set(m, 0u, 1);
    return 1;
}

int pc_physmem_reserve(pc_physmem *m, uint64_t base, uint64_t length) {
    uint64_t end, last;
    if (!m || !m->bitmap || m->bitmap_bytes < PC_PHYSMEM_BITMAP_BYTES ||
        !length || base > UINT64_MAX - length)
        return 0;
    end = base + length;
    if (base >= PHYS_LIMIT) return 1;
    if (end > PHYS_LIMIT) end = PHYS_LIMIT;
    last = (end + PC_PHYSMEM_PAGE_SIZE - 1u) &
           ~(uint64_t)(PC_PHYSMEM_PAGE_SIZE - 1u);
    mark_pages(m, base & ~(uint64_t)(PC_PHYSMEM_PAGE_SIZE - 1u),
               last, 1);
    if (m->search_hint >= PC_PHYSMEM_PAGE_COUNT)
        m->search_hint = 0;
    return 1;
}

uint32_t pc_physmem_alloc_page(pc_physmem *m) {
    uint32_t i, start;
    if (!m || !m->bitmap || !m->free_pages ||
        m->bitmap_bytes < PC_PHYSMEM_BITMAP_BYTES)
        return 0;
    start = m->search_hint < PC_PHYSMEM_PAGE_COUNT ? m->search_hint : 0;
    for (i = start; i < PC_PHYSMEM_PAGE_COUNT; ++i) {
        if (!bit_get(m, i)) {
            bit_set(m, i, 1);
            m->search_hint = i + 1u;
            return i * PC_PHYSMEM_PAGE_SIZE;
        }
    }
    for (i = 0; i < start; ++i) {
        if (!bit_get(m, i)) {
            bit_set(m, i, 1);
            m->search_hint = i + 1u;
            return i * PC_PHYSMEM_PAGE_SIZE;
        }
    }
    return 0;
}


uint32_t pc_physmem_alloc_pages(pc_physmem *m, uint32_t page_count,
                                uint32_t alignment_pages) {
    uint32_t pass, pass_start;
    if (!m || !m->bitmap || !page_count || !alignment_pages ||
        (alignment_pages & (alignment_pages - 1u)) ||
        page_count > m->free_pages ||
        m->bitmap_bytes < PC_PHYSMEM_BITMAP_BYTES ||
        page_count > PC_PHYSMEM_PAGE_COUNT)
        return 0;

    for (pass = 0; pass < 2u; ++pass) {
        uint32_t candidate, limit;
        pass_start = pass == 0u && m->search_hint < PC_PHYSMEM_PAGE_COUNT
                   ? m->search_hint : 1u;
        limit = pass == 0u ? PC_PHYSMEM_PAGE_COUNT : m->search_hint;
        if (pass_start < 1u) pass_start = 1u;
        candidate = (pass_start + alignment_pages - 1u) &
                    ~(alignment_pages - 1u);
        while (candidate <= limit && page_count <= limit - candidate) {
            uint32_t i;
            for (i = 0; i < page_count && !bit_get(m, candidate + i); ++i) { }
            if (i == page_count) {
                uint32_t base = candidate * PC_PHYSMEM_PAGE_SIZE;
                for (i = 0; i < page_count; ++i)
                    bit_set(m, candidate + i, 1);
                m->search_hint = candidate + page_count;
                return base;
            }
            candidate = (candidate + i + 1u + alignment_pages - 1u) &
                        ~(alignment_pages - 1u);
        }
    }
    return 0;
}
