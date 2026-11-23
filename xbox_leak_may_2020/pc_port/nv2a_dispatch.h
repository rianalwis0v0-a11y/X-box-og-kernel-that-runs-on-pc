#ifndef PC_PORT_NV2A_DISPATCH_H
#define PC_PORT_NV2A_DISPATCH_H

#include "nv2a_transport.h"

typedef struct pc_nv2a_dispatch_ops {
    void *context;
    pc_nv2a_status (*submit_pushbuffer)(void *context,
                                        const void *commands,
                                        uint32_t command_bytes,
                                        uint32_t guest_context_id,
                                        uint64_t completion_fence);
    pc_nv2a_status (*register_buffer)(void *context,
                                      const pc_nv2a_buffer *buffer);
    pc_nv2a_status (*unregister_buffer)(void *context, uint32_t buffer_id);
    pc_nv2a_status (*wait_fence)(void *context, uint64_t fence);
    pc_nv2a_status (*query_caps)(void *context, void *output,
                                 uint32_t output_bytes);
} pc_nv2a_dispatch_ops;

/*
 * Consume at most one published command. Shared-memory offsets are validated
 * before dereference. A command is retired even when its operation fails;
 * host_status records the failure so a malformed command cannot wedge the ring.
 */
pc_nv2a_status pc_nv2a_dispatch_one(void *mapping, size_t mapped_bytes,
                                    const pc_nv2a_dispatch_ops *ops);

#endif
