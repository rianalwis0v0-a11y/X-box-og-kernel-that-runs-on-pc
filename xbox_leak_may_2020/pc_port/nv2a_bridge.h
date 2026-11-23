#ifndef PC_PORT_NV2A_BRIDGE_H
#define PC_PORT_NV2A_BRIDGE_H

/*
 * Versioned guest-to-host NV2A submission ABI.
 * This is a shared-memory protocol definition, not a GPU implementation.
 *
 * ABI rules:
 * - fixed-width fields only; no native pointers, size_t, or bitfields;
 * - offsets are relative to a registered shared-memory region;
 * - host validates offset + length with overflow-safe arithmetic;
 * - sequence numbers are monotonically increasing per guest context;
 * - guest does not reuse command/data storage until its fence completes.
 */
#include <stdint.h>

#define PC_NV2A_ABI_MAJOR 1u
#define PC_NV2A_ABI_MINOR 0u
#define PC_NV2A_MAX_SUBMISSION_BYTES (16u * 1024u * 1024u)

#if defined(_MSC_VER)
# define PC_NV2A_ALIGN8 __declspec(align(8))
#else
# define PC_NV2A_ALIGN8 __attribute__((aligned(8)))
#endif

typedef enum pc_nv2a_status {
    PC_NV2A_OK = 0,
    PC_NV2A_E_INVALID_ARGUMENT = 1,
    PC_NV2A_E_BAD_ABI = 2,
    PC_NV2A_E_BOUNDS = 3,
    PC_NV2A_E_UNSUPPORTED_METHOD = 4,
    PC_NV2A_E_NO_MEMORY = 5,
    PC_NV2A_E_DEVICE_LOST = 6,
    PC_NV2A_E_INTERNAL = 7
} pc_nv2a_status;

typedef enum pc_nv2a_opcode {
    PC_NV2A_OP_NOP = 0,
    PC_NV2A_OP_SUBMIT_PUSHBUFFER = 1,
    PC_NV2A_OP_REGISTER_BUFFER = 2,
    PC_NV2A_OP_UNREGISTER_BUFFER = 3,
    PC_NV2A_OP_WAIT_FENCE = 4,
    PC_NV2A_OP_QUERY_CAPS = 5
} pc_nv2a_opcode;

typedef struct PC_NV2A_ALIGN8 pc_nv2a_shared_header {
    uint32_t abi_major;
    uint32_t abi_minor;
    uint32_t header_bytes;
    uint32_t ring_bytes;
    uint64_t producer_seq;
    uint64_t consumer_seq;
    uint64_t completed_fence;
    uint32_t host_status;
    uint32_t reserved0;
} pc_nv2a_shared_header;

typedef struct PC_NV2A_ALIGN8 pc_nv2a_command {
    uint32_t opcode;
    uint32_t flags;
    uint64_t sequence;
    uint64_t payload_offset;
    uint32_t payload_bytes;
    uint32_t context_id;
} pc_nv2a_command;

typedef struct PC_NV2A_ALIGN8 pc_nv2a_pushbuffer {
    uint64_t command_offset;
    uint32_t command_bytes;
    uint32_t guest_context_id;
    uint64_t completion_fence;
} pc_nv2a_pushbuffer;

typedef struct PC_NV2A_ALIGN8 pc_nv2a_buffer {
    uint64_t guest_physical_address;
    uint64_t shared_offset;
    uint64_t byte_length;
    uint32_t access_flags;
    uint32_t buffer_id;
} pc_nv2a_buffer;

#define PC_NV2A_BUFFER_READ  0x1u
#define PC_NV2A_BUFFER_WRITE 0x2u

#if defined(__cplusplus)
static_assert(sizeof(pc_nv2a_shared_header) == 48, "NV2A shared header ABI drift");
static_assert(sizeof(pc_nv2a_command) == 32, "NV2A command ABI drift");
static_assert(sizeof(pc_nv2a_pushbuffer) == 24, "NV2A pushbuffer ABI drift");
static_assert(sizeof(pc_nv2a_buffer) == 32, "NV2A buffer ABI drift");
#else
_Static_assert(sizeof(pc_nv2a_shared_header) == 48, "NV2A shared header ABI drift");
_Static_assert(sizeof(pc_nv2a_command) == 32, "NV2A command ABI drift");
_Static_assert(sizeof(pc_nv2a_pushbuffer) == 24, "NV2A pushbuffer ABI drift");
_Static_assert(sizeof(pc_nv2a_buffer) == 32, "NV2A buffer ABI drift");
#endif

#endif
