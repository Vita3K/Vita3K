#include "functions.h"

#include "profile.h"

#include <gxm/functions.h>
#include <gxm/types.h>
#include <util/log.h>

namespace renderer {
template <class T>
static void uniform_4(GLint location, GLsizei count, const T *value);

template <>
void uniform_4<GLfloat>(GLint location, GLsizei count, const GLfloat *value) {
    glUniform4fv(location, count, value);
}

template <>
void uniform_4<GLint>(GLint location, GLsizei count, const GLint *value) {
    glUniform4iv(location, count, value);
}

template <class T>
static void uniform_1(GLint location, GLsizei count, const T *value);

template <>
void uniform_1<GLfloat>(GLint location, GLsizei count, const GLfloat *value) {
    glUniform1fv(location, count, value);
}

template <>
void uniform_1<GLint>(GLint location, GLsizei count, const GLint *value) {
    glUniform1iv(location, count, value);
}

template <class T>
static void uniform_matrix_4(GLint location, GLsizei count, GLboolean, const T *value);

template <>
void uniform_matrix_4<GLfloat>(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    glUniformMatrix4fv(location, count, transpose, value);
}

template <>
void uniform_matrix_4<GLint>(GLint location, GLsizei count, GLboolean transpose, const GLint *value) {
    glUniform4iv(location, count, (GLint *)value);
}

template <class T>
static void set_uniform(GLint location, size_t component_count, GLsizei array_size, const T *value) {
    switch (component_count) {
    case 1:
        switch (array_size) {
        case 1:
            uniform_1<T>(location, array_size, value);
            break;
        }
        break;
    case 4:
        if (array_size % 4 == 0) {
            uniform_matrix_4<T>(location, array_size / 4, GL_FALSE, value);
        } else {
            uniform_4<T>(location, array_size, value);
        }
        break;

    default:
        LOG_WARN("Unhandled uniform component count {}.", component_count);
        break;
    }
}

static void set_uniforms(GLuint gl_program, const UniformBuffers &uniform_buffers, const SceGxmProgram &gxm_program, const MemState &mem) {
    R_PROFILE(__func__);

    const SceGxmProgramParameter *const parameters = gxm::program_parameters(gxm_program);
    for (size_t i = 0; i < gxm_program.parameter_count; ++i) {
        const SceGxmProgramParameter &parameter = parameters[i];
        if (parameter.category != SCE_GXM_PARAMETER_CATEGORY_UNIFORM) {
            continue;
        }

        auto name = gxm::parameter_name(parameter);
        const GLint location = glGetUniformLocation(gl_program, name.c_str());
        if (location < 0) {
            // NOTE: This can happen because the uniform isn't used in the shader, thus optimized away by the shader compiler.
            LOG_WARN("Uniform parameter {} not found in current OpenGL program.", name);
            continue;
        }

        const SceGxmParameterType type = static_cast<SceGxmParameterType>(static_cast<uint16_t>(parameter.type));
        const Ptr<const void> uniform_buffer = uniform_buffers[parameter.container_index];
        if (!uniform_buffer) {
            LOG_WARN("Uniform buffer {} not set for parameter {}.", parameter.container_index, name);
            continue;
        }

        const GLint *src_s32;
        const GLfloat *src_f32;

        const uint8_t *const base = static_cast<const uint8_t *>(uniform_buffer.get(mem));
        switch (type) {
        case SCE_GXM_PARAMETER_TYPE_S32:
            src_s32 = reinterpret_cast<const GLint *>(base + parameter.resource_index * 4); // TODO What offset?
            set_uniform<GLint>(location, parameter.component_count, parameter.array_size, src_s32);
            break;
        case SCE_GXM_PARAMETER_TYPE_F32:
            src_f32 = reinterpret_cast<const GLfloat *>(base + parameter.resource_index * 4); // TODO What offset?
            set_uniform<GLfloat>(location, parameter.component_count, parameter.array_size, src_f32);
            break;
        default:
            LOG_WARN("Type {} not handled for uniform parameter {}.", type, name);
            break;
        }
    }
}

void set_uniforms(GLuint program, const GxmContextState &state, const MemState &mem) {
    R_PROFILE(__func__);

    assert(state.fragment_program);
    assert(state.vertex_program);

    const SceGxmFragmentProgram &fragment_program = *state.fragment_program.get(mem);
    const SceGxmVertexProgram &vertex_program = *state.vertex_program.get(mem);
    assert(fragment_program.program);
    assert(vertex_program.program);

    set_uniforms(program, state.fragment_uniform_buffers, *fragment_program.program.get(mem), mem);
    set_uniforms(program, state.vertex_uniform_buffers, *vertex_program.program.get(mem), mem);
}
} // namespace renderer
