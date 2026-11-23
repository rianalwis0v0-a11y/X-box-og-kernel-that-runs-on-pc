#ifndef PC_PORT_NV2A_TRANSPORT_H
#define PC_PORT_NV2A_TRANSPORT_H

#include <stddef.h>
#include "nv2a_bridge.h"

/* Validate guest-controlled shared-memory metadata before dereferencing it. */
pc_nv2a_status pc_nv2a_validate_header(const pc_nv2a_shared_header *header,
                                        size_t mapped_bytes);
pc_nv2a_status pc_nv2a_validate_command(const pc_nv2a_shared_header *header,
                                         const pc_nv2a_command *command,
                                         size_t mapped_bytes,
                                         uint64_t expected_sequence);
pc_nv2a_status pc_nv2a_validate_pushbuffer(const pc_nv2a_pushbuffer *pushbuffer,
                                            size_t mapped_bytes);

#endif
