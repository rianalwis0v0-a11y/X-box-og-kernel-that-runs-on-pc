#ifndef PC_PORT_NV2A_VERTEX_SHADER_H
#define PC_PORT_NV2A_VERTEX_SHADER_H

#include <stdint.h>

#if defined(_MSC_VER)
# define PC_NV2A_API __declspec(dllexport)
# define PC_NV2A_CALL __cdecl
#else
# define PC_NV2A_API
# define PC_NV2A_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pc_nv2a_vertex_program_info {
    uint32_t instruction_count;
    uint16_t input_mask;
    uint16_t output_mask;
    uint64_t constant_mask[3];
    uint32_t uses_relative_constants;
} pc_nv2a_vertex_program_info;

/*
 * XboxRuntime portable NV2A microcode validation/metadata stage.
 * This validates instruction fields and reports referenced registers.
 * It does NOT execute or compile the shader; a native PC GPU backend must
 * lower the validated program to that GPU's native instruction/API model.
 */
PC_NV2A_API int PC_NV2A_CALL PcNv2aValidateVertexProgram(
    const uint32_t *words, uint32_t instruction_count,
    pc_nv2a_vertex_program_info *metadata);

#ifdef __cplusplus
}
#endif
#endif
