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

#include <renderer/functions.h>

#include <xxh3.h>

#include <renderer/types.h>
#include <renderer/vulkan/functions.h>
#include <renderer/vulkan/gxm_to_vulkan.h>
#include <renderer/vulkan/state.h>

#include <gxm/types.h>

#include <util/log.h>
#include <vkutil/vkutil.h>

namespace renderer::vulkan {

VKContext::VKContext(VKState &state, MemState &mem)
    : state(state)
    , mem(mem)
    , vertex_stream_ring_buffer(state.allocator, vk::BufferUsageFlagBits::eVertexBuffer, MiB(/*128*/ 64))
    , index_stream_ring_buffer(state.allocator, vk::BufferUsageFlagBits::eIndexBuffer, MiB(64))
    , vertex_uniform_stream_ring_buffer(state.allocator, vk::BufferUsageFlagBits::eStorageBuffer, MiB(/*256*/ 64))
    , fragment_uniform_stream_ring_buffer(state.allocator, vk::BufferUsageFlagBits::eStorageBuffer, MiB(/*256*/ 64))
    , vertex_info_uniform_buffer(state.allocator, vk::BufferUsageFlagBits::eUniformBuffer, MiB(16))
    , fragment_info_uniform_buffer(state.allocator, vk::BufferUsageFlagBits::eUniformBuffer, MiB(16)) {
    memset(&previous_vert_info, 0, sizeof(shader::RenderVertUniformBlockWithMapping));
    memset(&previous_frag_info, 0, sizeof(shader::RenderFragUniformBlockWithMapping));

    if (state.features.support_memory_mapping) {
        // use the default buffer
        std::fill_n(vertex_stream_buffers, SCE_GXM_MAX_VERTEX_STREAMS, state.default_buffer.buffer);

        // also initialize the gpu wait thread
        gpu_request_wait_thread = std::thread(&VKContext::wait_thread_function, this);
    } else {
        // these are not needed when using memory mapping
        vertex_stream_ring_buffer.create();
        index_stream_ring_buffer.create();
        vertex_uniform_stream_ring_buffer.create();
        fragment_uniform_stream_ring_buffer.create();

        std::fill_n(vertex_stream_buffers, SCE_GXM_MAX_VERTEX_STREAMS, vertex_stream_ring_buffer.handle());
    }

    vertex_info_uniform_buffer.create();
    fragment_info_uniform_buffer.create();

    // default values for the viewport and scissors
    viewport = vk::Viewport{
        .x = 0.f,
        .y = 0.f,
        .width = 960.f * state.res_multiplier,
        .height = 544.f * state.res_multiplier,
        .minDepth = 0.f,
        .maxDepth = 1.f
    };
    scissor = vk::Rect2D{
        .offset = { 0, 0 },
        .extent = { 960U * state.res_multiplier, 544U * state.res_multiplier }
    };

    // allocate descriptor pools
    {
        const uint32_t nb_descriptor = state.features.support_memory_mapping ? 2U : 4U;

        std::array<vk::DescriptorPoolSize, 2> pool_sizes = {
            vk::DescriptorPoolSize{ vk::DescriptorType::eUniformBufferDynamic, 2 },
            vk::DescriptorPoolSize{ vk::DescriptorType::eStorageBufferDynamic, 2 },
        };

        vk::DescriptorPoolCreateInfo descriptor_pool_info{
            .maxSets = 1,
            .poolSizeCount = nb_descriptor / 2,
            .pPoolSizes = pool_sizes.data()
        };

        global_descriptor_pool = state.device.createDescriptorPool(descriptor_pool_info);

        // we can also create the descriptor set now
        vk::DescriptorSetAllocateInfo descr_set_info{
            .descriptorPool = global_descriptor_pool
        };
        descr_set_info.setSetLayouts(state.pipeline_cache.uniforms_layout);
        global_set = state.device.allocateDescriptorSets(descr_set_info)[0];

        // update it now (will not be updated after)
        const uint64_t vert_uniform_size = state.features.support_memory_mapping ? sizeof(shader::RenderVertUniformBlockWithMapping) : sizeof(shader::RenderVertUniformBlock);
        const uint64_t frag_uniform_size = state.features.support_memory_mapping ? sizeof(shader::RenderFragUniformBlockWithMapping) : sizeof(shader::RenderFragUniformBlock);
        std::array<vk::DescriptorBufferInfo, 4> buffers_info = {
            vk::DescriptorBufferInfo{
                .buffer = vertex_info_uniform_buffer.handle(),
                .range = vert_uniform_size },
            vk::DescriptorBufferInfo{
                .buffer = fragment_info_uniform_buffer.handle(),
                .range = frag_uniform_size },
            vk::DescriptorBufferInfo{
                .buffer = vertex_uniform_stream_ring_buffer.handle(),
                // TODO: get max range of buffer
                .range = KB(500) },
            vk::DescriptorBufferInfo{
                .buffer = fragment_uniform_stream_ring_buffer.handle(),
                // TODO: get max range of buffer
                .range = KB(500) },
        };

        std::array<vk::WriteDescriptorSet, 4> write_descr;
        for (uint32_t i = 0; i < 4; i++) {
            write_descr[i] = vk::WriteDescriptorSet{
                .dstSet = global_set,
                .dstBinding = i,
                .dstArrayElement = 0,
                .descriptorType = i < 2 ? vk::DescriptorType::eUniformBufferDynamic : vk::DescriptorType::eStorageBufferDynamic
            };
            write_descr[i].setBufferInfo(buffers_info[i]);
        }

        state.device.updateDescriptorSets(nb_descriptor, write_descr.data(), 0, nullptr);
    }

    for (int i = 0; i < MAX_FRAMES_RENDERING; i++) {
        FrameObject &frame = frames[i];

        vk::CommandPoolCreateInfo pool_info{
            .queueFamilyIndex = state.general_family_index
        };

        frame.render_pool = state.device.createCommandPool(pool_info);
        pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
        frame.prerender_pool = state.device.createCommandPool(pool_info);

        std::array<vk::DescriptorPoolSize, 3> pool_sizes = {
            vk::DescriptorPoolSize{ vk::DescriptorType::eStorageImage, 256 },
            vk::DescriptorPoolSize{ vk::DescriptorType::eInputAttachment, 256 },
            vk::DescriptorPoolSize{ vk::DescriptorType::eCombinedImageSampler, 8192 },
        };

        vk::DescriptorPoolCreateInfo descriptor_pool_info{
            .maxSets = 4096
        };
        descriptor_pool_info.setPoolSizes(pool_sizes);

        frame.descriptor_pool = state.device.createDescriptorPool(descriptor_pool_info);

        frame.destroy_queue.init(state.device, state.allocator);
    }
}

VKRenderTarget::VKRenderTarget(VKState &state, vma::Allocator allocator, uint16_t width, uint16_t height, uint16_t samples_per_frame)
    : mask(allocator, width, height, vk::Format::eR8G8B8A8Unorm)
    , color(allocator, width * state.res_multiplier, height * state.res_multiplier, vk::Format::eR8G8B8A8Unorm)
    , depthstencil(allocator, width * state.res_multiplier, height * state.res_multiplier, vk::Format::eD32SfloatS8Uint) {
    this->width = width * state.res_multiplier;
    this->height = height * state.res_multiplier;

    if (state.features.use_mask_bit)
        mask.init_image(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage);

    color.init_image(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eInputAttachment);
    depthstencil.init_image(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc);

    // transition images to their right state (not needed for the mask)
    vk::CommandBuffer cmd_buffer = vkutil::create_single_time_command(state.device, state.general_command_pool);
    // color
    {
        color.transition_to(cmd_buffer, vkutil::ImageLayout::TransferDst);

        vk::ClearColorValue clear_color{ std::array<float, 4>({ 0.968627450f, 0.776470588f, 0.0f, 0.0f }) };
        cmd_buffer.clearColorImage(color.image, vk::ImageLayout::eTransferDstOptimal, clear_color, vkutil::color_subresource_range);
        color.transition_to(cmd_buffer, vkutil::ImageLayout::ColorAttachmentReadWrite);
    }
    // depth stencil
    {
        depthstencil.transition_to(cmd_buffer, vkutil::ImageLayout::TransferDst, vkutil::ds_subresource_range);
        vk::ClearDepthStencilValue clear_value{
            .depth = 1.0,
            .stencil = 0
        };
        cmd_buffer.clearDepthStencilImage(depthstencil.image, vk::ImageLayout::eTransferDstOptimal, clear_value, vkutil::ds_subresource_range);
        depthstencil.transition_to(cmd_buffer, vkutil::ImageLayout::DepthStencilAttachment, vkutil::ds_subresource_range);
    }
    vkutil::end_single_time_command(state.device, state.general_queue, state.general_command_pool, cmd_buffer);

    constexpr uint16_t SCE_GXM_MAX_SCENES_PER_RENDERTARGET = 8;
    // hopefully this will always be enough
    samples_per_frame = std::min<uint16_t>(samples_per_frame + 2, SCE_GXM_MAX_SCENES_PER_RENDERTARGET);

    // the maximum number of fence we will ever need simultaneously is samples_per_frame * MAX_FRAMES_RENDERING
    fences.resize(samples_per_frame * MAX_FRAMES_RENDERING);
    vk::FenceCreateInfo fence_info{};
    for (int i = 0; i < fences.size(); i++)
        fences[i] = state.device.createFence(fence_info);

    for (int i = 0; i < MAX_FRAMES_RENDERING; i++) {
        vk::CommandBufferAllocateInfo buffer_info{
            .commandPool = reinterpret_cast<VKContext *>(state.context)->frames[i].render_pool,
            .commandBufferCount = static_cast<uint32_t>(samples_per_frame)
        };
        cmd_buffers[i] = state.device.allocateCommandBuffers(buffer_info);

        buffer_info.commandPool = reinterpret_cast<VKContext *>(state.context)->frames[i].prerender_pool;
        pre_cmd_buffers[i] = state.device.allocateCommandBuffers(buffer_info);
    }
}

bool create(VKState &state, std::unique_ptr<Context> &context, MemState &mem) {
    context = std::make_unique<VKContext>(state, mem);

    return true;
}

bool create(VKState &state, std::unique_ptr<RenderTarget> &rt, const SceGxmRenderTargetParams &params, const FeatureState &features) {
    rt = std::make_unique<VKRenderTarget>(state, state.allocator, params.width, params.height, params.scenesPerFrame);

    if (state.features.use_mask_bit) {
        vkutil::Image &mask = reinterpret_cast<VKRenderTarget *>(rt.get())->mask;

        // transition it to general
        vk::CommandBuffer cmd_buffer = vkutil::create_single_time_command(state.device, state.general_command_pool);
        mask.transition_to(cmd_buffer, vkutil::ImageLayout::StorageImage);
        vkutil::end_single_time_command(state.device, state.general_queue, state.general_command_pool, cmd_buffer);
    }
    return true;
}

void destroy(VKState &state, std::unique_ptr<RenderTarget> &rt) {
    VKContext &context = *reinterpret_cast<VKContext *>(state.context);
    VKRenderTarget &render_target = *reinterpret_cast<VKRenderTarget *>(rt.get());

    // don't forget to destroy the framebuffers
    state.surface_cache.destroy_associated_framebuffers(&render_target);

    // deferred destroy everything in case some object is still being used
    FrameObject &frame = context.frame();
    frame.destroy_queue.add_image(render_target.color);
    frame.destroy_queue.add_image(render_target.depthstencil);
    if (state.features.use_mask_bit)
        frame.destroy_queue.add_image(render_target.mask);

    for (auto fence : render_target.fences)
        frame.destroy_queue.add(fence);
    for (int i = 0; i < MAX_FRAMES_RENDERING; i++) {
        for (auto cmd_buffer : render_target.cmd_buffers[i])
            frame.destroy_queue.add_cmd_buffer(cmd_buffer, context.frames[i].render_pool);

        for (auto cmd_buffer : render_target.pre_cmd_buffers[i])
            frame.destroy_queue.add_cmd_buffer(cmd_buffer, context.frames[i].prerender_pool);
    }
}

bool create(std::unique_ptr<VertexProgram> &vp, VKState &state, const SceGxmProgram &program) {
    vp = std::make_unique<VertexProgram>();

    return true;
}

bool create(std::unique_ptr<FragmentProgram> &fp, VKState &state, const SceGxmProgram &program, const SceGxmBlendInfo *blend) {
    fp = std::make_unique<VKFragmentProgram>();

    VKFragmentProgram *fp_vk = reinterpret_cast<VKFragmentProgram *>(fp.get());

    // Translate blending.
    // programs using native color can't use traditional blending
    if (blend != nullptr && !program.is_native_color()) {
        vk::ColorComponentFlags color_mask{};
        if (blend->colorMask & SCE_GXM_COLOR_MASK_R)
            color_mask |= vk::ColorComponentFlagBits::eR;
        if (blend->colorMask & SCE_GXM_COLOR_MASK_G)
            color_mask |= vk::ColorComponentFlagBits::eG;
        if (blend->colorMask & SCE_GXM_COLOR_MASK_B)
            color_mask |= vk::ColorComponentFlagBits::eB;
        if (blend->colorMask & SCE_GXM_COLOR_MASK_A)
            color_mask |= vk::ColorComponentFlagBits::eA;

        fp_vk->blending = vk::PipelineColorBlendAttachmentState{
            .blendEnable = (blend->colorFunc != SCE_GXM_BLEND_FUNC_NONE) || (blend->alphaFunc != SCE_GXM_BLEND_FUNC_NONE),
            .srcColorBlendFactor = translate_blend_factor(blend->colorSrc),
            .dstColorBlendFactor = translate_blend_factor(blend->colorDst),
            .colorBlendOp = translate_blend_func(blend->colorFunc),
            .srcAlphaBlendFactor = translate_blend_factor(blend->alphaSrc),
            .dstAlphaBlendFactor = translate_blend_factor(blend->alphaDst),
            .alphaBlendOp = translate_blend_func(blend->alphaFunc),
            .colorWriteMask = color_mask
        };
    } else {
        // default values, only blendEnable and colorWriteMask are useful
        fp_vk->blending = vk::PipelineColorBlendAttachmentState{
            .blendEnable = VK_FALSE,
            .srcColorBlendFactor = vk::BlendFactor::eOne,
            .dstColorBlendFactor = vk::BlendFactor::eZero,
            .colorBlendOp = vk::BlendOp::eAdd,
            .srcAlphaBlendFactor = vk::BlendFactor::eOne,
            .dstAlphaBlendFactor = vk::BlendFactor::eZero,
            .alphaBlendOp = vk::BlendOp::eAdd,
            .colorWriteMask = vk::ColorComponentFlagBits::eR
                | vk::ColorComponentFlagBits::eG
                | vk::ColorComponentFlagBits::eB
                | vk::ColorComponentFlagBits::eA
        };
    }

    // compute blending hash, as it will be used for the pipeline hash
    fp_vk->blending_hash = XXH_INLINE_XXH3_64bits(&fp_vk->blending, sizeof(vk::PipelineColorBlendAttachmentState));
    return true;
}

} // namespace renderer::vulkan