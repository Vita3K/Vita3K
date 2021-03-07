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

struct GXMRenderUniformBlock {
    float viewport_flip[4];
    float viewport_flag;
    float screen_width;
    float screen_height;
};

void draw(GLState &renderer, GLContext &context, const FeatureState &features, SceGxmPrimitiveType type, SceGxmIndexFormat format, void *indices, size_t count, uint32_t instance_count,
    const MemState &mem, const char *base_path, const char *title_id, const Config &config) {
    R_PROFILE(__func__);

    GLuint program_id = context.last_draw_program;

    const SceGxmFragmentProgram &gxm_fragment_program = *context.record.fragment_program.get(mem);
    const SceGxmProgram &fragment_program_gxp = *gxm_fragment_program.program.get(mem);
    const auto &gl_frag_program = reinterpret_cast<gl::GLFragmentProgram *>(gxm_fragment_program.renderer_data.get());

    // Trying to cache: the last time vs this time shader pair. Does it different somehow?
    // If it's different, we need to switch. Else just stick to it.
    if (context.record.vertex_program.get(mem)->renderer_data->hash != context.last_draw_vertex_program_hash || context.record.fragment_program.get(mem)->renderer_data->hash != context.last_draw_fragment_program_hash) {
        // Need to recompile!
        SharedGLObject program = gl::compile_program(renderer.program_cache, renderer.vertex_shader_cache,
            renderer.fragment_shader_cache, context.record, features, mem, config.spirv_shader, gxm_fragment_program.is_maskupdate, base_path, title_id);

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
        const std::string hash_text_f = hex_string(context.record.fragment_program.get(mem)->renderer_data->hash);
        const std::string hash_text_v = hex_string(context.record.vertex_program.get(mem)->renderer_data->hash);

        LOG_DEBUG("\nVertex  : {}\nFragment: {}", hash_text_v, hash_text_f);
        LOG_DEBUG("Vertex default uniform buffer: {:a}\n", spdlog::to_hex(context.ubo_data[0].begin(), context.ubo_data[0].end(), 16));
        LOG_DEBUG("Fragment default uniform buffer: {:a}\n", spdlog::to_hex(context.ubo_data[SCE_GXM_REAL_MAX_UNIFORM_BUFFER].begin(), context.ubo_data[SCE_GXM_REAL_MAX_UNIFORM_BUFFER].end(), 16));
    }

    if (!program_id) {
        assert(false && "Should never happen");
        glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<GLint *>(&program_id));
    }

    glUseProgram(program_id);

    if (fragment_program_gxp.is_native_color() && features.is_programmable_blending_need_to_bind_color_attachment()) {
        glBindImageTexture(shader::COLOR_ATTACHMENT_TEXTURE_SLOT_IMAGE, context.render_target->color_attachment[0], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
    }

    glBindImageTexture(shader::MASK_TEXTURE_SLOT_IMAGE, context.render_target->masktexture[0], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);

    GXMRenderUniformBlock uniform_block;

    std::memcpy(uniform_block.viewport_flip, context.viewport_flip, sizeof(context.viewport_flip));
    uniform_block.viewport_flag = (context.record.viewport_flat) ? 0.0f : 1.0f;
    uniform_block.screen_width = static_cast<float>(context.record.color_surface.width);
    uniform_block.screen_height = static_cast<float>(context.record.color_surface.height);

    set_uniform_buffer(context, true, SCE_GXM_REAL_MAX_UNIFORM_BUFFER, sizeof(GXMRenderUniformBlock), &uniform_block, false);

    context.vertex_set_requests.clear();
    context.fragment_set_requests.clear();

    // Upload index data.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, context.element_buffer[0]);
    const GLsizeiptr index_size = (format == SCE_GXM_INDEX_FORMAT_U16) ? 2 : 4;

    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_size * count, nullptr, GL_DYNAMIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_size * count, indices, GL_DYNAMIC_DRAW);

    std::uint8_t *indices_u8 = reinterpret_cast<std::uint8_t *>(indices);
    delete[] indices_u8;

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

    if (instance_count == 1) {
        glDrawElements(mode, static_cast<GLsizei>(count), gl_type, nullptr);
    } else {
        glDrawElementsInstanced(mode, static_cast<GLsizei>(count), gl_type, nullptr, instance_count);
    }

    context.last_draw_vertex_program_hash = context.record.vertex_program.get(mem)->renderer_data->hash;
    context.last_draw_fragment_program_hash = context.record.fragment_program.get(mem)->renderer_data->hash;
}
} // namespace renderer::gl
