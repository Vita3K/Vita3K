// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <renderer/profile.h>

#include <renderer/gl/functions.h>
#include <renderer/gl/state.h>

#include <renderer/state.h>
#include <renderer/types.h>

#include <gxm/types.h>
#include <util/log.h>

#include <shader/spirv_recompiler.h>

#include <features/state.h>
#include <gxm/functions.h>

#include <spdlog/fmt/bin_to_hex.h>

#include <bit>
#include <cmath>

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

void draw(GLState &renderer, GLContext &context, const FeatureState &features, SceGxmPrimitiveType type, SceGxmIndexFormat format, void *indices, size_t count, uint32_t instance_count,
    MemState &mem, const Config &config) {
    R_PROFILE(__func__);

    // a draw whose color nothing observes and which leaves depth, stencil and guest memory untouched has no effect
    if (context.record.skip_color_output && !may_write_depth_stencil(context.record) && !may_store_to_memory(context.record, mem)) {
        for (auto &stream : context.record.vertex_streams) {
            stream.data = nullptr;
            stream.size = 0;
        }
        clear_previous_uniform_storage(context);
        return;
    }

    GLuint program_id = context.last_draw_program;

    const SceGxmFragmentProgram &gxm_fragment_program = *context.record.fragment_program.get(mem);
    const SceGxmProgram &fragment_program_gxp = *gxm_fragment_program.program.get(mem);

    if (!renderer.features.use_mask_bit && gxm_fragment_program.is_maskupdate) {
        static bool has_happened = false;
        LOG_ERROR_IF(!has_happened, "Mask bit is disabled!");
        has_happened = true;
        return;
    }

    // Trying to cache: the last time vs this time shader pair. Does it different somehow?
    // If it's different, we need to switch. Else just stick to it.
    if (context.record.vertex_program.get(mem)->renderer_data->hash != context.last_draw_vertex_program_hash || context.record.fragment_program.get(mem)->renderer_data->hash != context.last_draw_fragment_program_hash) {
        // Need to recompile!
        SharedGLObject program = gl::compile_program(renderer, context, context.record, features, mem, config.shader_cache, config.spirv_shader, gxm_fragment_program.is_maskupdate);

        LOG_ERROR_IF(!program, "Fail to get program!");

        // Use it
        program_id = program ? (*program).get() : 0;
        context.last_draw_program = program_id;
    } else {
        program_id = context.last_draw_program;
    }

    if (config.log_active_shaders) {
        const std::string hash_text_f = hex_string(context.record.fragment_program.get(mem)->renderer_data->hash);
        const std::string hash_text_v = hex_string(context.record.vertex_program.get(mem)->renderer_data->hash);

        LOG_DEBUG("\nVertex  : {}\nFragment: {}", hash_text_v, hash_text_f);
        LOG_DEBUG(fmt::runtime("Vertex default uniform buffer: {}\n"), spdlog::to_hex(context.ubo_data[0], 16));
        LOG_DEBUG(fmt::runtime("Fragment default uniform buffer: {}\n"), spdlog::to_hex(context.ubo_data[SCE_GXM_REAL_MAX_UNIFORM_BUFFER], 16));
    }

    if (!program_id) {
        assert(false && "Should never happen");
        glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<GLint *>(&program_id));
    }

    glUseProgram(program_id);

    const bool use_raw_image = renderer.features.preserve_f16_nan_as_u16 && color::is_write_surface_stored_rawly(gxm::get_base_format(context.record.color_surface.colorFormat));

    const SceGxmColorBaseFormat base_format = gxm::get_base_format(context.record.color_surface.colorFormat);
    const GLenum surface_format = color::translate_internal_format(base_format);

    const bool both_side_fragment_program_disabled = (context.record.front_side_fragment_program_mode == SCE_GXM_FRAGMENT_PROGRAM_DISABLED)
        && ((context.record.back_side_fragment_program_mode == SCE_GXM_FRAGMENT_PROGRAM_DISABLED) || (context.record.two_sided == SCE_GXM_TWO_SIDED_DISABLED));

    const bool binds_color_image = fragment_program_gxp.is_frag_color_used() && features.is_programmable_blending_need_to_bind_color_attachment()
        && !both_side_fragment_program_disabled && !use_raw_image;
    if (context.record.color_surface.gamma && renderer.surface_cache.support_srgb_decode && surface_format == GL_RGBA8)
        // sRGB textures cannot be used as images (reads return 0), the shader does the conversion then
        renderer.surface_cache.set_color_surface_srgb(context.record.color_surface.data, !binds_color_image);
    renderer.surface_cache.refresh_gamma_samplings();

    if (fragment_program_gxp.is_frag_color_used() && features.is_programmable_blending_need_to_bind_color_attachment()) {
        if (both_side_fragment_program_disabled) {
            // The color mask does not stop image stores, but stores to an unbound image have no effect
            glBindImageTexture(shader::COLOR_ATTACHMENT_RAW_TEXTURE_SLOT_IMAGE, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16UI);
            glBindImageTexture(shader::COLOR_ATTACHMENT_TEXTURE_SLOT_IMAGE, 0, 0, GL_FALSE, 0, GL_READ_WRITE, surface_format);
        } else if (use_raw_image) {
            glBindImageTexture(shader::COLOR_ATTACHMENT_RAW_TEXTURE_SLOT_IMAGE, context.current_color_attachment, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16UI);
            glBindImageTexture(shader::COLOR_ATTACHMENT_TEXTURE_SLOT_IMAGE, 0, 0, GL_FALSE, 0, GL_READ_WRITE, surface_format);
        } else {
            glBindImageTexture(shader::COLOR_ATTACHMENT_RAW_TEXTURE_SLOT_IMAGE, 0, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA16UI);
            glBindImageTexture(shader::COLOR_ATTACHMENT_TEXTURE_SLOT_IMAGE, context.current_color_attachment, 0, GL_FALSE, 0, GL_READ_WRITE, surface_format);
        }
    }

    if (renderer.features.use_mask_bit) {
        // The shader only reads it, but on AMD integrated GPUs a read-only binding does not see the
        // fast clear done by glClearTexImage in sync_mask: every load returns 0 and all fragments are discarded
        glBindImageTexture(shader::MASK_TEXTURE_SLOT_IMAGE, context.render_target->masktexture[0], 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
        // SPIR-V shaders read the mask from this sampler. The setting can change while programs
        // built with it are cached, so bind it for every draw; GLSL programs do not declare it.
        // Without that unit glActiveTexture would fail and the mask would replace texture 0.
        if (renderer.support_mask_sampler) {
            glActiveTexture(GL_TEXTURE0 + shader::MASK_TEXTURE_SLOT_SAMPLER);
            glBindTexture(GL_TEXTURE_2D, context.render_target->masktexture[0]);
            glActiveTexture(GL_TEXTURE0);
        }
    }

    shader::RenderVertUniformBlock &vert_ublock = context.current_vert_render_info;
    vert_ublock.viewport_flip = context.record.viewport_flip;
    vert_ublock.viewport_flag = (context.record.viewport_flat) ? 0.0f : 1.0f;
    vert_ublock.z_offset = context.record.z_offset;
    vert_ublock.z_scale = context.record.z_scale;
    vert_ublock.screen_width = static_cast<float>(context.record.color_surface.width);
    vert_ublock.screen_height = static_cast<float>(context.record.color_surface.height);

    if (memcmp(&context.previous_vert_info, &vert_ublock, sizeof(shader::RenderVertUniformBlock)) != 0) {
        std::pair<std::uint8_t *, std::size_t> allocated_buffer = context.vertex_info_uniform_buffer.allocate(sizeof(shader::RenderVertUniformBlock));
        std::memcpy(allocated_buffer.first, &vert_ublock, sizeof(shader::RenderVertUniformBlock));

        context.previous_vert_info = vert_ublock;

        glBindBufferRange(GL_UNIFORM_BUFFER, 2, context.vertex_info_uniform_buffer.handle(), allocated_buffer.second, sizeof(shader::RenderVertUniformBlock));
    }

    shader::RenderFragUniformBlock &frag_ublock = context.current_frag_render_info;
    if (both_side_fragment_program_disabled) {
        frag_ublock.front_disabled = 0.0f;
        frag_ublock.back_disabled = 0.0f;
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    } else {
        frag_ublock.front_disabled = (context.record.front_side_fragment_program_mode == SCE_GXM_FRAGMENT_PROGRAM_DISABLED) ? 1.0f : 0.0f;
        if (context.record.two_sided == SCE_GXM_TWO_SIDED_DISABLED)
            frag_ublock.back_disabled = frag_ublock.front_disabled;
        else
            frag_ublock.back_disabled = (context.record.back_side_fragment_program_mode == SCE_GXM_FRAGMENT_PROGRAM_DISABLED) ? 1.0f : 0.0f;
    }

    // nothing can observe the color of this scene, see cmd_handle_draw
    if (context.record.skip_color_output)
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

    if (context.record.is_maskupdate) {
        // Tests bypassed in maskupdate
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_STENCIL_TEST);

        glBindFramebuffer(GL_FRAMEBUFFER, context.render_target->maskbuffer[0]);
    }
    frag_ublock.writing_mask = context.record.writing_mask;
    frag_ublock.use_raw_image = static_cast<float>(use_raw_image);
    // must match the format used by the surface cache
    frag_ublock.is_srgb = (context.record.color_surface.gamma && renderer.surface_cache.support_srgb_decode && surface_format == GL_RGBA8) ? 1.0f : 0.0f;
    frag_ublock.res_multiplier = renderer.res_multiplier;
    frag_ublock.surface_lod = std::log2(renderer.res_multiplier);
    const bool has_msaa = context.render_target->multisample_mode;
    const bool has_downscale = context.record.color_surface.downscale;
    if (has_msaa && !has_downscale)
        frag_ublock.res_multiplier *= 2;
    else if (!has_msaa && has_downscale)
        frag_ublock.res_multiplier /= 2;

    if (memcmp(&context.previous_frag_info, &frag_ublock, sizeof(shader::RenderFragUniformBlock)) != 0) {
        std::pair<std::uint8_t *, std::size_t> allocated_buffer = context.fragment_info_uniform_buffer.allocate(sizeof(shader::RenderFragUniformBlock));
        std::memcpy(allocated_buffer.first, &frag_ublock, sizeof(shader::RenderFragUniformBlock));

        context.previous_frag_info = frag_ublock;

        glBindBufferRange(GL_UNIFORM_BUFFER, 3, context.fragment_info_uniform_buffer.handle(), allocated_buffer.second, sizeof(shader::RenderFragUniformBlock));
    }

    // Upload vertex stream
    sync_vertex_streams_and_attributes(context, context.record, mem);

    GLenum mode = translate_primitive(type);
    const GLenum gl_type = format == SCE_GXM_INDEX_FORMAT_U16 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;

    // A texture barrier only makes the writes of the previous draws visible: when primitives of the same draw overlap,
    // a fragment can read the color before an earlier primitive of the draw wrote it. With high accuracy, draw each
    // primitive on its own with a barrier in between, as the Vulkan renderer does without rasterization order access.
    // This is slow when the GPU is the bottleneck, so standard accuracy keeps a single draw.
    // gl_InstanceID would not count instances drawn one by one, so instanced draws are left as they are.
    const bool order_primitives = config.current_config.high_accuracy && fragment_program_gxp.is_frag_color_used()
        && features.should_use_texture_barrier() && instance_count == 1;
    const bool expand_to_triangles = order_primitives && (mode == GL_TRIANGLE_STRIP || mode == GL_TRIANGLE_FAN);

    // Upload index data.
    const GLsizeiptr index_size = (format == SCE_GXM_INDEX_FORMAT_U16) ? 2 : 4;
    const std::size_t upload_count = expand_to_triangles ? (count > 2 ? (count - 2) * 3 : 0) : count;
    const std::size_t index_buffer_size = index_size * upload_count;

    // Restore context for normal draws
    const auto restore_state = [&]() {
        if (context.record.is_maskupdate) {
            sync_depth_data(context.record);
            sync_stencil_data(context.record, mem);
            // sync_stencil_data writes every stencil bit, restore the write masks
            sync_stencil_func(context.record, true);
            sync_stencil_func(context.record, false);
            glBindFramebuffer(GL_FRAMEBUFFER, context.current_framebuffer);
        }

        if (both_side_fragment_program_disabled || context.record.skip_color_output) {
            sync_blending(context.record, features, mem);
        }
    };

    std::pair<std::uint8_t *, std::size_t> index_gpu_ptr = context.index_stream_ring_buffer.allocate(index_buffer_size);
    if (!index_gpu_ptr.first) {
        LOG_ERROR("Failed to allocate index stream ring buffer data from GPU!");
        restore_state();
        return;
    }

    if (expand_to_triangles) {
        // The same triangles, with their vertices in the same order, as GL makes of the strip or fan
        const auto expand = [&](auto *dst, const auto *src) {
            for (std::size_t i = 0; i + 2 < count; i++) {
                const bool odd = mode == GL_TRIANGLE_STRIP && (i & 1);
                dst[i * 3 + 0] = mode == GL_TRIANGLE_FAN ? src[0] : src[odd ? i + 1 : i];
                dst[i * 3 + 1] = src[odd ? i : i + 1];
                dst[i * 3 + 2] = src[i + 2];
            }
        };
        if (format == SCE_GXM_INDEX_FORMAT_U16)
            expand(reinterpret_cast<std::uint16_t *>(index_gpu_ptr.first), static_cast<const std::uint16_t *>(indices));
        else
            expand(reinterpret_cast<std::uint32_t *>(index_gpu_ptr.first), static_cast<const std::uint32_t *>(indices));
        mode = GL_TRIANGLES;
        count = upload_count;
    } else {
        std::memcpy(index_gpu_ptr.first, indices, index_buffer_size);
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, context.index_stream_ring_buffer.handle());

    const std::uint32_t used_mipmap_units = static_cast<std::uint32_t>(gxm_fragment_program.renderer_data->textures_used.to_ulong()) & context.self_sampling_mipmap_units;
    if (used_mipmap_units) {
        if (features.should_use_shader_interlock())
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);
        glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + std::countr_zero(used_mipmap_units)));
        generate_surface_mipmaps(context.current_color_attachment, renderer.res_multiplier);
    }

    // The shader reads the color attachment whenever it uses the fragment color, so writes from the previous draws
    // must be made visible to it, native color or not
    bool color_synced = false;
    if (fragment_program_gxp.is_frag_color_used() || fragment_program_gxp.is_native_color()) {
        if (features.should_use_shader_interlock() && !config.spirv_shader) {
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
            color_synced = true;
        } else if (features.should_use_texture_barrier()) {
            // Needs texture barrier
            glTextureBarrier();
            color_synced = true;
        }
    }
    if (!color_synced && !context.self_sampling_indices.empty()) {
        if (features.support_texture_barrier) {
            glTextureBarrier();
        } else {
            glActiveTexture(GL_TEXTURE0 + context.self_sampling_indices[0]);
            const float mipmap_res_multiplier = used_mipmap_units ? renderer.res_multiplier : 1.0f;
            std::uint64_t ping_pong = renderer.surface_cache.retrieve_ping_pong_color_surface_texture_handle(context.record.color_surface.data, mipmap_res_multiplier);
            if (ping_pong != 0) {
                for (std::size_t i = 0; i < context.self_sampling_indices.size(); i++) {
                    glActiveTexture(GL_TEXTURE0 + context.self_sampling_indices[i]);
                    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(ping_pong));
                }
            }
        }
    }

    // Draw.
    if (order_primitives) {
        const std::size_t per_primitive = mode == GL_POINTS ? 1 : (mode == GL_LINES ? 2 : 3);
        for (std::size_t first = 0; first + per_primitive <= count; first += per_primitive) {
            // the barrier before the first primitive was issued above
            if (first)
                glTextureBarrier();
            glDrawElements(mode, static_cast<GLsizei>(per_primitive), gl_type, reinterpret_cast<const void *>(index_gpu_ptr.second + first * index_size));
        }
    } else if (instance_count == 1) {
        glDrawElements(mode, static_cast<GLsizei>(count), gl_type, reinterpret_cast<const void *>(index_gpu_ptr.second));
    } else {
        glDrawElementsInstanced(mode, static_cast<GLsizei>(count), gl_type, reinterpret_cast<const void *>(index_gpu_ptr.second), instance_count);
    }

    restore_state();

    context.last_draw_vertex_program_hash = context.record.vertex_program.get(mem)->renderer_data->hash;
    context.last_draw_fragment_program_hash = context.record.fragment_program.get(mem)->renderer_data->hash;

    context.vertex_stream_ring_buffer.draw_call_done();
    context.index_stream_ring_buffer.draw_call_done();
    context.vertex_uniform_stream_ring_buffer.draw_call_done();
    context.fragment_uniform_stream_ring_buffer.draw_call_done();
    context.vertex_info_uniform_buffer.draw_call_done();
    context.fragment_info_uniform_buffer.draw_call_done();

    clear_previous_uniform_storage(context);
}
} // namespace renderer::gl
