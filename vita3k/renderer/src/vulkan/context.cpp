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

#include <renderer/vulkan/types.h>

#include <renderer/vulkan/functions.h>
#include <renderer/vulkan/gxm_to_vulkan.h>
#include <renderer/vulkan/state.h>

#include <gxm/functions.h>

#include <util/log.h>
#include <util/overloaded.h>

namespace renderer::vulkan {

void VKContext::wait_thread_function(const MemState &mem) {
    // try to wait for multiple fences at the same time if possible
    std::vector<vk::Fence> fences;

    auto wait_for_fences = [&]() {
        if (!fences.empty()) {
            auto result = state.device.waitForFences(fences, VK_TRUE, std::numeric_limits<uint64_t>::max());
            if (result != vk::Result::eSuccess) {
                LOG_ERROR("Could not wait for fences.");
                assert(false);
                return;
            }
            // don't reset them
            fences.clear();
        }
    };

    while (true) {
        auto wait_request = request_queue.pop();

        if (!wait_request)
            break;

        std::visit(overloaded{
                       [&](FenceWaitRequest &request) {
                           fences.push_back(request.fence);
                       },
                       [&](NotificationRequest &request) {
                           if (request.notifications[0].address || request.notifications[1].address) {
                               wait_for_fences();

                               // same as in handle_sync_surface_data
                               std::unique_lock<std::mutex> lock(state.notification_mutex);

                               if (request.notifications[0].address)
                                   *request.notifications[0].address.get(mem) = request.notifications[0].value;
                               if (request.notifications[1].address)
                                   *request.notifications[1].address.get(mem) = request.notifications[1].value;

                               // unlocking before a notify should be faster
                               lock.unlock();
                               state.notification_ready.notify_all();
                           }
                       },
                       [&](FrameDoneRequest &request) {
                           wait_for_fences();

                           // don't reset them, the reset will be done in the new_frame function
                           // and these fences can still be waited for during texture uploading
                           std::unique_lock<std::mutex> lock(new_frame_mutex);
                           last_frame_waited = request.frame_timestamp;
                           lock.unlock();
                           new_frame_condv.notify_one();
                       },
                       [&](PostSurfaceSyncRequest &request) {
                           wait_for_fences();

                           state.surface_cache.perform_post_surface_sync(mem, request.cache_info);
                       } },
            *wait_request);
    }
}

void set_context(VKContext &context, MemState &mem, VKRenderTarget *rt, const FeatureState &features) {
    context.render_target = rt;
    context.scene_timestamp++;
    context.state.texture_cache.current_scene_timestamp = context.scene_timestamp;

    SceGxmColorSurface *color_surface_fin = &context.record.color_surface;
    // set these values for the pipeline cache
    context.record.color_base_format = gxm::get_base_format(color_surface_fin->colorFormat);
    context.record.is_gamma_corrected = static_cast<bool>(color_surface_fin->gamma);
    vk::Format vk_format = color::translate_format(context.record.color_base_format);

    if (color_surface_fin->gamma && vk_format == vk::Format::eR8G8B8A8Unorm) {
        vk_format = vk::Format::eR8G8B8A8Srgb;
    }

    if (color_surface_fin->data.address() == 0) {
        color_surface_fin = nullptr;

        // set back default values
        vk_format = vk::Format::eR8G8B8A8Unorm;
        context.record.color_surface.downscale = false;
        context.record.is_gamma_corrected = false;
        context.record.is_maskupdate = false;
        context.record.color_base_format = SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8;
    }
    context.current_color_format = vk_format;

    if (rt->multisample_mode && !context.record.color_surface.downscale) {
        // using MSAA without downscaling, emulate this as best as we can by multiplying the width and height of the render target by 2
        rt->width *= 2;
        rt->height *= 2;
    }

    SceGxmDepthStencilSurface *ds_surface_fin = &context.record.depth_stencil_surface;
    if ((ds_surface_fin->depth_data.address() == 0) && (ds_surface_fin->stencil_data.address() == 0)) {
        ds_surface_fin = nullptr;
    }

    VKState &state = context.state;
    state.surface_cache.set_render_target(rt);

    context.start_recording();

    bool force_load = context.record.depth_stencil_surface.force_load;
    bool force_store = context.record.depth_stencil_surface.force_store;
    if (ds_surface_fin == nullptr) {
        // no memory backing the depth stencil
        force_load = false;
        force_store = false;
    }
    if (context.state.features.support_shader_interlock)
        // we must always store the depth stencil
        force_store = true;
    context.current_render_pass = context.state.pipeline_cache.retrieve_render_pass(vk_format, force_load, force_store);
    if (context.state.features.support_shader_interlock)
        // also retrieve / create the shader interlock pass
        context.current_shader_interlock_pass = context.state.pipeline_cache.retrieve_render_pass(vk_format, true, true, true);

    Framebuffer &framebuffer = state.surface_cache.retrieve_framebuffer_handle(mem, color_surface_fin, ds_surface_fin, context.current_render_pass, context.current_shader_interlock_pass, context.current_color_view, context.current_ds_view);
    context.current_framebuffer = framebuffer.standard;
    context.current_shader_interlock_framebuffer = framebuffer.shader_interlock;
    context.current_color_base_image = framebuffer.base_image;

    if (state.features.use_mask_bit)
        sync_mask(context, mem);

    // make sure we are not keeping any texture from the previous pass
    // (textures can be still bound even though they are not used)
    context.last_vert_texture_count = ~0;
    context.last_frag_texture_count = ~0;
    for (int i = 0; i < 16; i++) {
        context.vertex_textures[i].sampler = nullptr;
        context.fragment_textures[i].sampler = nullptr;
    }

    context.is_first_scene_draw = true;
    context.last_macroblock_x = ~0;
    context.last_macroblock_y = ~0;
    context.ignore_macroblock = false;
}

void VKContext::start_recording() {
    if (is_recording) {
        LOG_ERROR("Attempt to start recording while already recording");
        return;
    }

    if (render_target == nullptr) {
        LOG_ERROR("Recording started without a set command buffer");
        return;
    }

    if (render_target->last_used_frame != frame_timestamp) {
        // reset idx if we are in a new frame
        render_target->cmd_buffer_idx = 0;
        render_target->last_used_frame = frame_timestamp;
    }

    // safety check
    if (render_target->cmd_buffer_idx == render_target->cmd_buffers[current_frame_idx].size()) {
        LOG_WARN_ONCE("Render Target is using more scenes per frame than what was planned!");

        // add additional cmd buffers, fences and semaphores
        vk::CommandBufferAllocateInfo cmd_buffer_info{
            .commandPool = frame().render_pool,
            .commandBufferCount = 1
        };
        render_target->cmd_buffers[current_frame_idx].push_back(state.device.allocateCommandBuffers(cmd_buffer_info)[0]);

        cmd_buffer_info.commandPool = frame().prerender_pool;
        render_target->pre_cmd_buffers[current_frame_idx].push_back(state.device.allocateCommandBuffers(cmd_buffer_info)[0]);

        vk::FenceCreateInfo fence_info{};
        // make sure the next fence used is the one we created
        render_target->fences.insert(render_target->fences.begin() + render_target->fence_idx, state.device.createFence(fence_info));
    }

    if (next_fence == nullptr) {
        next_fence = render_target->fences[render_target->fence_idx];
        // only increase the fence index if we used the previous one
        render_target->fence_idx = (render_target->fence_idx + 1) % render_target->fences.size();
    }

    render_cmd = render_target->cmd_buffers[current_frame_idx][render_target->cmd_buffer_idx];
    prerender_cmd = render_target->pre_cmd_buffers[current_frame_idx][render_target->cmd_buffer_idx];
    render_target->cmd_buffer_idx++;

    vk::CommandBufferBeginInfo begin_info{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    };
    render_cmd.begin(begin_info);
    prerender_cmd.begin(begin_info);

    is_recording = true;

    // set all the dynamic state here
    render_cmd.setViewport(0, viewport);
    render_cmd.setScissor(0, scissor);
    sync_depth_bias(*this);
    sync_point_line_width(*this, true);
    sync_stencil_func(*this, false);
    if (record.two_sided == SCE_GXM_TWO_SIDED_ENABLED) {
        sync_stencil_func(*this, true);
    }
}

void VKContext::start_render_pass(bool create_descriptor_set) {
    if (in_renderpass) {
        LOG_ERROR("Starting render pass while already in render pass");
        return;
    }

    if (!is_recording)
        start_recording();

    curr_renderpass_info = {
        .renderPass = current_render_pass,
        .framebuffer = current_framebuffer
    };

    if (render_target->has_macroblock_sync && !ignore_macroblock) {
        // set the render area to the correct macroblock
        curr_renderpass_info.renderArea = {
            .offset = {
                last_macroblock_x * render_target->macroblock_width,
                last_macroblock_y * render_target->macroblock_height },
            .extent = { render_target->macroblock_width, render_target->macroblock_height }
        };
    } else {
        curr_renderpass_info.renderArea = {
            .offset = { 0, 0 },
            .extent = { render_target->width, render_target->height }
        };
    }

    // only the depth-stencil attachment may be clear if not force loaded
    std::array<vk::ClearValue, 2> curr_clear_values{};
    curr_clear_values[1].depthStencil = vk::ClearDepthStencilValue{
        .depth = record.depth_stencil_surface.background_depth,
        .stencil = record.depth_stencil_surface.stencil
    };
    curr_renderpass_info.setClearValues(curr_clear_values);
    render_cmd.beginRenderPass(curr_renderpass_info, vk::SubpassContents::eInline);

    // set the renderpass info ready in case we need to switch between classic and framebuffer fetch usage
    curr_renderpass_info.setClearValues(nullptr);
    last_draw_was_framebuffer_fetch = false;

    refresh_pipeline = true;
    current_pipeline = nullptr;
    in_renderpass = true;

    if (!create_descriptor_set)
        return;

    // create and update the rendertarget descriptor set
    vk::DescriptorSetAllocateInfo descr_set_info{
        .descriptorPool = frame().descriptor_pool
    };
    descr_set_info.setSetLayouts(state.pipeline_cache.attachments_layout);
    rendertarget_set = state.device.allocateDescriptorSets(descr_set_info)[0];

    // creat descriptor set for the whole scene with the mask and the color attachment
    // the mask will only be used if state.features.use_mask_bit is true
    vk::DescriptorImageInfo descr_color_info{
        .sampler = nullptr,
        .imageView = current_color_view,
        .imageLayout = vk::ImageLayout::eGeneral,
    };
    vk::DescriptorImageInfo descr_mask_info{
        .sampler = nullptr,
        .imageView = render_target->mask.view,
        .imageLayout = vk::ImageLayout::eGeneral,
    };
    std::array<vk::WriteDescriptorSet, 2> write_descr;

    const vk::DescriptorType input_type = state.features.support_shader_interlock ? vk::DescriptorType::eStorageImage : vk::DescriptorType::eInputAttachment;
    write_descr[0] = {
        .dstSet = rendertarget_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorType = input_type,
    };
    write_descr[0].setImageInfo(descr_color_info);
    write_descr[1] = {
        .dstSet = rendertarget_set,
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorType = vk::DescriptorType::eStorageImage,
    };
    write_descr[1].setImageInfo(descr_mask_info);
    state.device.updateDescriptorSets(state.features.use_mask_bit ? 2 : 1, write_descr.data(), 0, nullptr);
}

void VKContext::stop_render_pass() {
    if (!in_renderpass) {
        LOG_ERROR("Stopping render pass while not in render pass");
        return;
    }

    // do this before ending the render pass
    if (is_in_query) {
        render_cmd.endQuery(current_visibility_buffer->query_pool, current_query_idx);
        is_in_query = false;
    }

    render_cmd.endRenderPass();

    in_renderpass = false;
}

void VKContext::stop_recording(const SceGxmNotification &notif1, const SceGxmNotification &notif2, bool submit) {
    if (!is_recording) {
        LOG_ERROR("Stopping recording while not recording");
        return;
    }

    if (in_renderpass)
        stop_render_pass();

    if (visibility_max_used_idx != -1) {
        // get all the entry ranges that were used
        struct VisibilityRange {
            uint32_t offset;
            uint32_t size;
        };
        std::vector<VisibilityRange> ranges;
        bool in_range = false;
        uint32_t range_start = 0;
        for (uint32_t entry = 0; entry <= visibility_max_used_idx + 1; entry++) {
            if (current_visibility_buffer->queries_used[entry] == in_range)
                continue;

            if (in_range) {
                ranges.push_back({ range_start, entry - range_start });
                in_range = false;
            } else {
                range_start = entry;
                in_range = true;
            }
        }

        for (auto &range : ranges) {
            // reset before the beginning of the render pass
            prerender_cmd.resetQueryPool(current_visibility_buffer->query_pool, range.offset, range.size);

            // wait for the range at the end
            // TODO: this will be wrong with upscaling enabled and precise mode set
            render_cmd.copyQueryPoolResults(current_visibility_buffer->query_pool, range.offset, range.size,
                current_visibility_buffer->gpu_buffer, current_visibility_buffer->buffer_offset + range.offset * sizeof(uint32_t),
                sizeof(uint32_t), vk::QueryResultFlagBits::eWait);
        }
        visibility_max_used_idx = -1;
        current_visibility_buffer->queries_used.assign(current_visibility_buffer->size, false);
    }

    ColorSurfaceCacheInfo *surface_info = nullptr;
    if (state.features.support_memory_mapping && !state.disable_surface_sync)
        surface_info = state.surface_cache.perform_surface_sync();

    prerender_cmd.end();
    render_cmd.end();

    // the prerender cmd must be submitted before the render cmd, the pipeline barriers do the rest
    cmdbuffers_to_submit.push_back(prerender_cmd);
    cmdbuffers_to_submit.push_back(render_cmd);

    render_cmd = nullptr;
    prerender_cmd = nullptr;
    is_recording = false;

    if (!submit)
        return;

    if (render_target->multisample_mode && !record.color_surface.downscale) {
        // revert changes made in set_context
        render_target->width /= 2;
        render_target->height /= 2;
    }

    vk::Fence fence = next_fence;
    next_fence = nullptr;

    vk::SubmitInfo submit_info{};
    submit_info.setCommandBuffers(cmdbuffers_to_submit);

    state.general_queue.submit(submit_info, fence);
    cmdbuffers_to_submit.clear();
    frame().rendered_fences.push_back(fence);

    if (state.features.support_memory_mapping) {
        // send it to the wait queue
        request_queue.push(FenceWaitRequest{ fence });

        if (surface_info) {
            request_queue.push(PostSurfaceSyncRequest{ surface_info });
        }

        // the notification must be the last thing sent
        NotificationRequest request = {
            .notifications = { notif1, notif2 },
        };
        request_queue.push(request);
    }
}

void VKContext::check_for_macroblock_change(bool is_draw) {
    if (!render_target->has_macroblock_sync)
        return;

    if (scissor.extent.width == render_target->width && scissor.extent.height == render_target->height) {
        // flower does not specify a scissor adapted to the current macroblock
        // so fallback to the slow path (one scene per draw, can't really do better)
        // TODO: with the feedback loop extension we can do better
        ignore_macroblock = true;
    }

    // use the scissor to know in which macroblock we are
    uint16_t curr_macroblock_x = scissor.offset.x / render_target->macroblock_width;
    uint16_t curr_macroblock_y = scissor.offset.y / render_target->macroblock_height;

    if ((ignore_macroblock && is_draw) || curr_macroblock_x != last_macroblock_x || curr_macroblock_y != last_macroblock_y) {
        // we changed the current macroblock, restart the renderpass
        last_macroblock_x = curr_macroblock_x;
        last_macroblock_y = curr_macroblock_y;

        if (in_renderpass) {
            if (state.features.use_texture_viewport) {
                // no need to copy the texture after this renderpass
                stop_render_pass();
            } else {
                SceGxmNotification empty_notification{};
                stop_recording(empty_notification, empty_notification, false);
                start_recording();
                scene_timestamp++;
            }
        }
    }
}

} // namespace renderer::vulkan