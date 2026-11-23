#ifndef XBOX_PC_PORT_NV2A_PC_BRIDGE_H
#define XBOX_PC_PORT_NV2A_PC_BRIDGE_H

/*
 * Boundary between Xbox-visible NV2A semantics and a normal PC GPU API.
 *
 * The Xbox side owns register/method/pushbuffer interpretation. The host
 * renderer owns resource creation, shader translation, command submission,
 * synchronization and presentation. No NV2A physical BAR is required on the
 * target PC; guest-visible addresses are translated by this bridge.
 */
#include <stddef.h>
#include <stdint.h>
#include "pc_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NV2A_PC_BRIDGE_ABI_VERSION 1u

typedef enum nv2a_pc_format {
    NV2A_PC_FORMAT_UNKNOWN = 0,
    NV2A_PC_FORMAT_A8R8G8B8 = 1,
    NV2A_PC_FORMAT_X8R8G8B8 = 2,
    NV2A_PC_FORMAT_R5G6B5 = 3,
    NV2A_PC_FORMAT_D24S8 = 4
} nv2a_pc_format;

typedef struct nv2a_pc_surface_desc {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    nv2a_pc_format format;
    uint32_t guest_address;
    uint32_t guest_size;
} nv2a_pc_surface_desc;

typedef struct nv2a_pc_host_ops {
    uint32_t abi_version;
    uint32_t struct_size;
    void *context;

    /* Implemented by a D3D/Vulkan host renderer, not by Xbox HAL code. */
    pc_status (*create_surface)(void *context,
                                const nv2a_pc_surface_desc *desc,
                                uint64_t *host_surface);
    pc_status (*upload_surface)(void *context, uint64_t host_surface,
                                const void *pixels, size_t byte_count);
    pc_status (*translate_shader)(void *context, const uint32_t *guest_tokens,
                                  size_t token_count, uint64_t *host_shader);
    pc_status (*draw_indexed)(void *context, uint64_t host_shader,
                              const void *vertices, size_t vertex_bytes,
                              const uint32_t *indices, size_t index_count);
    pc_status (*present)(void *context, uint64_t host_surface);
    void (*destroy_surface)(void *context, uint64_t host_surface);
} nv2a_pc_host_ops;

typedef struct nv2a_pc_bridge {
    uint32_t abi_version;
    uint32_t struct_size;
    void *context;
    const nv2a_pc_host_ops *host;
} nv2a_pc_bridge;

/* Reject partial or ABI-incompatible host renderers; never silently succeed. */
pc_status nv2a_pc_bridge_validate(const nv2a_pc_bridge *bridge);

#ifdef __cplusplus
}
#endif

#endif
