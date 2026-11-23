#include "include/nv2a_pc_bridge.h"

pc_status nv2a_pc_bridge_validate(const nv2a_pc_bridge *bridge)
{
    const nv2a_pc_host_ops *host;
    if (!bridge || !bridge->host) return PC_STATUS_INVALID_ARGUMENT;
    if (bridge->abi_version != NV2A_PC_BRIDGE_ABI_VERSION ||
        bridge->struct_size < sizeof(nv2a_pc_bridge))
        return PC_STATUS_UNSUPPORTED;
    host = bridge->host;
    if (host->abi_version != NV2A_PC_BRIDGE_ABI_VERSION ||
        host->struct_size < sizeof(nv2a_pc_host_ops))
        return PC_STATUS_UNSUPPORTED;
    if (!host->create_surface || !host->upload_surface ||
        !host->translate_shader || !host->draw_indexed ||
        !host->present || !host->destroy_surface)
        return PC_STATUS_UNSUPPORTED;
    return PC_STATUS_OK;
}
