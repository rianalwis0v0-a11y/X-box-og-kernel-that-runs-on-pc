#include "pc_platform_abi.h"
#include "nv2a_bridge.h"

/* Compile-time contract test; no hardware support is implied. */
int main(void) {
    pc_platform_call call = {0};
    pc_platform_pci_device gpu = {0};
    pc_platform_dma_request dma = {0};
    pc_nv2a_command command = {0};

    call.magic = PC_PLATFORM_CALL_MAGIC;
    call.abi_major = PC_PLATFORM_ABI_MAJOR;
    call.operation = PC_PLATFORM_OP_NV2A_SUBMIT;
    gpu.vendor_id = PC_PCI_VENDOR_NVIDIA;
    gpu.device_id = PC_PCI_DEVICE_NV2A;
    dma.direction = PC_NV2A_BUFFER_READ;
    command.opcode = PC_NV2A_OP_SUBMIT_PUSHBUFFER;

    return (int)(call.magic == 0 || gpu.device_id == 0 ||
                 dma.direction == 0 || command.opcode == 0);
}
