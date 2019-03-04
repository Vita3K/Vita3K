#include "functions.h"

#include "profile.h"

#include <gxm/functions.h>
#include <gxm/types.h>
#include <renderer/types.h>
#include <util/log.h>

#include <algorithm>

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
static void set_uniform(GLint location, size_t component_count, GLsizei array_size, const T *value, const std::string &name, bool log_uniforms) {
    if (log_uniforms) {
        // warning: shit code
        std::string values = "{ ";
        for (auto i = 0; i < array_size; ++i) {
            int i2;
            for (i2 = 0; i2 < component_count; ++i2) {
                values += fmt::format("{}, ", value[i + (i2 * component_count)]);
            }
            if (i2 > 0)
                values.erase(values.size() - 2, 2);
            values += '\n';
        }
        values.erase(values.size() - 1, 1);
        values += " }";
        const auto items = array_size * component_count;
        LOG_ERROR("name: {}   loc: {}, component_count: {}, array_size: {}, values: {}{}", name, location, component_count, array_size, items > 1 ? "\n" : "", values);
    }

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

static void set_uniforms(GLuint gl_program, ShaderProgram &shader_program, const UniformBuffers &uniform_buffers, const SceGxmProgram &gxm_program, const MemState &mem, bool log_uniforms) {
    R_PROFILE(__func__);

    const SceGxmProgramParameter *const parameters = gxp::program_parameters(gxm_program);
    for (size_t i = 0; i < gxm_program.parameter_count; ++i) {
        const SceGxmProgramParameter &parameter = parameters[i];
        if (parameter.category != SCE_GXM_PARAMETER_CATEGORY_UNIFORM)
            continue;

        auto name = gxp::parameter_name(parameter);

        auto do_supply = [&](const std::string &name, const int arr_size, const int idx) -> bool {
            auto &excluded_uniforms = shader_program.excluded_uniforms;

            if (std::find(excluded_uniforms.begin(), excluded_uniforms.end(), name) != excluded_uniforms.end())
                return false;

            const GLint location = glGetUniformLocation(gl_program, name.c_str());

            if (location < 0) {
                // NOTE: This can happen because the uniform isn't used in the shader, thus optimized away by the shader compiler.
                LOG_WARN("Uniform parameter {} not found in current OpenGL program, it will not be set again.", name);
                excluded_uniforms.push_back(name);
                return false;
            }

            const SceGxmParameterType type = static_cast<SceGxmParameterType>(static_cast<uint16_t>(parameter.type));
            const Ptr<const void> uniform_buffer = uniform_buffers[parameter.container_index];
            if (!uniform_buffer) {
                LOG_WARN("Uniform buffer {} not set for parameter {}.", parameter.container_index, name);
                return false;
            }

            const GLint *src_s32;
            const GLfloat *src_f32;

            const uint8_t *const base = static_cast<const uint8_t *>(uniform_buffer.get(mem));
            switch (type) {
            case SCE_GXM_PARAMETER_TYPE_S32:
                src_s32 = reinterpret_cast<const GLint *>(base + idx * 4); // TODO What offset?
                set_uniform<GLint>(location, parameter.component_count, arr_size, src_s32, name, log_uniforms);
                break;
            case SCE_GXM_PARAMETER_TYPE_F32:
                src_f32 = reinterpret_cast<const GLfloat *>(base + idx * 4); // TODO What offset?
                set_uniform<GLfloat>(location, parameter.component_count, arr_size, src_f32, name, log_uniforms);
                break;
            default:
                LOG_WARN("Type {} not handled for uniform parameter {}.", type, name);
                break;
            }

            return true;
        };

        // An array only
        if (parameter.array_size == 1) {
            do_supply(name, 1, parameter.resource_index);
        } else {
            // There are two types: a matrix listed as a bunch of vector
            //                      Or simply a matrix
            if (glGetUniformLocation(gl_program, name.c_str()) < 0) {
                // Do loop
                for (std::uint32_t i = 0; i < parameter.array_size; i++) {
                    do_supply(name + "_" + std::to_string(i), 1, parameter.resource_index + i * parameter.component_count);
                }
            } else {
                do_supply(name, parameter.array_size, parameter.resource_index);
            }
        }
    }
}

void set_uniforms(GLuint program, const GxmContextState &state, const MemState &mem, bool log_uniforms) {
    R_PROFILE(__func__);

    assert(state.fragment_program);
    assert(state.vertex_program);

    const SceGxmFragmentProgram &fragment_shader = *state.fragment_program.get(mem);
    const SceGxmVertexProgram &vertex_shader = *state.vertex_program.get(mem);
    const SceGxmProgram &fragment_program = *fragment_shader.program.get(mem);
    const SceGxmProgram &vertex_program = *vertex_shader.program.get(mem);

    set_uniforms(program, *fragment_shader.renderer_data, state.fragment_uniform_buffers, fragment_program, mem, log_uniforms);
    set_uniforms(program, *vertex_shader.renderer_data, state.vertex_uniform_buffers, vertex_program, mem, log_uniforms);
}
} // namespace renderer
