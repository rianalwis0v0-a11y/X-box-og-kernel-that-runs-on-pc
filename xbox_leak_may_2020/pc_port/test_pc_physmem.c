#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pc_physmem.h"

static uint8_t bitmap[PC_PHYSMEM_BITMAP_BYTES];
static uint8_t mmap_bytes[48];

static void put32(uint32_t o, uint32_t v) {
    mmap_bytes[o]=(uint8_t)v; mmap_bytes[o+1]=(uint8_t)(v>>8);
    mmap_bytes[o+2]=(uint8_t)(v>>16); mmap_bytes[o+3]=(uint8_t)(v>>24);
}
static void put64(uint32_t o, uint64_t v) {
    put32(o,(uint32_t)v); put32(o+4,(uint32_t)(v>>32));
}
static void entry(uint32_t o, uint64_t base, uint64_t length, uint32_t type) {
    put32(o,20); put64(o+4,base); put64(o+12,length); put32(o+20,type);
}
int main(void) {
    pc_physmem memory;
    uint32_t page;
    memset(mmap_bytes,0,sizeof(mmap_bytes));
    entry(0,0x00100000u,0x00400000u,1u); /* 1–5 MiB usable */
    entry(24,0x00200000u,0x00002000u,2u); /* reserved overlap wins */
    assert(pc_physmem_init(&memory,bitmap,sizeof(bitmap),mmap_bytes,48u));
    assert(memory.free_pages == (1024u - 2u));
    assert(pc_physmem_reserve(&memory,0x00101001u,1u));
    assert(memory.free_pages == 1021u);
    page=pc_physmem_alloc_page(&memory);
    assert(page == 0x00102000u);
    assert(memory.free_pages == 1020u);
    assert(pc_physmem_reserve(&memory,0x00102000u,0x1000u));
    assert(memory.free_pages == 1020u); /* already allocated */
    assert(pc_physmem_alloc_page(&memory) == 0x00103000u);

    /* Malformed/truncated map must fail closed and expose no free pages. */
    assert(!pc_physmem_init(&memory,bitmap,sizeof(bitmap),mmap_bytes,47u));
    assert(memory.free_pages == 0u);
    assert(pc_physmem_alloc_page(&memory) == 0u);
    assert(!pc_physmem_reserve(&memory,UINT64_MAX-4u,16u));
    puts("PC-PHYSMEM: reserved-range page allocator tests PASS");
    return 0;
}
