#ifndef PC_PORT_PLATFORM_ABI_H
#define PC_PORT_PLATFORM_ABI_H

/*
 * Guest-to-PC platform ABI for the Xbox x86 compatibility guest.
 *
 * Transport: VMCALL from the 32-bit guest. The monitor validates the
 * registered shared-memory window and copies request/reply records.
 * No guest virtual pointers cross this boundary.
 *
 * This is an ABI contract only. It does not implement a VMM or claim that
 * any physical device is already supported.
 */
#include <stdint.h>

#define PC_PLATFORM_ABI_MAJOR 1u
#define PC_PLATFORM_ABI_MINOR 0u
#define PC_PLATFORM_CALL_MAGIC 0x50435043u /* "PCPC" */
#define PC_PLATFORM_MAX_RECORD_BYTES (64u * 1024u)

#define PC_PCI_VENDOR_NVIDIA 0x10DEu
#define PC_PCI_DEVICE_NV2A   0x02A0u

typedef enum pc_platform_status {
    PC_PLATFORM_OK = 0,
    PC_PLATFORM_E_INVALID = 1,
    PC_PLATFORM_E_BAD_ABI = 2,
    PC_PLATFORM_E_BOUNDS = 3,
    PC_PLATFORM_E_UNSUPPORTED = 4,
    PC_PLATFORM_E_NO_DEVICE = 5,
    PC_PLATFORM_E_BUSY = 6,
    PC_PLATFORM_E_IO = 7,
    PC_PLATFORM_E_NO_MEMORY = 8
} pc_platform_status;

typedef enum pc_platform_operation {
    PC_PLATFORM_OP_QUERY = 0,
    PC_PLATFORM_OP_MEMORY_MAP = 1,
    PC_PLATFORM_OP_PCI_ENUMERATE = 2,
    PC_PLATFORM_OP_PCI_CONFIG_READ = 3,
    PC_PLATFORM_OP_PCI_CONFIG_WRITE = 4,
    PC_PLATFORM_OP_MMIO_MAP = 5,
    PC_PLATFORM_OP_DMA_MAP = 6,
    PC_PLATFORM_OP_DMA_UNMAP = 7,
    PC_PLATFORM_OP_INTERRUPT_REGISTER = 8,
    PC_PLATFORM_OP_INTERRUPT_EOI = 9,
    PC_PLATFORM_OP_TIMER_QUERY = 10,
    PC_PLATFORM_OP_BLOCK_IO = 11,
    PC_PLATFORM_OP_NV2A_SUBMIT = 12
} pc_platform_operation;

typedef enum pc_platform_memory_type {
    PC_PLATFORM_MEMORY_USABLE = 1,
    PC_PLATFORM_MEMORY_RESERVED = 2,
    PC_PLATFORM_MEMORY_ACPI = 3,
    PC_PLATFORM_MEMORY_MMIO = 4
} pc_platform_memory_type;

typedef struct pc_platform_call {
    uint32_t magic;
    uint16_t abi_major;
    uint16_t abi_minor;
    uint32_t operation;
    uint32_t record_bytes;
    uint64_t shared_offset;
    uint64_t sequence;
    uint32_t status;
    uint32_t reserved;
} pc_platform_call;

typedef struct pc_platform_memory_range {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t attributes;
} pc_platform_memory_range;

typedef struct pc_platform_pci_address {
    uint16_t segment;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t reserved[3];
} pc_platform_pci_address;

typedef struct pc_platform_pci_device {
    pc_platform_pci_address address;
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint32_t class_code;
    uint32_t bar[6];
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint16_t reserved;
} pc_platform_pci_device;

typedef struct pc_platform_pci_config {
    pc_platform_pci_address address;
    uint16_t offset;
    uint8_t width;
    uint8_t is_write;
    uint32_t value;
} pc_platform_pci_config;

typedef struct pc_platform_dma_request {
    uint64_t guest_physical_address;
    uint64_t byte_length;
    uint64_t host_mapping_id;
    uint32_t direction;
    uint32_t reserved;
} pc_platform_dma_request;

typedef struct pc_platform_interrupt_request {
    uint32_t guest_irq;
    uint32_t vector;
    uint32_t flags;
    uint32_t reserved;
} pc_platform_interrupt_request;

typedef struct pc_platform_timer_info {
    uint64_t frequency_hz;
    uint64_t counter;
    uint32_t flags;
    uint32_t reserved;
} pc_platform_timer_info;

typedef struct pc_platform_block_io {
    uint64_t lba;
    uint64_t shared_offset;
    uint32_t sector_count;
    uint32_t sector_bytes;
    uint32_t is_write;
    uint32_t device_id;
} pc_platform_block_io;

#if defined(__cplusplus)
# define PC_PLATFORM_STATIC_ASSERT static_assert
#else
# define PC_PLATFORM_STATIC_ASSERT _Static_assert
#endif

PC_PLATFORM_STATIC_ASSERT(sizeof(pc_platform_call) == 40, "platform call ABI drift");
PC_PLATFORM_STATIC_ASSERT(sizeof(pc_platform_memory_range) == 24, "memory range ABI drift");
PC_PLATFORM_STATIC_ASSERT(sizeof(pc_platform_pci_address) == 8, "PCI address ABI drift");
PC_PLATFORM_STATIC_ASSERT(sizeof(pc_platform_pci_device) == 40, "PCI device ABI drift");
PC_PLATFORM_STATIC_ASSERT(sizeof(pc_platform_pci_config) == 16, "PCI config ABI drift");
PC_PLATFORM_STATIC_ASSERT(sizeof(pc_platform_dma_request) == 32, "DMA request ABI drift");
PC_PLATFORM_STATIC_ASSERT(sizeof(pc_platform_interrupt_request) == 16, "interrupt ABI drift");
PC_PLATFORM_STATIC_ASSERT(sizeof(pc_platform_timer_info) == 24, "timer ABI drift");
PC_PLATFORM_STATIC_ASSERT(sizeof(pc_platform_block_io) == 32, "block I/O ABI drift");

#undef PC_PLATFORM_STATIC_ASSERT
#endif
