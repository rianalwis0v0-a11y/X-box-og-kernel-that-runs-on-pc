#include "pc_paging.h"

#include <stddef.h>

#define X86_PAGE_ADDRESS_MASK 0xfffff000u
#define X86_PDE_ADDRESS_MASK  0xfffff000u

static int aligned_page(uint32_t value) {
    return (value & (PC_PAGING_PAGE_SIZE - 1u)) == 0;
}

int pc_paging_init(pc_paging_context *c,
                   uint32_t *directory, uint32_t directory_physical,
                   uint32_t *tables, uint32_t tables_physical,
                   uint32_t table_capacity) {
    uint32_t i;
    uint32_t table_bytes;
    if (!c || !directory || !tables || !table_capacity ||
        table_capacity > PC_PAGING_ENTRIES ||
        !aligned_page(directory_physical) || !aligned_page(tables_physical) ||
        ((uintptr_t)directory & (PC_PAGING_PAGE_SIZE - 1u)) ||
        ((uintptr_t)tables & (PC_PAGING_PAGE_SIZE - 1u)))
        return 0;
    if (table_capacity > UINT32_MAX / PC_PAGING_PAGE_SIZE) return 0;
    table_bytes = table_capacity * PC_PAGING_PAGE_SIZE;
    if (tables_physical > UINT32_MAX - table_bytes ||
        directory_physical > UINT32_MAX - PC_PAGING_PAGE_SIZE)
        return 0;
    if (directory_physical < tables_physical + table_bytes &&
        tables_physical < directory_physical + PC_PAGING_PAGE_SIZE)
        return 0;

    c->directory = directory;
    c->directory_physical = directory_physical;
    c->tables = tables;
    c->tables_physical = tables_physical;
    c->table_capacity = table_capacity;
    c->table_count = 0;
    for (i = 0; i < PC_PAGING_ENTRIES; ++i) {
        c->directory[i] = 0;
        c->table_slot[i] = -1;
    }
    for (i = 0; i < table_capacity * PC_PAGING_ENTRIES; ++i)
        c->tables[i] = 0;
    return 1;
}

int pc_paging_map(pc_paging_context *c,
                  uint32_t virtual_address, uint32_t physical_address,
                  uint32_t length, uint32_t flags) {
    uint8_t needed[PC_PAGING_ENTRIES] = {0};
    uint32_t pages, i, needed_count = 0;
    if (!c || !c->directory || !c->tables || !length ||
        !aligned_page(virtual_address) || !aligned_page(physical_address) ||
        !aligned_page(length) || (flags & ~PC_PAGING_FLAGS_MASK))
        return 0;
    if (virtual_address > UINT32_MAX - length ||
        physical_address > UINT32_MAX - length)
        return 0;
    pages = length / PC_PAGING_PAGE_SIZE;

    /* Preflight the complete range so conflicts/capacity failures are atomic. */
    for (i = 0; i < pages; ++i) {
        uint32_t va = virtual_address + i * PC_PAGING_PAGE_SIZE;
        uint32_t pa = physical_address + i * PC_PAGING_PAGE_SIZE;
        uint32_t pdi = va >> 22;
        uint32_t pti = (va >> 12) & 0x3ffu;
        int16_t slot = c->table_slot[pdi];
        if (slot < 0) {
            if (!needed[pdi]) {
                needed[pdi] = 1;
                ++needed_count;
            }
        } else {
            uint32_t old = c->tables[(uint32_t)slot * PC_PAGING_ENTRIES + pti];
            if ((old & PC_PAGING_PRESENT) &&
                (old & X86_PAGE_ADDRESS_MASK) != pa)
                return 0;
            if ((old & PC_PAGING_PRESENT) &&
                (old & PC_PAGING_FLAGS_MASK) != flags)
                return 0;
        }
    }
    if (needed_count > c->table_capacity - c->table_count) return 0;

    for (i = 0; i < PC_PAGING_ENTRIES; ++i) {
        if (needed[i]) {
            uint32_t slot = c->table_count++;
            uint32_t *table = c->tables + slot * PC_PAGING_ENTRIES;
            uint32_t j;
            for (j = 0; j < PC_PAGING_ENTRIES; ++j) table[j] = 0;
            c->table_slot[i] = (int16_t)slot;
            c->directory[i] = (c->tables_physical +
                               slot * PC_PAGING_PAGE_SIZE) |
                              PC_PAGING_PRESENT | PC_PAGING_WRITE;
        }
    }
    for (i = 0; i < pages; ++i) {
        uint32_t va = virtual_address + i * PC_PAGING_PAGE_SIZE;
        uint32_t pa = physical_address + i * PC_PAGING_PAGE_SIZE;
        uint32_t pdi = va >> 22;
        uint32_t pti = (va >> 12) & 0x3ffu;
        uint32_t slot = (uint32_t)c->table_slot[pdi];
        uint32_t *pte = &c->tables[slot * PC_PAGING_ENTRIES + pti];
        *pte = pa | PC_PAGING_PRESENT | flags;
        if (flags & PC_PAGING_USER) c->directory[pdi] |= PC_PAGING_USER;
    }
    return 1;
}

int pc_paging_translate(const pc_paging_context *c,
                        uint32_t virtual_address, uint32_t *physical_address,
                        uint32_t *flags) {
    uint32_t pdi, pti, pte;
    int16_t slot;
    if (!c || !c->directory || !physical_address) return 0;
    pdi = virtual_address >> 22;
    pti = (virtual_address >> 12) & 0x3ffu;
    slot = c->table_slot[pdi];
    if (slot < 0) return 0;
    pte = c->tables[(uint32_t)slot * PC_PAGING_ENTRIES + pti];
    if (!(pte & PC_PAGING_PRESENT)) return 0;
    *physical_address = (pte & X86_PAGE_ADDRESS_MASK) |
                        (virtual_address & (PC_PAGING_PAGE_SIZE - 1u));
    if (flags) *flags = pte & PC_PAGING_FLAGS_MASK;
    return 1;
}
