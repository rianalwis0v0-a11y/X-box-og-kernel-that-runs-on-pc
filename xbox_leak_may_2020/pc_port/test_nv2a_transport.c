#include "nv2a_transport.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    pc_nv2a_shared_header h;
    pc_nv2a_command c;
    pc_nv2a_pushbuffer p;
    memset(&h, 0, sizeof(h));
    memset(&c, 0, sizeof(c));
    memset(&p, 0, sizeof(p));

    h.abi_major = PC_NV2A_ABI_MAJOR;
    h.abi_minor = PC_NV2A_ABI_MINOR;
    h.header_bytes = sizeof(h);
    h.ring_bytes = 4 * sizeof(c);
    h.producer_seq = 1;
    h.consumer_seq = 0;
    assert(pc_nv2a_validate_header(&h, sizeof(h) + h.ring_bytes) == PC_NV2A_OK);
    assert(pc_nv2a_validate_header(&h, sizeof(h)) == PC_NV2A_E_BOUNDS);

    c.opcode = PC_NV2A_OP_SUBMIT_PUSHBUFFER;
    c.sequence = 1;
    c.payload_offset = sizeof(h) + h.ring_bytes;
    c.payload_bytes = 16;
    assert(pc_nv2a_validate_command(&h, &c, sizeof(h) + h.ring_bytes + 16, 1) == PC_NV2A_OK);
    assert(pc_nv2a_validate_command(&h, &c, sizeof(h) + h.ring_bytes, 1) == PC_NV2A_E_BOUNDS);
    assert(pc_nv2a_validate_command(&h, &c, sizeof(h) + h.ring_bytes + 16, 2) == PC_NV2A_E_BAD_ABI);

    p.command_offset = sizeof(h) + h.ring_bytes;
    p.command_bytes = 32;
    assert(pc_nv2a_validate_pushbuffer(&p, sizeof(h) + h.ring_bytes + 32) == PC_NV2A_OK);
    assert(pc_nv2a_validate_pushbuffer(&p, sizeof(h) + h.ring_bytes) == PC_NV2A_E_BOUNDS);
    puts("NV2A-TRANSPORT: bounds tests PASS");
    return 0;
}
