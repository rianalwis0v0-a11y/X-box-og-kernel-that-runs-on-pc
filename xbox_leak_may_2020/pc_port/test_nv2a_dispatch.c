#include "nv2a_dispatch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned submitted;
static pc_nv2a_status submit(void *ctx, const void *commands, uint32_t bytes,
                             uint32_t guest_context, uint64_t fence)
{
    (void)ctx;
    assert(commands != NULL);
    assert(bytes == 8);
    assert(guest_context == 7);
    assert(fence == 42);
    ++submitted;
    return PC_NV2A_OK;
}

int main(void)
{
    unsigned char memory[512];
    pc_nv2a_shared_header *h = (pc_nv2a_shared_header *)memory;
    pc_nv2a_command *ring;
    pc_nv2a_pushbuffer push;
    pc_nv2a_dispatch_ops ops;
    const uint32_t ring_bytes = 2 * sizeof(pc_nv2a_command);
    const uint32_t payload_offset = sizeof(*h) + ring_bytes;
    const uint32_t push_offset = payload_offset + sizeof(push);

    memset(memory, 0, sizeof(memory));
    memset(&ops, 0, sizeof(ops));
    ops.submit_pushbuffer = submit;
    h->abi_major = PC_NV2A_ABI_MAJOR;
    h->abi_minor = PC_NV2A_ABI_MINOR;
    h->header_bytes = sizeof(*h);
    h->ring_bytes = ring_bytes;
    h->producer_seq = 1;
    h->consumer_seq = 0;
    ring = (pc_nv2a_command *)(memory + h->header_bytes);

    memset(&push, 0, sizeof(push));
    push.command_offset = push_offset;
    push.command_bytes = 8;
    push.guest_context_id = 7;
    push.completion_fence = 42;
    memcpy(memory + payload_offset, &push, sizeof(push));
    memcpy(memory + push_offset, "NV2ATEST", 8);

    ring[0].opcode = PC_NV2A_OP_SUBMIT_PUSHBUFFER;
    ring[0].sequence = 1;
    ring[0].payload_offset = payload_offset;
    ring[0].payload_bytes = sizeof(push);

    assert(pc_nv2a_dispatch_one(memory, sizeof(memory), &ops) == PC_NV2A_OK);
    assert(submitted == 1);
    assert(h->consumer_seq == 1);
    assert(h->host_status == PC_NV2A_OK);
    assert(pc_nv2a_dispatch_one(memory, sizeof(memory), &ops) == PC_NV2A_OK);
    assert(submitted == 1);

    h->producer_seq = 2;
    ring[1].opcode = PC_NV2A_OP_SUBMIT_PUSHBUFFER;
    ring[1].sequence = 2;
    ring[1].payload_offset = sizeof(memory) - 4;
    ring[1].payload_bytes = sizeof(push);
    assert(pc_nv2a_dispatch_one(memory, sizeof(memory), &ops) == PC_NV2A_E_BOUNDS);
    assert(h->consumer_seq == 2);
    assert(h->host_status == PC_NV2A_E_BOUNDS);

    puts("NV2A-DISPATCH: ring dispatch PASS");
    return 0;
}
