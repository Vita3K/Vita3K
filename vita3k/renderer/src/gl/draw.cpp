#include <renderer/functions.h>
#include <renderer/profile.h>

#include <renderer/gl/functions.h>
#include <renderer/gl/state.h>

#include <renderer/state.h>
#include <renderer/types.h>

#include <sstream>

#include <gxm/types.h>
#include <util/log.h>

#include <shader/spirv_recompiler.h>

#include <features/state.h>
#include <gxm/functions.h>

#include <spdlog/fmt/bin_to_hex.h>

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
    const char *base_path, const char *title_id, const Config &config) {
    R_PROFILE(__func__);

    GLuint program_id = context.last_draw_program;

    const SceGxmFragmentProgram &gxm_fragment_program = *state.fragment_program.get(mem);
    const SceGxmProgram &fragment_program_gxp = *gxm_fragment_program.program.get(mem);
    const auto &gl_frag_program = reinterpret_cast<gl::GLFragmentProgram *>(gxm_fragment_program.renderer_data.get());

    // Trying to cache: the last time vs this time shader pair. Does it different somehow?
    // If it's different, we need to switch. Else just stick to it.
    if (state.vertex_program.get(mem)->renderer_data->hash != state.last_draw_vertex_program_hash || state.fragment_program.get(mem)->renderer_data->hash != state.last_draw_fragment_program_hash) {
        // Need to recompile!
        SharedGLObject program = gl::compile_program(renderer.program_cache, renderer.vertex_shader_cache,
            renderer.fragment_shader_cache, state, features, mem, gxm_fragment_program.is_maskupdate, base_path, title_id);

        if (!program) {
            LOG_ERROR("Fail to get program!");
        }

        // Use it
        program_id = (*program).get();
        context.last_draw_program = program_id;
    } else {
        program_id = context.last_draw_program;
    }

    if (config.log_active_shaders) {
        const std::string hash_text_f = hex_string(state.fragment_program.get(mem)->renderer_data->hash);
        const std::string hash_text_v = hex_string(state.vertex_program.get(mem)->renderer_data->hash);

        LOG_DEBUG("\nVertex  : {}\nFragment: {}", hash_text_v, hash_text_f);
        LOG_DEBUG("Vertex default uniform buffer: {:a}\n", spdlog::to_hex(context.ubo_data[0].begin(), context.ubo_data[0].end(), 16));
        LOG_DEBUG("Fragment default uniform buffer: {:a}\n", spdlog::to_hex(context.ubo_data[SCE_GXM_REAL_MAX_UNIFORM_BUFFER].begin(), context.ubo_data[SCE_GXM_REAL_MAX_UNIFORM_BUFFER].end(), 16));
    }

    if (!program_id) {
        assert(false && "Should never happen");
        glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<GLint *>(&program_id));
    }

    const SceGxmProgramParameter *const fragment_params = gxp::program_parameters(fragment_program_gxp);
    std::array<bool, SCE_GXM_MAX_TEXTURE_UNITS> sampler_slot_used = { false };
    for (std::uint32_t i = 0; i < fragment_program_gxp.parameter_count; ++i) {
        const SceGxmProgramParameter &param = fragment_params[i];
        if (param.category != SCE_GXM_PARAMETER_CATEGORY_SAMPLER) {
            continue;
        }
        if (context.need_resync_texture_slots.find(param.resource_index) != context.need_resync_texture_slots.end()) {
            sync_texture(context, state, mem, param.resource_index, config, base_path, title_id);
            context.need_resync_texture_slots.erase(param.resource_index);
        }
        sampler_slot_used[param.resource_index] = true;
    }

    glUseProgram(program_id);

    const auto bind_host_texture = [&](std::string uniform_name, int image_index, GLint texture) {
        GLint loc = glGetUniformLocation(program_id, uniform_name.c_str());
        // It maybe a hand-written shader. So colorAttachment didn't exist
        if (loc != -1) {
            if (features.should_use_shader_interlock()) {
                glBindImageTexture(image_index, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
            } else {
                // Tries to find unused texture slot
                auto it = std::find(sampler_slot_used.begin(), sampler_slot_used.end(), false);
                if (it == sampler_slot_used.end())
                    assert(false); // hahahahahahahahahahaha
                auto index = it - sampler_slot_used.begin();
                sampler_slot_used[index] = true;
                context.need_resync_texture_slots.insert(index);
                glUniform1i(loc, index);
                glActiveTexture(GL_TEXTURE0 + index);
                glBindTexture(GL_TEXTURE_2D, texture);
            }
        }
    };

    if (fragment_program_gxp.is_native_color() && features.is_programmable_blending_need_to_bind_color_attachment()) {
        bind_host_texture("f_colorAttachment", shader::COLOR_ATTACHMENT_TEXTURE_SLOT_IMAGE, context.render_target->color_attachment[0]);
    }
    bind_host_texture("f_mask", shader::MASK_TEXTURE_SLOT_IMAGE, context.render_target->masktexture[0]);

    // Try to configure the vertex shader, to output coordinates suited for GXM viewport
    GLuint flip_vec_loc = glGetUniformLocation(program_id, "flip_vec");

    if (flip_vec_loc != -1) {
        // Let's do flipping
        glUniform4fv(flip_vec_loc, 1, context.viewport_flip);
    }

    const auto set_uniform_if_exists = [&](const std::string &name, float val) {
        GLuint loc = glGetUniformLocation(program_id, name.c_str());
        if (loc != -1) {
            glUniform1f(loc, val);
        }
    };

    set_uniform_if_exists("viewport_flag", state.viewport.enable == SCE_GXM_VIEWPORT_ENABLED ? 1.0f : 0.0f);
    set_uniform_if_exists("screen_width", state.color_surface.width);
    set_uniform_if_exists("screen_height", state.color_surface.height);

    context.vertex_set_requests.clear();
    context.fragment_set_requests.clear();

    // Upload index data.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, context.element_buffer[0]);
    const GLsizeiptr index_size = (format == SCE_GXM_INDEX_FORMAT_U16) ? 2 : 4;

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_size * count, nullptr, GL_DYNAMIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_size * count, indices, GL_DYNAMIC_DRAW);

    const std::uint8_t *indicies_u8 = reinterpret_cast<const std::uint8_t *>(indices);
    delete indicies_u8;

    if (fragment_program_gxp.is_native_color()) {
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

    state.last_draw_vertex_program_hash = state.vertex_program.get(mem)->renderer_data->hash;
    state.last_draw_fragment_program_hash = state.fragment_program.get(mem)->renderer_data->hash;
}
} // namespace renderer::gl
