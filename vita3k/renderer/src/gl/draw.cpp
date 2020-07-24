#include <renderer/functions.h>
#include <renderer/profile.h>

#include <renderer/gl/functions.h>
#include <renderer/gl/state.h>

#include <renderer/state.h>
#include <renderer/types.h>

#include <gxm/types.h>
#include <util/log.h>

#include <features/state.h>
#include <gxm/functions.h>

namespace renderer::gl {
static GLenum translate_primitive(SceGxmPrimitiveType primType) {
    R_PROFILE(__func__);

    switch (primType) {
    case SCE_GXM_PRIMITIVE_TRIANGLES:
        return GL_TRIANGLES;
    case SCE_GXM_PRIMITIVE_TRIANGLE_STRIP:
        return GL_TRIANGLE_STRIP;
    case SCE_GXM_PRIMITIVE_TRIANGLE_FAN:
        return GL_TRIANGLE_FAN;
    case SCE_GXM_PRIMITIVE_LINES:
        return GL_LINES;
    case SCE_GXM_PRIMITIVE_POINTS:
        return GL_POINTS;
    case SCE_GXM_PRIMITIVE_TRIANGLE_EDGES: // Todo: Implement this
        LOG_WARN("Unsupported primitive: SCE_GXM_PRIMITIVE_TRIANGLE_EDGES, using GL_TRIANGLES instead");
        return GL_TRIANGLES;
    }
    return GL_TRIANGLES;
}

void draw(GLState &renderer, GLContext &context, GxmContextState &state, const FeatureState &features, SceGxmPrimitiveType type, SceGxmIndexFormat format, const void *indices, size_t count, const MemState &mem,
    const char *base_path, const char *title_id, const bool log_active_shaders, const bool log_uniforms, const bool do_hardware_flip) {
    R_PROFILE(__func__);

    GLuint program_id = context.last_draw_program;

    // Trying to cache: the last time vs this time shader pair. Does it different somehow?
    // If it's different, we need to switch. Else just stick to it.
    // Pass 1: Check pointer.
    if (state.last_draw_vertex_program.address() != state.vertex_program.address() || state.last_draw_fragment_program.address() != state.fragment_program.address()) {
        // Pass 2: Check content
        if (!state.last_draw_vertex_program || !state.last_draw_fragment_program || (state.last_draw_vertex_program.get(mem)->renderer_data->hash != state.vertex_program.get(mem)->renderer_data->hash) || (state.last_draw_fragment_program.get(mem)->renderer_data->hash != state.fragment_program.get(mem)->renderer_data->hash)) {
            // Need to recompile!
            SharedGLObject program = gl::compile_program(renderer.program_cache, renderer.vertex_shader_cache,
                renderer.fragment_shader_cache, state, features, mem, base_path, title_id);

            if (!program) {
                LOG_ERROR("Fail to get program!");
            }

            // Use it
            program_id = (*program).get();
            context.last_draw_program = program_id;
        } else {
            program_id = context.last_draw_program;
        }
    }

    const SceGxmProgram &fragment_gxp_program = *state.fragment_program.get(mem)->program.get(mem);

    if (log_active_shaders) {
        const std::string hash_text_f = hex_string(state.fragment_program.get(mem)->renderer_data->hash);
        const std::string hash_text_v = hex_string(state.vertex_program.get(mem)->renderer_data->hash);

        LOG_DEBUG("\nVertex  : {}\nFragment: {}", hash_text_v, hash_text_f);
    }

    if (!program_id) {
        assert(false && "Should never happen");
        glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<GLint *>(&program_id));
    }

    glUseProgram(program_id);

    if (!features.use_ubo) {
        const SceGxmFragmentProgram &gxm_fragment_program = *state.fragment_program.get(mem);
        const SceGxmVertexProgram &gxm_vertex_program = *state.vertex_program.get(mem);
        const FragmentProgram &fragment_program = *gxm_fragment_program.renderer_data.get();

        // Set uniforms
        const SceGxmProgram &vertex_program_gxp = *gxm_vertex_program.program.get(mem);
        const SceGxmProgram &fragment_program_gxp = *gxm_fragment_program.program.get(mem);

        gl::GLShaderStatics &vertex_gl_statics = reinterpret_cast<gl::GLVertexProgram *>(gxm_vertex_program.renderer_data.get())->statics;
        gl::GLShaderStatics &fragment_gl_statics = reinterpret_cast<gl::GLFragmentProgram *>(gxm_fragment_program.renderer_data.get())->statics;

        for (auto &vertex_uniform : context.vertex_set_requests) {
            gl::set_uniform(program_id, vertex_program_gxp, vertex_gl_statics, mem, vertex_uniform.parameter, vertex_uniform.data,
                log_uniforms);

            delete vertex_uniform.data;
        }

        for (auto &fragment_uniform : context.fragment_set_requests) {
            gl::set_uniform(program_id, fragment_program_gxp, fragment_gl_statics, mem, fragment_uniform.parameter,
                fragment_uniform.data, log_uniforms);

            delete fragment_uniform.data;
        }
    }

    if (do_hardware_flip) {
        // Try to configure the vertex shader, to output coordinates suited for GXM viewport
        GLuint flip_vec_loc = glGetUniformLocation(program_id, "flip_vec");

        if (flip_vec_loc != -1) {
            // Let's do flipping
            glUniform4fv(flip_vec_loc, 1, context.viewport_flip);
        }
    }

    context.vertex_set_requests.clear();
    context.fragment_set_requests.clear();

    // Upload index data.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, context.element_buffer[0]);
    const GLsizeiptr index_size = (format == SCE_GXM_INDEX_FORMAT_U16) ? 2 : 4;
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_size * count, nullptr, GL_DYNAMIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_size * count, indices, GL_DYNAMIC_DRAW);

    if (fragment_gxp_program.is_native_color()) {
        if (features.should_use_texture_barrier()) {
            // Needs texture barrier
            glTextureBarrier();
        } else if (features.should_use_shader_interlock()) {
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        }
    }

    // Draw.
    const GLenum mode = translate_primitive(type);
    const GLenum gl_type = format == SCE_GXM_INDEX_FORMAT_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
    glDrawElements(mode, static_cast<GLsizei>(count), gl_type, nullptr);

    state.last_draw_vertex_program = state.vertex_program;
    state.last_draw_fragment_program = state.fragment_program;
}
} // namespace renderer::gl
