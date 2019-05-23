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
static void uniform_2(GLint location, GLsizei count, const T *value);

template <>
void uniform_2<GLfloat>(GLint location, GLsizei count, const GLfloat *value) {
    glUniform2fv(location, count, value);
}

template <>
void uniform_2<GLint>(GLint location, GLsizei count, const GLint *value) {
    glUniform2iv(location, count, value);
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
static void set_uniform(GLint location, size_t component_count, GLsizei array_size, const T *value, const std::string &name, bool is_matrix, bool log_uniforms) {
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

    case 2: {
        switch (array_size) {
        case 1:
            uniform_2<T>(location, array_size, value);
            break;
        }
        break;
    }

    case 4:
        if (is_matrix) {
            uniform_matrix_4<T>(location, array_size / 4, GL_FALSE, value);
            break;
        }

        uniform_4<T>(location, array_size, value);
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

            // This was added for compability with hand-written shaders. GXP shaders don't use matrix, but rather flatten them as array.
            // Hand-written shaders usually convet them to matrix if possible. Until we get rid of hand-written shaders, this will stay here.
            bool is_matrix = false;
            gl::GLenum utype{};

            auto uniform_type_ite = shader_program.uniform_types.find(location);
            if (uniform_type_ite == shader_program.uniform_types.end()) {
                GLint total_uniforms = 0;
                glGetProgramiv(gl_program, GL_ACTIVE_UNIFORMS, &total_uniforms);

                GLint max_uniform_name_length = 0;
                glGetProgramiv(gl_program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_uniform_name_length);

                gl::GLint usize = 0;

                std::string uname;
                uname.resize(max_uniform_name_length);

                GLint written_name_length = 0;

                for (GLint i = 0; i < total_uniforms; i++) {
                    glGetActiveUniform(gl_program, i, max_uniform_name_length, &written_name_length, &usize, &utype, &uname[0]);
                    if (written_name_length == name.length() && strncmp(name.c_str(), uname.c_str(), written_name_length) == 0) {
                        shader_program.uniform_types[location] = utype;
                        break;
                    }
                }
            } else {
                utype = uniform_type_ite->second;
            }

            // TODO: Add more types
            if (utype == GL_FLOAT_MAT2 || utype == GL_FLOAT_MAT3 || utype == GL_FLOAT_MAT4) {
                is_matrix = true;
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
                set_uniform<GLint>(location, parameter.component_count, arr_size, src_s32, name, is_matrix, log_uniforms);
                break;
            case SCE_GXM_PARAMETER_TYPE_F32:
                src_f32 = reinterpret_cast<const GLfloat *>(base + idx * 4); // TODO What offset?
                set_uniform<GLfloat>(location, parameter.component_count, arr_size, src_f32, name, is_matrix, log_uniforms);
                break;
            default:
                LOG_WARN("Type {} not handled for uniform parameter {}.", type, name);
                break;
            }

            return true;
        };

        do_supply(name, parameter.array_size, parameter.resource_index);
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
