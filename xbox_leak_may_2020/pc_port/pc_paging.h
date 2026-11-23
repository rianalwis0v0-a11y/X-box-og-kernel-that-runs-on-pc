#ifndef PC_PAGING_H
#define PC_PAGING_H

#include <stdint.h>

#define PC_PAGING_PAGE_SIZE 4096u
#define PC_PAGING_ENTRIES 1024u
#define PC_PAGING_PRESENT 0x001u
#define PC_PAGING_WRITE   0x002u
#define PC_PAGING_USER    0x004u
#define PC_PAGING_PWT     0x008u
#define PC_PAGING_PCD     0x010u
#define PC_PAGING_FLAGS_MASK (PC_PAGING_WRITE | PC_PAGING_USER | PC_PAGING_PWT | PC_PAGING_PCD)

/*
 * A non-PAE 32-bit x86 page-table builder.
 *
 * directory and tables are writable kernel virtual pointers to physically
 * contiguous, page-aligned storage. Their physical addresses are supplied
 * separately because this code must also be usable by a future PC monitor.
 * This builder does not enable paging or allocate/reserve physical memory.
 */
typedef struct pc_paging_context {
    uint32_t *directory;
    uint32_t directory_physical;
    uint32_t *tables;
    uint32_t tables_physical;
    uint32_t table_capacity;
    uint32_t table_count;
    int16_t table_slot[PC_PAGING_ENTRIES];
} pc_paging_context;

int pc_paging_init(pc_paging_context *context,
                   uint32_t *directory, uint32_t directory_physical,
                   uint32_t *tables, uint32_t tables_physical,
                   uint32_t table_capacity);

/* Map a page-aligned, page-sized identity or remapped range. */
int pc_paging_map(pc_paging_context *context,
                 uint32_t virtual_address, uint32_t physical_address,
                 uint32_t length, uint32_t flags);

/* Resolve one virtual address through the tables built by this module. */
int pc_paging_translate(const pc_paging_context *context,
                        uint32_t virtual_address, uint32_t *physical_address,
                        uint32_t *flags);

#endif
