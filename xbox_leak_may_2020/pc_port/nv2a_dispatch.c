#include "nv2a_dispatch.h"
#include <string.h>

static int range_ok(uint64_t offset, uint64_t length, size_t limit)
{
    return offset <= (uint64_t)limit &&
           length <= (uint64_t)limit - offset;
}

pc_nv2a_status pc_nv2a_dispatch_one(void *mapping, size_t mapped_bytes,
                                    const pc_nv2a_dispatch_ops *ops)
{
    pc_nv2a_shared_header *shared;
    pc_nv2a_shared_header snapshot;
    pc_nv2a_command command;
    uint64_t producer, consumer, slots, slot_offset;
    const unsigned char *base;
    const void *payload;
    pc_nv2a_status status;

    if (!mapping || !ops || mapped_bytes < sizeof(pc_nv2a_shared_header))
        return PC_NV2A_E_INVALID_ARGUMENT;

    shared = (pc_nv2a_shared_header *)mapping;
    base = (const unsigned char *)mapping;
    producer = __atomic_load_n(&shared->producer_seq, __ATOMIC_ACQUIRE);
    consumer = __atomic_load_n(&shared->consumer_seq, __ATOMIC_RELAXED);
    memcpy(&snapshot, shared, sizeof(snapshot));
    snapshot.producer_seq = producer;
    snapshot.consumer_seq = consumer;

    status = pc_nv2a_validate_header(&snapshot, mapped_bytes);
    if (status != PC_NV2A_OK) return status;
    if (producer == consumer) return PC_NV2A_OK;

    slots = snapshot.ring_bytes / sizeof(pc_nv2a_command);
    slot_offset = (uint64_t)snapshot.header_bytes +
                  ((consumer % slots) * sizeof(pc_nv2a_command));
    if (!range_ok(slot_offset, sizeof(command), mapped_bytes))
        return PC_NV2A_E_BOUNDS;
    memcpy(&command, base + slot_offset, sizeof(command));

    status = pc_nv2a_validate_command(&snapshot, &command, mapped_bytes,
                                      consumer + 1);
    if (status != PC_NV2A_OK) goto retire;

    if (!range_ok(command.payload_offset, command.payload_bytes, mapped_bytes)) {
        status = PC_NV2A_E_BOUNDS;
        goto retire;
    }
    payload = base + command.payload_offset;

    switch (command.opcode) {
    case PC_NV2A_OP_NOP:
        status = PC_NV2A_OK;
        break;
    case PC_NV2A_OP_SUBMIT_PUSHBUFFER: {
        pc_nv2a_pushbuffer push;
        if (command.payload_bytes != sizeof(push)) {
            status = PC_NV2A_E_INVALID_ARGUMENT;
            break;
        }
        memcpy(&push, payload, sizeof(push));
        status = pc_nv2a_validate_pushbuffer(&push, mapped_bytes);
        if (status != PC_NV2A_OK) break;
        if (!ops->submit_pushbuffer) {
            status = PC_NV2A_E_UNSUPPORTED_METHOD;
            break;
        }
        status = ops->submit_pushbuffer(ops->context,
                    base + push.command_offset, push.command_bytes,
                    push.guest_context_id, push.completion_fence);
        break;
    }
    case PC_NV2A_OP_REGISTER_BUFFER: {
        pc_nv2a_buffer buffer;
        if (command.payload_bytes != sizeof(buffer)) {
            status = PC_NV2A_E_INVALID_ARGUMENT;
            break;
        }
        memcpy(&buffer, payload, sizeof(buffer));
        if (!buffer.byte_length ||
            !range_ok(buffer.shared_offset, buffer.byte_length, mapped_bytes) ||
            !(buffer.access_flags & (PC_NV2A_BUFFER_READ | PC_NV2A_BUFFER_WRITE)) ||
            (buffer.access_flags & ~(PC_NV2A_BUFFER_READ | PC_NV2A_BUFFER_WRITE))) {
            status = PC_NV2A_E_BOUNDS;
            break;
        }
        status = ops->register_buffer ?
            ops->register_buffer(ops->context, &buffer) :
            PC_NV2A_E_UNSUPPORTED_METHOD;
        break;
    }
    case PC_NV2A_OP_UNREGISTER_BUFFER: {
        uint32_t id;
        if (command.payload_bytes != sizeof(id)) {
            status = PC_NV2A_E_INVALID_ARGUMENT;
            break;
        }
        memcpy(&id, payload, sizeof(id));
        status = ops->unregister_buffer ?
            ops->unregister_buffer(ops->context, id) :
            PC_NV2A_E_UNSUPPORTED_METHOD;
        break;
    }
    case PC_NV2A_OP_WAIT_FENCE: {
        uint64_t fence;
        if (command.payload_bytes != sizeof(fence)) {
            status = PC_NV2A_E_INVALID_ARGUMENT;
            break;
        }
        memcpy(&fence, payload, sizeof(fence));
        status = ops->wait_fence ?
            ops->wait_fence(ops->context, fence) :
            PC_NV2A_E_UNSUPPORTED_METHOD;
        break;
    }
    case PC_NV2A_OP_QUERY_CAPS:
        status = ops->query_caps ?
            ops->query_caps(ops->context, (void *)payload,
                            command.payload_bytes) :
            PC_NV2A_E_UNSUPPORTED_METHOD;
        break;
    default:
        status = PC_NV2A_E_UNSUPPORTED_METHOD;
        break;
    }

retire:
    __atomic_store_n(&shared->host_status, (uint32_t)status, __ATOMIC_RELAXED);
    __atomic_store_n(&shared->consumer_seq, consumer + 1, __ATOMIC_RELEASE);
    return status;
}
