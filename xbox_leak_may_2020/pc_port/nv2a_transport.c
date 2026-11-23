#include "nv2a_transport.h"
#include <stdint.h>

static int range_ok(uint64_t offset, uint64_t length, size_t limit)
{
    return offset <= (uint64_t)limit &&
           length <= (uint64_t)limit - offset;
}

pc_nv2a_status pc_nv2a_validate_header(const pc_nv2a_shared_header *h,
                                        size_t mapped_bytes)
{
    uint64_t required;
    if (!h) return PC_NV2A_E_INVALID_ARGUMENT;
    if (h->abi_major != PC_NV2A_ABI_MAJOR ||
        h->abi_minor > PC_NV2A_ABI_MINOR ||
        h->header_bytes < sizeof(*h) ||
        h->ring_bytes < sizeof(pc_nv2a_command) ||
        h->ring_bytes % sizeof(pc_nv2a_command) != 0)
        return PC_NV2A_E_BAD_ABI;
    required = (uint64_t)h->header_bytes + h->ring_bytes;
    if (!range_ok(0, required, mapped_bytes)) return PC_NV2A_E_BOUNDS;
    if (h->producer_seq < h->consumer_seq ||
        h->producer_seq - h->consumer_seq >
            h->ring_bytes / sizeof(pc_nv2a_command))
        return PC_NV2A_E_BOUNDS;
    return PC_NV2A_OK;
}

pc_nv2a_status pc_nv2a_validate_command(const pc_nv2a_shared_header *h,
                                         const pc_nv2a_command *c,
                                         size_t mapped_bytes,
                                         uint64_t expected_sequence)
{
    pc_nv2a_status status = pc_nv2a_validate_header(h, mapped_bytes);
    if (status != PC_NV2A_OK) return status;
    if (!c) return PC_NV2A_E_INVALID_ARGUMENT;
    if (c->sequence != expected_sequence) return PC_NV2A_E_BAD_ABI;
    if (c->payload_bytes > PC_NV2A_MAX_SUBMISSION_BYTES ||
        !range_ok(c->payload_offset, c->payload_bytes, mapped_bytes))
        return PC_NV2A_E_BOUNDS;
    switch (c->opcode) {
    case PC_NV2A_OP_NOP:
        return c->payload_bytes == 0 ? PC_NV2A_OK : PC_NV2A_E_INVALID_ARGUMENT;
    case PC_NV2A_OP_SUBMIT_PUSHBUFFER:
    case PC_NV2A_OP_REGISTER_BUFFER:
    case PC_NV2A_OP_UNREGISTER_BUFFER:
    case PC_NV2A_OP_WAIT_FENCE:
    case PC_NV2A_OP_QUERY_CAPS:
        return PC_NV2A_OK;
    default:
        return PC_NV2A_E_UNSUPPORTED_METHOD;
    }
}

pc_nv2a_status pc_nv2a_validate_pushbuffer(const pc_nv2a_pushbuffer *p,
                                            size_t mapped_bytes)
{
    if (!p) return PC_NV2A_E_INVALID_ARGUMENT;
    if (p->command_bytes == 0 ||
        p->command_bytes > PC_NV2A_MAX_SUBMISSION_BYTES ||
        (p->command_offset & 3u) != 0 ||
        !range_ok(p->command_offset, p->command_bytes, mapped_bytes))
        return PC_NV2A_E_BOUNDS;
    return PC_NV2A_OK;
}
