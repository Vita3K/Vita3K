// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <util/align.h>
#include <util/log.h>

namespace renderer::vulkan {

void set_uniform_buffer(VKContext &context, const MemState &mem, const ShaderProgram *program, const bool vertex_shader, const int block_num, const int size, Ptr<uint8_t> data) {
    auto offset = program->uniform_buffer_data_offsets.at(block_num);
    if (offset == static_cast<std::uint32_t>(-1)) {
        return;
    }

    if (context.state.features.support_memory_mapping) {
        const uint64_t buffer_address = context.state.get_matching_device_address(data.address());
        if (vertex_shader) {
            context.current_vert_render_info.buffer_addresses[block_num] = buffer_address;
        } else {
            context.current_frag_render_info.buffer_addresses[block_num] = buffer_address;
        }
    } else {
        const uint32_t data_size_upload = std::min<uint32_t>(size, program->uniform_buffer_sizes.at(block_num) * 4);
        const uint32_t offset_start_upload = offset * 4;

        if (vertex_shader) {
            if (!context.vertex_uniform_storage_allocated) {
                // Allocate a region for it. Don't worry though, when the shader program is changed
                context.vertex_uniform_stream_ring_buffer.allocate(program->max_total_uniform_buffer_storage * 4);
                context.vertex_uniform_storage_allocated = true;
            }

            context.vertex_uniform_stream_ring_buffer.copy(context.prerender_cmd, data_size_upload, data.get(mem), offset_start_upload);
        } else {
            if (!context.fragment_uniform_storage_allocated) {
                // Allocate a region for it. Don't worry though, when the shader program is changed
                context.fragment_uniform_stream_ring_buffer.allocate(program->max_total_uniform_buffer_storage * 4);
                context.fragment_uniform_storage_allocated = true;
            }

            context.fragment_uniform_stream_ring_buffer.copy(context.prerender_cmd, data_size_upload, data.get(mem), offset_start_upload);
        }
    }
}

void mid_scene_flush(VKContext &context, const SceGxmNotification notification) {
    // two cases :
    // notification.addr is 0: this means that the mid scene flush must be used as a barrier in the renderpass
    // notification.addr is not 0: this means the app is waiting for this part to be finished to re-use the ressources

    // Note: however, when testing, the barrier inside a pipeline does not work (or not entirely, depending on the GPU)
    // maybe because I'm writing using buffer device addresses, not sure...
    // so for the time being always restart the render pass
    // const bool restart_render_pass = notification.address.address() != 0;
    const bool restart_render_pass = true;

    if (restart_render_pass && context.in_renderpass)
        context.stop_render_pass();

    // in case there is no notification, this will happen in the render pass
    vk::MemoryBarrier barrier{
        .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
        .dstAccessMask = vk::AccessFlagBits::eVertexAttributeRead,
    };
    context.render_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eVertexShader, vk::PipelineStageFlagBits::eVertexInput,
        vk::DependencyFlags(), barrier, {}, {});

    if (restart_render_pass) {
        SceGxmNotification empty_notification = { Ptr<uint32_t>(0), 0 };
        const bool submit = notification.address.address() != 0;
        context.stop_recording(notification, empty_notification, submit);
        context.start_recording();
        context.scene_timestamp++;
    }
}

void new_frame(VKContext &context) {
    if (context.state.features.support_memory_mapping) {
        FrameDoneRequest request = { context.frame_timestamp };
        context.request_queue.push(request);
    }

    context.frame_timestamp++;
    context.current_frame_idx = context.frame_timestamp % MAX_FRAMES_RENDERING;

    vk::Device device = context.state.device;
    FrameObject &frame = context.frame();

    // wait on all fences still present to make sure
    if (!frame.rendered_fences.empty()) {
        // wait for the fences, then reset them

        if (context.state.features.support_memory_mapping) {
            // this will underflow for the first MAX_FRAMES_RENDERING frames
            // but that's not an issue as frame.rendered_fences will be empty
            uint64_t previous_frame_timestamp = context.frame_timestamp - MAX_FRAMES_RENDERING;

            // the wait is done by the wait thread
            std::unique_lock<std::mutex> lock(context.new_frame_mutex);
            context.new_frame_condv.wait(lock, [&]() {
                return context.last_frame_waited >= previous_frame_timestamp;
            });
        } else {
            auto result = device.waitForFences(frame.rendered_fences, VK_TRUE, std::numeric_limits<uint64_t>::max());
            if (result != vk::Result::eSuccess) {
                LOG_ERROR("Could not wait for fences.");
                assert(false);
                return;
            }
        }

        // reset the fences in both case (the wait thread does not do that as they can still be used)
        device.resetFences(frame.rendered_fences);
        frame.rendered_fences.clear();
    }

    device.resetCommandPool(frame.prerender_pool);
    device.resetCommandPool(frame.render_pool);
    device.resetDescriptorPool(frame.descriptor_pool);

    // deferred destruction of the objects
    frame.destroy_queue.destroy_objects();

    context.last_vert_texture_count = ~0;
    context.last_frag_texture_count = ~0;

    frame.frame_timestamp = context.frame_timestamp;
}

