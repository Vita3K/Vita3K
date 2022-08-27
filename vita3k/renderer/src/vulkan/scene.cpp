// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <renderer/vulkan/functions.h>

#include <gxm/functions.h>
#include <renderer/vulkan/gxm_to_vulkan.h>

#include <config/state.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <util/log.h>

namespace renderer::vulkan {

void set_uniform_buffer(VKContext &context, const ShaderProgram *program, const bool vertex_shader, const int block_num, const int size, const uint8_t *data) {
    auto offset = program->uniform_buffer_data_offsets.at(block_num);
    if (offset == static_cast<std::uint32_t>(-1)) {
        return;
    }

    const uint32_t data_size_upload = std::min<uint32_t>(size, program->uniform_buffer_sizes.at(block_num) * 4);
    const uint32_t offset_start_upload = offset * 4;

    if (vertex_shader) {
        if (!context.vertex_uniform_storage_allocated) {
            // Allocate a region for it. Don't worry though, when the shader program is changed
            context.vertex_uniform_stream_ring_buffer.allocate(program->max_total_uniform_buffer_storage * 4);
            context.vertex_uniform_storage_allocated = true;
        }

        context.vertex_uniform_stream_ring_buffer.copy(context.prerender_cmd, data_size_upload, data, offset_start_upload);
    } else {
        if (!context.fragment_uniform_storage_allocated) {
            // Allocate a region for it. Don't worry though, when the shader program is changed
            context.fragment_uniform_stream_ring_buffer.allocate(program->max_total_uniform_buffer_storage * 4);
            context.fragment_uniform_storage_allocated = true;
        }

        context.fragment_uniform_stream_ring_buffer.copy(context.prerender_cmd, data_size_upload, data, offset_start_upload);
    }
}

void new_frame(VKContext &context) {
    context.frame_timestamp++;
    context.current_frame_idx = context.frame_timestamp % MAX_FRAMES_RENDERING;

    vk::Device device = context.state.device;
    FrameObject &frame = context.frame();

    // wait on all fences still present to make sure
    if (!frame.rendered_fences.empty()) {
        // wait for the fences, then reset them
        constexpr uint64_t max_time = std::numeric_limits<uint64_t>::max();
        auto result = device.waitForFences(frame.rendered_fences, VK_TRUE, max_time);
        if (result != vk::Result::eSuccess) {
            LOG_ERROR("Could not wait for fences.");
            assert(false);
            return;
        }
        device.resetFences(frame.rendered_fences);
        frame.rendered_fences.clear();
    }

    device.resetCommandPool(frame.command_pool);
    device.resetDescriptorPool(frame.descriptor_pool);

    // deferred destruction of the objects
    frame.destroy_queue.destroy_objects();

    context.last_vert_texture_count = ~0;
    context.last_frag_texture_count = ~0;
}

void update_sync_target(SceGxmSyncObject *sync, VKRenderTarget *target) {
    SyncExtraData *extra = reinterpret_cast<SyncExtraData *>(sync->extra);
    extra->render_target = target;
}

void update_sync_signal(SceGxmSyncObject *sync) {
    SyncExtraData *extra = reinterpret_cast<SyncExtraData *>(sync->extra);
    if (extra->render_target) {
        // add the render_target current fence to the list of fences
        // this should not need a mutex (the display thread should not be able to clear the fence list now)
        extra->fences.push_back(extra->render_target->fences[extra->render_target->fence_idx]);
    }
}

static void draw_bind_descriptors(VKContext &context, MemState &mem) {
    VKState &state = context.state;

    std::array<vk::DescriptorSet, 4> descriptors;
    descriptors[0] = context.global_set;
    descriptors[1] = context.rendertarget_set;

    const uint16_t vertex_textures_count = reinterpret_cast<VertexProgram *>(
        context.record.vertex_program.get(mem)->renderer_data.get())
                                               ->texture_count;
    const uint16_t fragment_texture_count = reinterpret_cast<VKFragmentProgram *>(
        context.record.fragment_program.get(mem)->renderer_data.get())
                                                ->texture_count;

    vk::PipelineLayout pipeline_layout = state.pipeline_cache.pipeline_layouts[vertex_textures_count][fragment_texture_count];

    // try to use last descriptor if it still matches
    bool need_vert_descr = (vertex_textures_count != context.last_vert_texture_count);
    bool need_frag_descr = (fragment_texture_count != context.last_frag_texture_count);

    context.last_vert_texture_count = vertex_textures_count;
    context.last_frag_texture_count = fragment_texture_count;

    {
        vk::DescriptorSetAllocateInfo descr_set_info{
            .descriptorPool = context.frame().descriptor_pool
        };
        std::vector<vk::DescriptorSetLayout> layouts;
        if (need_vert_descr)
            layouts.push_back(state.pipeline_cache.vertex_textures_layout[vertex_textures_count]);
        if (need_frag_descr)
            layouts.push_back(state.pipeline_cache.fragment_textures_layout[fragment_texture_count]);

        std::vector<vk::DescriptorSet> sets;
        if (!layouts.empty()) {
            descr_set_info.setSetLayouts(layouts);
            sets = state.device.allocateDescriptorSets(descr_set_info);
        }

        int set_idx = 0;
        if (need_vert_descr) {
            context.last_vert_texture_descriptor = sets[set_idx++];
        }
        descriptors[2] = context.last_vert_texture_descriptor;

        if (need_frag_descr) {
            context.last_frag_texture_descriptor = sets[set_idx++];
        }
        descriptors[3] = context.last_frag_texture_descriptor;
    }

    // bind textures
    std::array<vk::WriteDescriptorSet, 16> write_descrs;
    // some default sampler in case a slot has never been set and we read a slot with higher idx
    vk::DescriptorImageInfo default_image_info{
        .sampler = context.default_image.sampler,
        .imageView = context.default_image.view,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
    };

    // vertex
    if (need_vert_descr) {
        for (uint32_t i = 0; i < vertex_textures_count; i++) {
            write_descrs[i] = vk::WriteDescriptorSet{
                .dstSet = descriptors[2],
                .dstBinding = i,
                .dstArrayElement = 0,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            };
            write_descrs[i].setImageInfo(context.vertex_textures[i].sampler ? context.vertex_textures[i] : default_image_info);
        }
        state.device.updateDescriptorSets(vertex_textures_count, write_descrs.data(), 0, nullptr);
    }

    // fragment
    if (need_frag_descr) {
        for (uint32_t i = 0; i < fragment_texture_count; i++) {
            write_descrs[i] = vk::WriteDescriptorSet{
                .dstSet = descriptors[3],
                .dstBinding = i,
                .dstArrayElement = 0,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            };
            write_descrs[i].setImageInfo(context.fragment_textures[i].sampler ? context.fragment_textures[i] : default_image_info);
        }
        state.device.updateDescriptorSets(fragment_texture_count, write_descrs.data(), 0, nullptr);
    }

    uint32_t dynamic_offsets[] = {
        // vertex uniform
        context.vertex_uniform_stream_ring_buffer.data_offset,
        // fragment uniform
        context.fragment_uniform_stream_ring_buffer.data_offset,
        // GXMRenderVertUniformBlock
        context.vertex_info_uniform_buffer.data_offset,
        // GXMRenderFragUniformBlock
        context.fragment_info_uniform_buffer.data_offset
    };

    context.render_cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, descriptors, dynamic_offsets);
}

