/*
 * Derived from XboxRuntime modules/nv2a/nv2a_vertex_shader.c.
 * Copyright (c) 2026 Rian Alwis. MIT License; see XBOXRUNTIME_LICENSE.txt.
 *
 * Kept freestanding: no Win32, D3D, heap, or host-process dependencies.
 */
#include <stdint.h>
#include <string.h>
#include "nv2a_vertex_shader.h"

static unsigned field(uint32_t value, unsigned shift, unsigned width)
{
    return (value >> shift) & ((1u << width) - 1u);
}

static int decode_source(const uint32_t *words, unsigned source,
                         pc_nv2a_vertex_program_info *metadata)
{
    unsigned mux;
    if (source == 0u) {
        mux = field(words[2], 26u, 2u);
    } else if (source == 1u) {
        mux = field(words[2], 11u, 2u);
    } else {
        mux = field(words[3], 28u, 2u);
    }

    if (mux == 1u) {
        /* Temporary register source; no external input metadata. */
    } else if (mux == 2u) {
        metadata->input_mask |= (uint16_t)(1u << field(words[1], 9u, 4u));
    } else if (mux == 0u || mux == 3u) {
        unsigned constant_index = field(words[1], 13u, 8u);
        if (constant_index >= 192u)
            return 0;
        metadata->constant_mask[constant_index >> 6] |=
            (uint64_t)1u << (constant_index & 63u);
        if (words[3] & 2u)
            metadata->uses_relative_constants = 1u;
    } else {
        return 0;
    }
    return 1;
}

int PcNv2aValidateVertexProgram(const uint32_t *words, uint32_t count,
                               pc_nv2a_vertex_program_info *output)
{
    pc_nv2a_vertex_program_info metadata;
    uint32_t index;
    int terminated = 0;

    if (!words || !output || !count || count > 136u)
        return 1;

    memset(&metadata, 0, sizeof(metadata));
    for (index = 0; index < count; ++index) {
        const uint32_t *instruction = words + index * 4u;
        unsigned mac = field(instruction[1], 21u, 4u);
        unsigned ilu = field(instruction[1], 25u, 3u);
        unsigned mac_mask = field(instruction[3], 24u, 4u);
        unsigned ilu_mask = field(instruction[3], 16u, 4u);
        unsigned output_mask = field(instruction[3], 12u, 4u);
        unsigned destination = field(instruction[3], 20u, 4u);
        unsigned output_register = field(instruction[3], 3u, 8u);

        if (mac > 13u || ilu > 7u)
            return 2;
        if (mac && !decode_source(instruction, 0u, &metadata))
            return 4;
        if (mac >= 2u && mac <= 12u &&
            !decode_source(instruction, 1u, &metadata))
            return 5;
        if ((ilu != 0u || mac == 4u) &&
            !decode_source(instruction, 2u, &metadata))
            return 6;
        if (mac && mac_mask && destination > 12u)
            return 7;
        (void)ilu_mask;
        if (output_mask) {
            if (!field(instruction[3], 11u, 1u) ||
                output_register > 12u ||
                output_register == 1u || output_register == 2u)
                return 8;
            metadata.output_mask |= (uint16_t)(1u << output_register);
        }
        metadata.instruction_count = index + 1u;
        if (instruction[3] & 1u) {
            terminated = 1;
            break;
        }
    }
    if (!terminated)
        return 9;
    *output = metadata;
    return 0;
}
