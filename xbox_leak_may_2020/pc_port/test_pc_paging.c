#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "pc_paging.h"

static uint32_t directory[PC_PAGING_ENTRIES] __attribute__((aligned(4096)));
static uint32_t tables[4 * PC_PAGING_ENTRIES] __attribute__((aligned(4096)));
static pc_paging_context paging;

int main(void) {
    uint32_t physical, flags;
    assert(pc_paging_init(&paging, directory, 0x00100000u,
                          tables, 0x00101000u, 4u));
    assert(pc_paging_map(&paging, 0xc0000000u, 0x00200000u,
                         0x3000u, PC_PAGING_WRITE));
    assert(pc_paging_translate(&paging, 0xc0001234u, &physical, &flags));
    assert(physical == 0x00201234u);
    assert(flags == PC_PAGING_WRITE);
    assert(pc_paging_map(&paging, 0xc0003000u, 0x00203000u,
                         0x1000u, PC_PAGING_USER));
    assert(directory[0x300] & PC_PAGING_USER);
    assert(pc_paging_translate(&paging, 0xc0003000u, &physical, &flags));
    assert(physical == 0x00203000u && flags == PC_PAGING_USER);

    /* Device MMIO pages can be mapped with cache-disable attributes. */
    assert(pc_paging_map(&paging, 0xc0400000u, 0xfec00000u,
                         0x1000u, PC_PAGING_WRITE |
                         PC_PAGING_PCD | PC_PAGING_PWT));
    assert(pc_paging_translate(&paging, 0xc0400123u, &physical, &flags));
    assert(physical == 0xfec00123u);
    assert(flags == (PC_PAGING_WRITE | PC_PAGING_PCD | PC_PAGING_PWT));
    assert(!pc_paging_map(&paging, 0xc0000000u, 0x00300000u,
                          0x1000u, PC_PAGING_WRITE));
    assert(!pc_paging_map(&paging, 0xc0000001u, 0x00300000u,
                          0x1000u, PC_PAGING_WRITE));
    assert(!pc_paging_map(&paging, 0xc0000000u, 0x00300000u,
                          0x1001u, PC_PAGING_WRITE));
    assert(!pc_paging_map(&paging, 0xfffff000u, 0x00300000u,
                          0x2000u, PC_PAGING_WRITE));
    {
        uint32_t mmio_base = 0u, mmio_length = 0u;
        /* Unaligned device BAR ranges are rounded to complete UC pages. */
        assert(pc_paging_map_mmio_identity(&paging, 0xfec00020ULL, 0x80u,
                                           &mmio_base, &mmio_length));
        assert(mmio_base == 0xfec00000u && mmio_length == 0x1000u);
        assert(pc_paging_translate(&paging, 0xfec00020u, &physical, &flags));
        assert(physical == 0xfec00020u);
        assert(flags == (PC_PAGING_WRITE | PC_PAGING_PCD | PC_PAGING_PWT));

        /* A range crossing a 4 KiB boundary maps both pages. */
        assert(pc_paging_map_mmio_identity(&paging, 0xfed00ff0ULL, 0x30u,
                                           &mmio_base, &mmio_length));
        assert(mmio_base == 0xfed00000u && mmio_length == 0x2000u);
        assert(pc_paging_translate(&paging, 0xfed01010u, &physical, &flags));
        assert(physical == 0xfed01010u);
        assert(flags == (PC_PAGING_WRITE | PC_PAGING_PCD | PC_PAGING_PWT));

        assert(!pc_paging_map_mmio_identity(&paging, 0x100000000ULL, 0x1000u,
                                            &mmio_base, &mmio_length));
        assert(!pc_paging_map_mmio_identity(&paging, 0xfffffff0ULL, 0x40u,
                                            &mmio_base, &mmio_length));
        assert(!pc_paging_map_mmio_identity(&paging, 0xfec00000ULL, 0u,
                                            &mmio_base, &mmio_length));
    }
    assert(!pc_paging_translate(&paging, 0x1000u, &physical, &flags));

    /* A corrupted public context must fail closed, not index past storage. */
    paging.table_count = paging.table_capacity + 1u;
    assert(!pc_paging_map(&paging, 0x40000000u, 0x00400000u,
                          0x1000u, PC_PAGING_WRITE));
    assert(!pc_paging_translate(&paging, 0xc0000000u, &physical, &flags));

    assert(!pc_paging_init(&paging, directory, 0x00100000u,
                           tables, 0x00100800u, 4u));
    assert(!pc_paging_init(&paging, directory, 0x00100000u,
                           directory, 0x00200000u, 4u));

    assert(pc_paging_init(&paging, directory, 0x00100000u,
                          tables, 0x00101000u, 1u));
    assert(!pc_paging_map(&paging, 0x00000000u, 0x00000000u,
                          0x00800000u, PC_PAGING_WRITE));
    assert(paging.table_count == 0u); /* Failed preflight is atomic. */
    puts("PC-PAGING: 4 KiB page-table builder tests PASS");
    return 0;
}
