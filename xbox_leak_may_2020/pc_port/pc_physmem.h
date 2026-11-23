#ifndef PC_PHYSMEM_H
#define PC_PHYSMEM_H

#include <stdint.h>

#define PC_PHYSMEM_PAGE_SIZE 4096u
#define PC_PHYSMEM_PAGE_COUNT 1048576u
#define PC_PHYSMEM_BITMAP_BYTES (PC_PHYSMEM_PAGE_COUNT / 8u)

/* One bit per 4 KiB page below 4 GiB: 1 = unavailable, 0 = allocatable. */
typedef struct pc_physmem {
    uint8_t *bitmap;
    uint32_t bitmap_bytes;
    uint32_t free_pages;
    uint32_t search_hint;
} pc_physmem;

/*
 * Initialize from a Multiboot v1 variable-sized memory-map byte stream.
 * Only pages wholly contained in type-1 RAM are made available. Any
 * overlapping non-type-1 entry wins and marks its touched pages reserved.
 */
int pc_physmem_init(pc_physmem *memory, uint8_t *bitmap,
                    uint32_t bitmap_bytes, const void *mmap,
                    uint32_t mmap_bytes);

/* Reserve every page touched by [base, base + length). */
int pc_physmem_reserve(pc_physmem *memory, uint64_t base, uint64_t length);

/* Allocate one page below 4 GiB; returns its physical address or 0 on failure. */
uint32_t pc_physmem_alloc_page(pc_physmem *memory);

/* Allocate a contiguous, aligned run of pages below 4 GiB. */
uint32_t pc_physmem_alloc_pages(pc_physmem *memory, uint32_t page_count,
                                uint32_t alignment_pages);

#endif