static void bind_vertex_streams(VKContext &context, MemState &mem) {
    GxmRecordState &state = context.record;
    const SceGxmVertexProgram &vertex_program = *state.vertex_program.get(mem);
    VertexProgram *vkvert = vertex_program.renderer_data.get();

    // we need to do another check here (the same is done in pipeline_cache)
    // because if a game (like Secret of Mana) uses two programs with the same shaders and the same vertex input stripped
    // the pipeline cache won't add stripped symbols for the second program
    if (!vkvert->stripped_symbols_checked) {
        // Insert some symbols here
        const SceGxmProgram *vertex_program_body = vertex_program.program.get(mem);
        if (vertex_program_body && (vertex_program_body->primary_reg_count != 0)) {
            for (std::size_t i = 0; i < vertex_program.attributes.size(); i++) {
                vkvert->attribute_infos.emplace(vertex_program.attributes[i].regIndex, shader::usse::AttributeInformation(static_cast<std::uint16_t>(i), SCE_GXM_PARAMETER_TYPE_F32, false, false, false));
            }
        }

        vkvert->stripped_symbols_checked = true;
    }

    int max_stream_idx = -1;

    for (const SceGxmVertexAttribute &attribute : vertex_program.attributes) {
        if (vkvert->attribute_infos.find(attribute.regIndex) == vkvert->attribute_infos.end())
            continue;
        max_stream_idx = std::max<int>(max_stream_idx, attribute.streamIndex);
    }
    max_stream_idx++;

    if (max_stream_idx == 0)
        return;

    for (std::size_t i = 0; i < max_stream_idx; i++) {
        if (state.vertex_streams[i].data) {
            context.vertex_stream_ring_buffer.allocate(context.prerender_cmd, state.vertex_streams[i].size, state.vertex_streams[i].data);

            context.vertex_buffer_offsets[i] = context.vertex_stream_ring_buffer.data_offset;
            delete[] state.vertex_streams[i].data;

            state.vertex_streams[i].data = nullptr;
            state.vertex_streams[i].size = 0;
        }
    }

    vk::Buffer buffers[SCE_GXM_MAX_VERTEX_STREAMS];
    std::fill_n(buffers, max_stream_idx, context.vertex_stream_ring_buffer.handle());

    context.render_cmd.bindVertexBuffers(0, max_stream_idx, buffers, context.vertex_buffer_offsets.data());
}

