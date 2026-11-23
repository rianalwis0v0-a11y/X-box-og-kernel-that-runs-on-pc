#ifndef XBOX_PC_BOOT_H
#define XBOX_PC_BOOT_H

#include <stdint.h>

#define XBOX_PC_MULTIBOOT_MAGIC 0x2BADB002u
#define XBOX_PC_BOOT_OK 0
#define XBOX_PC_BOOT_NO_KERNEL 1
#define XBOX_PC_BOOT_BAD_HANDOFF 2
#define XBOX_PC_BOOT_BAD_PE 3

typedef struct xbox_pc_kernel_image {
    const uint8_t *file;
    uint32_t file_size;
    uint32_t preferred_base;
    uint32_t entry_rva;
    uint32_t image_size;
    uint32_t headers_size;
    uint16_t section_count;
} xbox_pc_kernel_image;

/* Validate one PE32/i386 file image without loading or executing it. */
int xbox_pc_validate_pe32_file(const uint8_t *file, uint32_t file_size,
                               xbox_pc_kernel_image *out);

/*
 * Locate and validate an xboxkrnl.exe Multiboot module.
 * This deliberately does not execute it or invent XboxCryptKeys.
 */
int xbox_pc_find_kernel_module(uint32_t multiboot_magic,
                               uint32_t multiboot_info_address,
                               xbox_pc_kernel_image *out);

#endif