#ifdef __APPLE__
// restride vertex attribute binding strides to multiple of 4
// needed for metal because it only allows multiples of 4.
void restride_stream(const uint8_t *&stream, uint32_t &size, uint32_t stride) {
    const uint32_t new_stride = align(stride, 4);
    const uint32_t nb_vertex_input = ((size + stride - 1) / stride);

    uint8_t *new_data = new uint8_t[nb_vertex_input * new_stride];
    for (uint32_t i = 0; i < nb_vertex_input; i++) {
        memcpy(new_data + new_stride * i, stream + stride * i, stride);
    }

    stream = new_data;
    size = nb_vertex_input * new_stride;
}
#endif

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
        .sampler = context.state.default_image.sampler,
        .imageView = context.state.default_image.view,
        .imageLayout = vk::ImageLayout::eGeneral
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

    const uint32_t dynamic_offset_count = state.features.support_memory_mapping ? 2U : 4U;
    const uint32_t dynamic_offsets[] = {
        // GXMRenderVertUniformBlock
        context.vertex_info_uniform_buffer.data_offset,
        // GXMRenderFragUniformBlock
        context.fragment_info_uniform_buffer.data_offset,
        // vertex ssbo
        context.vertex_uniform_stream_ring_buffer.data_offset,
        // fragment ssbo
        context.fragment_uniform_stream_ring_buffer.data_offset
    };

    context.render_cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0,
        descriptors.size(), descriptors.data(), dynamic_offset_count, dynamic_offsets);
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
        if (!vkvert->attribute_infos.contains(attribute.regIndex))
            continue;
        max_stream_idx = std::max<int>(max_stream_idx, attribute.streamIndex);
    }
    max_stream_idx++;

    if (max_stream_idx == 0)
        return;

    for (int i = 0; i < max_stream_idx; i++) {
        if (state.vertex_streams[i].data) {
            if (context.state.features.support_memory_mapping) {
                auto [buffer, offset] = context.state.get_matching_mapping(state.vertex_streams[i].data.cast<void>());

                context.vertex_stream_offsets[i] = offset;
                context.vertex_stream_buffers[i] = buffer;
            } else {
                const uint8_t *stream = state.vertex_streams[i].data.get(mem);
                uint32_t stream_size = state.vertex_streams[i].size;
#ifdef __APPLE__
                // Vulkan allows any stride, but Metal only allows multiples of 4.
                const bool restride = vertex_program.streams[i].stride % 4 != 0;
                if (restride) {
                    restride_stream(stream, stream_size, vertex_program.streams[i].stride);
                }
#endif
                context.vertex_stream_ring_buffer.allocate(context.prerender_cmd, stream_size, stream);
                context.vertex_stream_offsets[i] = context.vertex_stream_ring_buffer.data_offset;

#ifdef __APPLE__
                if (restride) {
                    delete[] stream;
                }
#endif
            }

            state.vertex_streams[i].data = nullptr;
            state.vertex_streams[i].size = 0;
        }
    }

    context.render_cmd.bindVertexBuffers(0, max_stream_idx, context.vertex_stream_buffers, context.vertex_stream_offsets);
}

#ifdef __APPLE__
// convert indices for triangle fans to indices for a triangle list
// needed for metal because it does not support a triangle fan implementation
template <typename T>
void triangle_fan_to_triangle_list(void *&indices, size_t &count) {
    // if N is the number of faces, there are N + 2 indices for triangle fans and 3N indices for triangle list
    if (count < 3)
        // safety check
        return;

    const uint32_t nb_triangle = count - 2;

    T *old_indices = reinterpret_cast<T *>(indices);
    indices = new uint8_t[3 * nb_triangle * sizeof(T)];
    T *curr_indices = reinterpret_cast<T *>(indices);

    for (uint32_t triangle = 0; triangle < nb_triangle; triangle++) {
        curr_indices[0] = old_indices[0];
        curr_indices[1] = old_indices[triangle + 1];
        curr_indices[2] = old_indices[triangle + 2];
        curr_indices += 3;
    }

    count = 3 * nb_triangle;
}
#endif