void draw(VKContext &context, SceGxmPrimitiveType type, SceGxmIndexFormat format,
    void *indices, size_t count, uint32_t instance_count, MemState &mem, const Config &config) {
    // do we need to check for a pipeline change?
    if (context.refresh_pipeline || !context.in_renderpass || type != context.last_primitive) {
        context.refresh_pipeline = false;
        context.last_primitive = type;
        vk::Pipeline new_pipeline = context.state.pipeline_cache.retrieve_pipeline(context, type, mem);

        if (!context.in_renderpass || new_pipeline != context.current_pipeline) {
            context.current_pipeline = new_pipeline;
            if (!context.in_renderpass)
                context.start_render_pass();

            context.render_cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, context.current_pipeline);
        }
    }

    const SceGxmFragmentProgram &gxm_fragment_program = *context.record.fragment_program.get(mem);
    const SceGxmProgram &fragment_program_gxp = *gxm_fragment_program.program.get(mem);
    if (fragment_program_gxp.is_native_color()) {
        // the fragment shader is using programmable blending
        vk::ImageMemoryBarrier barrier{
            .srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite,
            .dstAccessMask = vk::AccessFlagBits::eInputAttachmentRead,
            .oldLayout = vk::ImageLayout::eGeneral,
            .newLayout = vk::ImageLayout::eGeneral,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = context.current_color_attachment->image,
            .subresourceRange = vkutil::color_subresource_range
        };
        context.render_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader,
            vk::DependencyFlagBits::eByRegion, {}, {}, barrier);
    }

    if (config.log_active_shaders) {
        const std::string hash_text_f = hex_string(context.record.fragment_program.get(mem)->renderer_data->hash);
        const std::string hash_text_v = hex_string(context.record.vertex_program.get(mem)->renderer_data->hash);

        LOG_DEBUG("\nVertex  : {}\nFragment: {}", hash_text_v, hash_text_f);
        LOG_DEBUG("Vertex default uniform buffer: {:a}\n", spdlog::to_hex(context.ubo_data[0].begin(), context.ubo_data[0].end(), 16));
        LOG_DEBUG("Fragment default uniform buffer: {:a}\n", spdlog::to_hex(context.ubo_data[SCE_GXM_REAL_MAX_UNIFORM_BUFFER].begin(), context.ubo_data[SCE_GXM_REAL_MAX_UNIFORM_BUFFER].end(), 16));
    }

    shader::RenderVertUniformBlock &vert_ublock = context.current_vert_render_info;
    vert_ublock.viewport_flip = context.record.viewport_flip;
    vert_ublock.viewport_flag = (context.record.viewport_flat) ? 0.0f : 1.0f;
    vert_ublock.z_offset = context.record.z_offset;
    vert_ublock.z_scale = context.record.z_scale;
    vert_ublock.screen_width = static_cast<float>(context.render_target->width);
    vert_ublock.screen_height = static_cast<float>(context.render_target->height);

    if (memcmp(&context.previous_vert_info, &vert_ublock, sizeof(shader::RenderVertUniformBlock)) != 0) {
        context.vertex_info_uniform_buffer.allocate(context.prerender_cmd, sizeof(shader::RenderVertUniformBlock), &vert_ublock);
        context.previous_vert_info = vert_ublock;
    }

    shader::RenderFragUniformBlock &frag_ublock = context.current_frag_render_info;

    frag_ublock.writing_mask = context.record.writing_mask;
    frag_ublock.use_raw_image = 0;
    frag_ublock.res_multiplier = context.state.res_multiplier;

    if (memcmp(&context.previous_frag_info, &frag_ublock, sizeof(shader::RenderFragUniformBlock)) != 0) {
        context.fragment_info_uniform_buffer.allocate(context.prerender_cmd, sizeof(shader::RenderFragUniformBlock), &frag_ublock);
        context.previous_frag_info = frag_ublock;
    }

    // create, update and bind descriptors (uniforms and textures)
    draw_bind_descriptors(context, mem);
    // bind the vertex streams
    bind_vertex_streams(context, mem);

    // Upload index data.
    vk::IndexType index_type = (format == SCE_GXM_INDEX_FORMAT_U16) ? vk::IndexType::eUint16 : vk::IndexType::eUint32;
    const size_t index_size = (format == SCE_GXM_INDEX_FORMAT_U16) ? 2 : 4;
    const size_t index_buffer_size = index_size * count;

    context.index_stream_ring_buffer.allocate(context.prerender_cmd, index_buffer_size, indices);

    // we can now destroy the indices
    std::uint8_t *indices_u8 = reinterpret_cast<std::uint8_t *>(indices);
    delete[] indices_u8;

    context.render_cmd.bindIndexBuffer(context.index_stream_ring_buffer.handle(), context.index_stream_ring_buffer.data_offset, index_type);

    context.render_cmd.drawIndexed(count, instance_count, 0, 0, 0);

    context.vertex_uniform_storage_allocated = false;
    context.fragment_uniform_storage_allocated = false;
}

} // namespace renderer::vulkan