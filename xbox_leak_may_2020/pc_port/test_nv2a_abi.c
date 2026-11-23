#include "nv2a_bridge.h"

/* Compile-time ABI/layout test; runtime GPU support is not implied. */
int main(void) {
    pc_nv2a_shared_header header = {0};
    pc_nv2a_command command = {0};
    pc_nv2a_pushbuffer push = {0};
    pc_nv2a_buffer buffer = {0};

    header.abi_major = PC_NV2A_ABI_MAJOR;
    command.opcode = PC_NV2A_OP_SUBMIT_PUSHBUFFER;
    push.command_bytes = 4;
    buffer.access_flags = PC_NV2A_BUFFER_READ | PC_NV2A_BUFFER_WRITE;
    return (int)(header.abi_major + command.opcode +
                 push.command_bytes + buffer.access_flags == 0);
}
