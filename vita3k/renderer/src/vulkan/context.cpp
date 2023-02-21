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

void VKContext::wait_thread_function() {
    // try to wait for multiple fences at the same time if possible
    std::vector<vk::Fence> fences;

    while (true) {
        auto wait_request = request_queue.pop();

        if (!wait_request)
            break;

        std::visit(overloaded{
                       [&](NotificationRequest &request) {
                           fences.push_back(request.fence);

                           if (request.notifications[0].address || request.notifications[1].address) {
                               state.device.waitForFences(fences, VK_TRUE, std::numeric_limits<uint64_t>::max());
                               // don't reset them
                               fences.clear();

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
                           if (!fences.empty()) {
                               state.device.waitForFences(fences, VK_TRUE, std::numeric_limits<uint64_t>::max());
                               fences.clear();
                           }

                           // don't reset them, the reset will be done in the new_frame function
                           // and these fences can still be waited for during texture uploading
                           std::unique_lock<std::mutex> lock(new_frame_mutex);
                           last_frame_waited = request.frame_timestamp;
                           lock.unlock();
                           new_frame_condv.notify_one();
                       } },
            *wait_request);
    }
}

void set_context(VKContext &context, const MemState &mem, VKRenderTarget *rt, const FeatureState &features) {
    if (rt) {
        context.render_target = rt;
    } else {
        // TODO: make context.current_render_target non-const instead of doing this
        context.render_target = const_cast<VKRenderTarget *>(reinterpret_cast<const VKRenderTarget *>(context.current_render_target));
    }

    context.scene_timestamp++;

    SceGxmColorSurface *color_surface_fin = &context.record.color_surface;
    // set these values for the pipeline cache
    context.record.color_base_format = gxm::get_base_format(color_surface_fin->colorFormat);
    vk::Format vk_format = color::translate_format(context.record.color_base_format);

    if (color_surface_fin->gamma && (vk_format == vk::Format::eR8G8B8A8Unorm)) {
        vk_format = vk::Format::eR8G8B8A8Srgb;
    }

    if (color_surface_fin->data.address() == 0) {
        color_surface_fin = nullptr;
        vk_format = vk::Format::eR8G8B8A8Unorm;
    }

    SceGxmDepthStencilSurface *ds_surface_fin = &context.record.depth_stencil_surface;
    if ((ds_surface_fin->depthData.address() == 0) && (ds_surface_fin->stencilData.address() == 0)) {
        ds_surface_fin = nullptr;
    }

    VKState &state = context.state;
    state.surface_cache.set_render_target(rt);

    context.start_recording();

    context.current_render_pass = context.state.pipeline_cache.retrieve_render_pass(vk_format, context.record.depth_stencil_surface.zlsControl);

    context.current_framebuffer = state.surface_cache.retrieve_framebuffer_handle(
        mem, color_surface_fin, ds_surface_fin, context.current_render_pass, &context.current_color_attachment, &context.current_ds_attachment,
        &context.current_framebuffer_height, rt->width, rt->height);

    if (state.features.use_mask_bit)
        sync_mask(context, mem);

    context.start_render_pass();
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
        static bool has_happened = false;
        LOG_WARN_IF(!has_happened, "Render Target is using more scenes per frame than what was planned!");
        has_happened = true;

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

void VKContext::start_render_pass() {
    if (in_renderpass) {
        LOG_ERROR("Starting render pass while already in render pass");
        return;
    }

    if (!is_recording)
        start_recording();

    // make sure we are not keeping any texture from the previous pass
    // (textures can be still bound even though they are not used)
    last_vert_texture_count = ~0;
    last_frag_texture_count = ~0;
    for (int i = 0; i < 16; i++) {
        vertex_textures[i].sampler = nullptr;
        fragment_textures[i].sampler = nullptr;
    }

    vk::RenderPassBeginInfo pass_info{
        .renderPass = current_render_pass,
        .framebuffer = current_framebuffer,
        .renderArea = {
            .offset = { 0, 0 },
            .extent = { render_target->width, render_target->height } }
    };
    std::array<vk::ClearValue, 2> clear_values{};
    // only the depth-stencil attachment may be clear if not force loaded
    clear_values[1].depthStencil = vk::ClearDepthStencilValue{
        .depth = record.depth_stencil_surface.backgroundDepth,
        .stencil = record.depth_stencil_surface.control.content & SceGxmDepthStencilControl::stencil_bits
    };
    pass_info.setClearValues(clear_values);
    render_cmd.beginRenderPass(pass_info, vk::SubpassContents::eInline);

    // create and update the rendertarget descriptor set
    vk::DescriptorSetAllocateInfo descr_set_info{
        .descriptorPool = frame().descriptor_pool
    };
    descr_set_info.setSetLayouts(state.pipeline_cache.attachments_layout);
    rendertarget_set = state.device.allocateDescriptorSets(descr_set_info)[0];

    // creat descriptor set for the whole scene with the mask and the color attachment
    // the mask will only be used if state.features.use_mask_bit is true
    vk::DescriptorImageInfo descr_mask_info{
        .sampler = nullptr,
        .imageView = render_target->mask.view,
        .imageLayout = vk::ImageLayout::eGeneral,
    };
    vk::DescriptorImageInfo descr_color_info{
        .sampler = nullptr,
        .imageView = current_color_attachment->view,
        .imageLayout = vk::ImageLayout::eGeneral,
    };
    std::array<vk::WriteDescriptorSet, 2> write_descr;

    write_descr[0] = {
        .dstSet = rendertarget_set,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorType = vk::DescriptorType::eInputAttachment,
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

    refresh_pipeline = true;
    current_pipeline = nullptr;
    in_renderpass = true;
}

void VKContext::stop_render_pass() {
    if (!in_renderpass) {
        LOG_ERROR("Stopping render pass while not in render pass");
        return;
    }
    render_cmd.endRenderPass();

    in_renderpass = false;
}

void VKContext::stop_recording(const SceGxmNotification &notif1, const SceGxmNotification &notif2) {
    if (!is_recording) {
        LOG_ERROR("Stopping recording while not recording");
        return;
    }

    if (in_renderpass)
        stop_render_pass();
    render_cmd.end();
    prerender_cmd.end();

    vk::Fence fence = render_target->fences[render_target->fence_idx];
    render_target->fence_idx++;
    if (render_target->fence_idx == render_target->fences.size())
        render_target->fence_idx = 0;

    vk::SubmitInfo submit_info{};
    // the prerender cmd must be submitted before the render cmd, the pipeline barriers do the rest
    std::array<vk::CommandBuffer, 2> cmd_buffers = { prerender_cmd, render_cmd };
    submit_info.setCommandBuffers(cmd_buffers);

    state.general_queue.submit(submit_info, fence);
    frame().rendered_fences.push_back(fence);

    if (state.features.support_memory_mapping) {
        // send it to the wait queue
        NotificationRequest request = {
            .notifications = { notif1, notif2 },
            .fence = fence
        };
        request_queue.push(request);
    }

    render_cmd = nullptr;
    prerender_cmd = nullptr;
    is_recording = false;
}

} // namespace renderer::vulkan