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

#include <renderer/metal/functions.h>

#include "gxm_to_metal.h"
#include "mtl_types.h"
#include "shader_translate.h"

#include <config/state.h>
#include <gxm/functions.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <util/align.h>
#include <util/log.h>

namespace renderer::metal {

void set_uniform_buffer(MetalContext &context, MemState &mem, const ShaderProgram *program, const bool vertex_shader, const int block_num, const int size, Ptr<uint8_t> data) {
    MetalContextImpl &ctx = *context.impl;
    const auto offset = program->uniform_buffer_data_offsets.at(block_num);
    if (offset == static_cast<std::uint32_t>(-1))
        return;

    const uint32_t data_size_upload = std::min<uint32_t>(size, program->uniform_buffer_sizes.at(block_num) * 4);
    const uint32_t offset_start_upload = offset * 4;

    if (vertex_shader) {
        if (!ctx.vertex_uniform_storage_allocated) {
            // Allocate a region for it. Don't worry though, when the shader program is changed
            ctx.vertex_uniform_stream_ring_buffer.allocate(program->max_total_uniform_buffer_storage * 4);
            ctx.vertex_uniform_storage_allocated = true;
        }

        ctx.vertex_uniform_stream_ring_buffer.copy(data_size_upload, data.get(mem), offset_start_upload);
    } else {
        if (!ctx.fragment_uniform_storage_allocated) {
            ctx.fragment_uniform_stream_ring_buffer.allocate(program->max_total_uniform_buffer_storage * 4);
            ctx.fragment_uniform_storage_allocated = true;
        }

        ctx.fragment_uniform_stream_ring_buffer.copy(data_size_upload, data.get(mem), offset_start_upload);
    }
}

// Metal only allows vertex strides which are multiples of 4
static void restride_stream(const uint8_t *&stream, uint32_t &size, uint32_t stride, std::vector<uint8_t> &storage) {
    const uint32_t new_stride = align(stride, 4);
    const uint32_t nb_vertex_input = ((size + stride - 1) / stride);

    storage.resize(static_cast<size_t>(nb_vertex_input) * new_stride);
    for (uint32_t i = 0; i < nb_vertex_input; i++) {
        memcpy(storage.data() + new_stride * i, stream + stride * i, stride);
    }

    stream = storage.data();
    size = nb_vertex_input * new_stride;
}

static void bind_vertex_streams(MetalContextImpl &ctx, MemState &mem) {
    GxmRecordState &state = ctx.owner.record;
    const SceGxmVertexProgram &vertex_program = *state.vertex_program.get(mem);
    VertexProgram *vert = vertex_program.renderer_data.get();

    int max_stream_idx = -1;

    for (const SceGxmVertexAttribute &attribute : vertex_program.attributes) {
        if (!vert->attribute_infos.contains(attribute.regIndex))
            continue;
        max_stream_idx = std::max<int>(max_stream_idx, attribute.streamIndex);
    }
    max_stream_idx++;

    if (max_stream_idx == 0)
        return;

    static thread_local std::vector<uint8_t> restride_storage;

    for (int i = 0; i < max_stream_idx; i++) {
        if (state.vertex_streams[i].data) {
            const uint8_t *stream = state.vertex_streams[i].data.get(mem);
            uint32_t stream_size = static_cast<uint32_t>(state.vertex_streams[i].size);

            const bool restride = vertex_program.streams[i].stride % 4 != 0;
            if (restride)
                restride_stream(stream, stream_size, vertex_program.streams[i].stride, restride_storage);

            ctx.vertex_stream_ring_buffer.allocate(stream_size, stream);
            ctx.vertex_stream_offsets[i] = ctx.vertex_stream_ring_buffer.data_offset;
            ctx.vertex_stream_buffers[i] = ctx.vertex_stream_ring_buffer.buffer;

            state.vertex_streams[i].data = nullptr;
            state.vertex_streams[i].size = 0;
        }
    }

    for (int i = 0; i < max_stream_idx; i++) {
        id<MTLBuffer> buffer = ctx.vertex_stream_buffers[i];
        const uint32_t offset = ctx.vertex_stream_offsets[i];
        if (ctx.bound_vertex_buffers[i] == buffer) {
            if (ctx.bound_vertex_offsets[i] != offset) {
                [ctx.encoder setVertexBufferOffset:offset atIndex:i];
                ctx.bound_vertex_offsets[i] = offset;
            }
        } else {
            [ctx.encoder setVertexBuffer:buffer offset:offset atIndex:i];
            ctx.bound_vertex_buffers[i] = buffer;
            ctx.bound_vertex_offsets[i] = offset;
        }
    }
}

static void bind_textures(MetalContextImpl &ctx, MemState &mem) {
    MetalDevice &dev = *ctx.state.device;

    const uint16_t vertex_textures_count = ctx.owner.record.vertex_program.get(mem)->renderer_data->texture_count;
    const uint16_t fragment_texture_count = ctx.owner.record.fragment_program.get(mem)->renderer_data->texture_count;

    id<MTLTexture> __unsafe_unretained textures[SCE_GXM_MAX_TEXTURE_UNITS];
    id<MTLSamplerState> __unsafe_unretained samplers[SCE_GXM_MAX_TEXTURE_UNITS];

    if (ctx.vertex_textures_dirty || vertex_textures_count > ctx.bound_vertex_texture_count) {
        for (uint32_t i = 0; i < vertex_textures_count; i++) {
            const BoundTexture &bound = ctx.vertex_textures[i];
            textures[i] = bound.sampler ? bound.texture : dev.default_texture;
            samplers[i] = bound.sampler ? bound.sampler : dev.default_sampler;
        }
        if (vertex_textures_count > 0) {
            [ctx.encoder setVertexTextures:textures withRange:NSMakeRange(0, vertex_textures_count)];
            [ctx.encoder setVertexSamplerStates:samplers withRange:NSMakeRange(0, vertex_textures_count)];
        }
        ctx.bound_vertex_texture_count = vertex_textures_count;
        ctx.vertex_textures_dirty = false;
    }

    if (ctx.fragment_textures_dirty || fragment_texture_count > ctx.bound_fragment_texture_count) {
        for (uint32_t i = 0; i < fragment_texture_count; i++) {
            const BoundTexture &bound = ctx.fragment_textures[i];
            textures[i] = bound.sampler ? bound.texture : dev.default_texture;
            samplers[i] = bound.sampler ? bound.sampler : dev.default_sampler;
        }
        if (fragment_texture_count > 0) {
            [ctx.encoder setFragmentTextures:textures withRange:NSMakeRange(0, fragment_texture_count)];
            [ctx.encoder setFragmentSamplerStates:samplers withRange:NSMakeRange(0, fragment_texture_count)];
        }
        ctx.bound_fragment_texture_count = fragment_texture_count;
        ctx.fragment_textures_dirty = false;
    }
}

// Metal has no triangle fans, expand the indices into a triangle list
template <typename T>
static uint32_t expand_triangle_fan(MetalContextImpl &ctx, const T *indices, uint32_t count) {
    const uint32_t primitive_count = count > 2 ? count - 2 : 0;
    const uint32_t output_count = primitive_count * 3;
    ctx.index_stream_ring_buffer.allocate(std::max<uint32_t>(output_count * sizeof(T), sizeof(T)));
    T *destination = reinterpret_cast<T *>(static_cast<uint8_t *>(ctx.index_stream_ring_buffer.buffer.contents) + ctx.index_stream_ring_buffer.data_offset);
    for (uint32_t primitive = 0; primitive < primitive_count; primitive++) {
        destination[primitive * 3] = indices[0];
        destination[primitive * 3 + 1] = indices[primitive + 1];
        destination[primitive * 3 + 2] = indices[primitive + 2];
    }
    return output_count;
}

void draw(MetalContext &context, SceGxmPrimitiveType type, SceGxmIndexFormat format,
    Ptr<void> indices, size_t count, uint32_t instance_count, MemState &mem, const Config &config) {
    @autoreleasepool {
        MetalContextImpl &ctx = *context.impl;
        MetalState &state = context.state;
        MetalDevice &dev = *state.device;
        GxmRecordState &record = context.record;

        const SceGxmFragmentProgram &gxm_fragment_program = *record.fragment_program.get(mem);
        void *indices_ptr = indices.get(mem);

        if (count == 0 || instance_count == 0 || !indices_ptr)
            return;

        ctx.check_for_macroblock_change(true);

        const bool maskupdate = gxm_fragment_program.is_maskupdate;
        if (maskupdate && (!state.features.use_mask_bit || !ctx.render_target->impl->mask))
            // the mask can't be emulated
            return;

        if (ctx.in_renderpass && ctx.in_maskupdate_pass != maskupdate)
            ctx.stop_render_pass();

        if (!ctx.in_renderpass)
            ctx.start_render_pass(maskupdate);

        if (!ctx.encoder)
            return;

        if (!maskupdate && ctx.is_first_scene_draw)
            ctx.is_first_scene_draw = false;

        if (!maskupdate && ctx.current_visibility_buffer != nullptr && ctx.current_query_idx != -1 && !ctx.is_in_query)
            ctx.begin_visibility_query();

        // do we need to check for a pipeline change?
        if (ctx.refresh_pipeline || type != ctx.last_primitive) {
            ctx.refresh_pipeline = false;
            ctx.last_primitive = type;

            // We don't want to defer cases where we draw a whole quad over the screen as these draws could be necessary
            // to be able to see anything
            const bool can_be_whole_quad = instance_count == 1 && count <= 6;
            id<MTLRenderPipelineState> new_pipeline = dev.pipeline_cache.retrieve_pipeline(ctx, !can_be_whole_quad, mem);

            if (new_pipeline != ctx.current_pipeline) {
                ctx.current_pipeline = new_pipeline;
                if (new_pipeline)
                    [ctx.encoder setRenderPipelineState:new_pipeline];
            }

            id<MTLDepthStencilState> ds_state = dev.pipeline_cache.retrieve_depth_stencil_state(record, maskupdate);
            if (ds_state != ctx.current_depth_stencil) {
                ctx.current_depth_stencil = ds_state;
                if (ds_state)
                    [ctx.encoder setDepthStencilState:ds_state];
            }
            ctx.raster_state_dirty = true;
        }

        // can happen with asynchronous pipeline compilation
        if (ctx.current_pipeline == nil)
            return;

        if (ctx.raster_state_dirty) {
            [ctx.encoder setCullMode:translate_cull_mode(record.cull_mode)];
            [ctx.encoder setTriangleFillMode:translate_polygon_mode(record.front_polygon_mode)];
            ctx.raster_state_dirty = false;
        }

        if (config.log_active_shaders) {
            const std::string hash_text_f = hex_string(record.fragment_program.get(mem)->renderer_data->hash);
            const std::string hash_text_v = hex_string(record.vertex_program.get(mem)->renderer_data->hash);

            LOG_DEBUG("\nVertex  : {}\nFragment: {}", hash_text_v, hash_text_f);
            LOG_DEBUG("Vertex default uniform buffer: {}\n", spdlog::to_hex(context.ubo_data[0], 16));
            LOG_DEBUG("Fragment default uniform buffer: {}\n", spdlog::to_hex(context.ubo_data[SCE_GXM_REAL_MAX_UNIFORM_BUFFER], 16));
        }

        // update uniforms if needed
        auto &vert_render_data = record.vertex_program.get(mem)->renderer_data;
        auto &frag_render_data = record.fragment_program.get(mem)->renderer_data;

        if (state.features.use_texture_viewport) {
            ctx.curr_vert_ublock.set_texture_count(vert_render_data->texture_count);
            ctx.curr_frag_ublock.set_texture_count(frag_render_data->texture_count);
        }

        auto &vert_ublock = ctx.curr_vert_ublock.base_block;
        vert_ublock.viewport_flip = record.viewport_flip;
        vert_ublock.viewport_flag = (record.viewport_flat) ? 0.0f : 1.0f;
        vert_ublock.z_offset = record.z_offset;
        vert_ublock.z_scale = record.z_scale;
        vert_ublock.screen_width = ctx.render_target->width / state.res_multiplier;
        vert_ublock.screen_height = ctx.render_target->height / state.res_multiplier;

        if (ctx.curr_vert_ublock.changed || memcmp(&ctx.prev_vert_ublock, &vert_ublock, sizeof(vert_ublock)) != 0) {
            ctx.curr_vert_ublock.copy_to(ctx.shader_info_temp);
            ctx.vertex_info_uniform_buffer.allocate(ctx.curr_vert_ublock.get_size(), ctx.shader_info_temp);
            memcpy(&ctx.prev_vert_ublock, &vert_ublock, sizeof(vert_ublock));
        }

        auto &frag_ublock = ctx.curr_frag_ublock.base_block;
        frag_ublock.writing_mask = record.writing_mask;
        frag_ublock.res_multiplier = state.res_multiplier;
        const bool has_msaa = ctx.render_target->multisample_mode;
        const bool has_downscale = record.color_surface.downscale;
        if (has_msaa && !has_downscale)
            frag_ublock.res_multiplier *= 2;
        else if (!has_msaa && has_downscale)
            frag_ublock.res_multiplier /= 2;

        if (ctx.curr_frag_ublock.changed || memcmp(&ctx.prev_frag_ublock, &frag_ublock, sizeof(frag_ublock)) != 0) {
            ctx.curr_frag_ublock.copy_to(ctx.shader_info_temp);
            ctx.fragment_info_uniform_buffer.allocate(ctx.curr_frag_ublock.get_size(), ctx.shader_info_temp);
            memcpy(&ctx.prev_frag_ublock, &frag_ublock, sizeof(frag_ublock));
        }

        // bind the uniform buffers
        [ctx.encoder setVertexBuffer:ctx.vertex_info_uniform_buffer.buffer offset:ctx.vertex_info_uniform_buffer.data_offset atIndex:msl::VERTEX_RENDER_INFO_BUFFER_INDEX];
        [ctx.encoder setVertexBuffer:ctx.vertex_uniform_stream_ring_buffer.buffer offset:ctx.vertex_uniform_stream_ring_buffer.data_offset atIndex:msl::VERTEX_GXM_UNIFORM_BUFFER_INDEX];
        [ctx.encoder setFragmentBuffer:ctx.fragment_info_uniform_buffer.buffer offset:ctx.fragment_info_uniform_buffer.data_offset atIndex:msl::FRAGMENT_RENDER_INFO_BUFFER_INDEX];
        [ctx.encoder setFragmentBuffer:ctx.fragment_uniform_stream_ring_buffer.buffer offset:ctx.fragment_uniform_stream_ring_buffer.data_offset atIndex:msl::FRAGMENT_GXM_UNIFORM_BUFFER_INDEX];

        bind_textures(ctx, mem);

        // Upload index data.
        const MTLIndexType index_type = (format == SCE_GXM_INDEX_FORMAT_U16) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
        const size_t index_size = (format == SCE_GXM_INDEX_FORMAT_U16) ? 2 : 4;

        uint32_t draw_count = static_cast<uint32_t>(count);
        if (type == SCE_GXM_PRIMITIVE_TRIANGLE_FAN) {
            if (format == SCE_GXM_INDEX_FORMAT_U16)
                draw_count = expand_triangle_fan(ctx, static_cast<const uint16_t *>(indices_ptr), draw_count);
            else
                draw_count = expand_triangle_fan(ctx, static_cast<const uint32_t *>(indices_ptr), draw_count);
        } else {
            ctx.index_stream_ring_buffer.allocate(static_cast<uint32_t>(index_size * count), indices_ptr);
        }
        const uint32_t index_offset = ctx.index_stream_ring_buffer.data_offset;

        // bind the vertex streams
        bind_vertex_streams(ctx, mem);

        if (draw_count > 0) {
            [ctx.encoder drawIndexedPrimitives:translate_primitive(type)
                                    indexCount:draw_count
                                     indexType:index_type
                                   indexBuffer:ctx.index_stream_ring_buffer.buffer
                             indexBufferOffset:index_offset
                                 instanceCount:instance_count];
        }

        if (maskupdate && record.writing_mask < 0.5f)
            ctx.render_target->impl->mask_may_reject = true;

        ctx.vertex_uniform_storage_allocated = false;
        ctx.fragment_uniform_storage_allocated = false;
    }
}

} // namespace renderer::metal