void draw(VKContext &context, SceGxmPrimitiveType type, SceGxmIndexFormat format,
    Ptr<void> indices, size_t count, uint32_t instance_count, MemState &mem, const Config &config) {
    void *indices_ptr = indices.get(mem);
#ifdef __APPLE__
    bool replaced_indices = (type == SCE_GXM_PRIMITIVE_TRIANGLE_FAN);
    // metal does not support triangle fans
    if (replaced_indices) {
        if (format == SCE_GXM_INDEX_FORMAT_U16) {
            triangle_fan_to_triangle_list<uint16_t>(indices_ptr, count);
        } else {
            triangle_fan_to_triangle_list<uint32_t>(indices_ptr, count);
        }
        type = SCE_GXM_PRIMITIVE_TRIANGLES;
    }
#else
    constexpr bool replaced_indices = false;
#endif

    context.check_for_macroblock_change();

    if (!context.in_renderpass)
        context.start_render_pass();

    if (context.is_first_scene_draw && context.state.features.support_shader_interlock) {
        // update the render pass to load and store the depth and stencil
        context.current_render_pass = context.state.pipeline_cache.retrieve_render_pass(context.current_color_attachment->format, ~0U);
        context.is_first_scene_draw = false;
    }

    const SceGxmFragmentProgram &gxm_fragment_program = *context.record.fragment_program.get(mem);
    const SceGxmProgram &fragment_program_gxp = *gxm_fragment_program.program.get(mem);
    if (context.state.features.direct_fragcolor && fragment_program_gxp.is_frag_color_used()) {
        // the fragment shader is using programmable blending with a subpass input
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
    } else if (context.state.features.support_shader_interlock
        && fragment_program_gxp.is_frag_color_used() != context.last_draw_was_framebuffer_fetch) {
        // restart the render pass to act as a barrier
        context.render_cmd.endRenderPass();

        if (fragment_program_gxp.is_frag_color_used()) {
            context.curr_renderpass_info.framebuffer = context.current_shader_interlock_framebuffer;
            context.curr_renderpass_info.renderPass = context.current_shader_interlock_pass;
        } else {
            context.curr_renderpass_info.framebuffer = context.current_framebuffer;
            context.curr_renderpass_info.renderPass = context.current_render_pass;
        }

        context.render_cmd.beginRenderPass(context.curr_renderpass_info, vk::SubpassContents::eInline);
        context.last_draw_was_framebuffer_fetch = fragment_program_gxp.is_frag_color_used();
    }

    if (context.current_visibility_buffer != nullptr && context.current_query_idx != -1 && !context.is_in_query) {
        if (context.current_visibility_buffer->queries_used[context.current_query_idx]) {
            static bool has_happened = false;
            LOG_WARN_IF(!has_happened, "Visibility buffer entry is used more than once in a scene");
            has_happened = true;
            // still let this happen, this is a validation error but I think most GPUs should be fine with it
        }
        context.current_visibility_buffer->queries_used[context.current_query_idx] = true;

        context.visibility_max_used_idx = std::max(context.visibility_max_used_idx, context.current_query_idx);

        const vk::QueryControlFlags control_flags = (context.is_query_op_increment && context.state.physical_device_features.occlusionQueryPrecise) ? vk::QueryControlFlagBits::ePrecise : vk::QueryControlFlags();
        context.render_cmd.beginQuery(context.current_visibility_buffer->query_pool, context.current_query_idx, control_flags);
        context.is_in_query = true;
    }

    // do we need to check for a pipeline change?
    if (context.refresh_pipeline || type != context.last_primitive) {
        context.refresh_pipeline = false;
        context.last_primitive = type;
        vk::Pipeline new_pipeline = context.state.pipeline_cache.retrieve_pipeline(context, type, mem);

        if (new_pipeline != context.current_pipeline) {
            context.current_pipeline = new_pipeline;
            context.render_cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, context.current_pipeline);
        }
    }

    if (config.log_active_shaders) {
        const std::string hash_text_f = hex_string(context.record.fragment_program.get(mem)->renderer_data->hash);
        const std::string hash_text_v = hex_string(context.record.vertex_program.get(mem)->renderer_data->hash);

        LOG_DEBUG("\nVertex  : {}\nFragment: {}", hash_text_v, hash_text_f);
        LOG_DEBUG("Vertex default uniform buffer: {}\n", spdlog::to_hex(context.ubo_data[0].begin(), context.ubo_data[0].end(), 16));
        LOG_DEBUG("Fragment default uniform buffer: {}\n", spdlog::to_hex(context.ubo_data[SCE_GXM_REAL_MAX_UNIFORM_BUFFER].begin(), context.ubo_data[SCE_GXM_REAL_MAX_UNIFORM_BUFFER].end(), 16));
    }

    const bool use_memory_mapping = context.state.features.support_memory_mapping;

    shader::RenderVertUniformBlockWithMapping &vert_ublock = context.current_vert_render_info;
    vert_ublock.viewport_flip = context.record.viewport_flip;
    vert_ublock.viewport_flag = (context.record.viewport_flat) ? 0.0f : 1.0f;
    vert_ublock.z_offset = context.record.z_offset;
    vert_ublock.z_scale = context.record.z_scale;
    vert_ublock.screen_width = static_cast<float>(context.render_target->width / context.state.res_multiplier);
    vert_ublock.screen_height = static_cast<float>(context.render_target->height / context.state.res_multiplier);
    const size_t vert_ublock_size = use_memory_mapping ? sizeof(shader::RenderVertUniformBlockWithMapping) : sizeof(shader::RenderVertUniformBlock);

    if (memcmp(&context.previous_vert_info, &vert_ublock, vert_ublock_size) != 0) {
        context.vertex_info_uniform_buffer.allocate(context.prerender_cmd, vert_ublock_size, &vert_ublock);
        memcpy(&context.previous_vert_info, &vert_ublock, vert_ublock_size);
    }

    shader::RenderFragUniformBlockWithMapping &frag_ublock = context.current_frag_render_info;
    frag_ublock.writing_mask = context.record.writing_mask;
    frag_ublock.res_multiplier = static_cast<float>(context.state.res_multiplier);
    const bool has_msaa = context.render_target->multisample_mode;
    const bool has_downscale = context.record.color_surface.downscale;
    if (has_msaa && !has_downscale)
        frag_ublock.res_multiplier *= 2;
    else if (!has_msaa && has_downscale)
        frag_ublock.res_multiplier /= 2;

    const size_t frag_ublock_size = use_memory_mapping ? sizeof(shader::RenderFragUniformBlockWithMapping) : sizeof(shader::RenderFragUniformBlock);

    if (memcmp(&context.previous_frag_info, &frag_ublock, frag_ublock_size) != 0) {
        context.fragment_info_uniform_buffer.allocate(context.prerender_cmd, frag_ublock_size, &frag_ublock);
        memcpy(&context.previous_frag_info, &frag_ublock, frag_ublock_size);
    }

    // create, update and bind descriptors (uniforms and textures)
    draw_bind_descriptors(context, mem);
    // bind the vertex streams
    bind_vertex_streams(context, mem);

    // Upload index data.
    vk::IndexType index_type = (format == SCE_GXM_INDEX_FORMAT_U16) ? vk::IndexType::eUint16 : vk::IndexType::eUint32;
    const size_t index_size = (format == SCE_GXM_INDEX_FORMAT_U16) ? 2 : 4;

    if (context.state.features.support_memory_mapping) {
        auto [buffer, offset] = context.state.get_matching_mapping(indices);
        context.render_cmd.bindIndexBuffer(buffer, offset, index_type);
    } else {
        const size_t index_buffer_size = index_size * count;

        context.index_stream_ring_buffer.allocate(context.prerender_cmd, index_buffer_size, indices_ptr);

        context.render_cmd.bindIndexBuffer(context.index_stream_ring_buffer.handle(), context.index_stream_ring_buffer.data_offset, index_type);
    }

    context.render_cmd.drawIndexed(count, instance_count, 0, 0, 0);

    if (replaced_indices)
        delete[] reinterpret_cast<uint8_t *>(indices_ptr);

    context.vertex_uniform_storage_allocated = false;
    context.fragment_uniform_storage_allocated = false;
}

} // namespace renderer::vulkan
