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
    assert(!pc_paging_map(&paging, 0xc0000000u, 0x00300000u,
                          0x1000u, PC_PAGING_WRITE));
    assert(!pc_paging_map(&paging, 0xc0000001u, 0x00300000u,
                          0x1000u, PC_PAGING_WRITE));
    assert(!pc_paging_map(&paging, 0xc0000000u, 0x00300000u,
                          0x1001u, PC_PAGING_WRITE));
    assert(!pc_paging_map(&paging, 0xfffff000u, 0x00300000u,
                          0x2000u, PC_PAGING_WRITE));
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
