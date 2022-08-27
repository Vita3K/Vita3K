// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include "renderer/vulkan/screen_renderer.h"

#include <SDL_vulkan.h>

#include "renderer/vulkan/state.h"
#include "util/log.h"
#include "vkutil/vkutil.h"

namespace renderer::vulkan {

struct screen_vertex {
    float pos[3];
    float uv[2];
};

static constexpr size_t screen_vertex_size = sizeof(screen_vertex);
static constexpr uint32_t screen_vertex_count = 4;

using screen_vertices_t = screen_vertex[screen_vertex_count];

ScreenRenderer::ScreenRenderer(VKState &state)
    : state(state) {
}

bool ScreenRenderer::create(SDL_Window *window) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    bool surface_error = SDL_Vulkan_CreateSurface(window, state.instance, &surface);
    if (!surface_error) {
        const char *error = SDL_GetError();
        LOG_ERROR("Failed to create vulkan surface. SDL Error: {}.", error);
        return false;
    }
    this->window = window;
    this->surface = vk::SurfaceKHR(surface);

    return true;
}

bool ScreenRenderer::setup(const char *base_path) {
    vk::SemaphoreCreateInfo semaphore_info{};
    image_acquired_semaphore = state.device.createSemaphore(semaphore_info);
    image_ready_semaphore = state.device.createSemaphore(semaphore_info);

    const auto surface_formats = state.physical_device.getSurfaceFormatsKHR(surface);
    bool surface_format_found = false;
    for (const auto &format : surface_formats) {
        // actually we don't care that much because we will just be copying what the game rendered
        // rgba8 or bgra8 should be the best as it matches the format output from the vita (we don't care about the swizzle)
        if ((format.format == vk::Format::eB8G8R8A8Unorm || format.format == vk::Format::eR8G8B8A8Unorm)
            && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            surface_format = format;
            surface_format_found = true;
            break;
        }
    }
    if (!surface_format_found)
        surface_format = surface_formats[0];

    // preferred order : mailbox > fifo_relaxed > fifo > whatever
    // the only drawback for mailbox is that it draws more power, so maybe on a portable device use something else
    const auto present_modes = state.physical_device.getSurfacePresentModesKHR(surface);
    // this one should always be available
    present_mode = vk::PresentModeKHR::eImmediate;
    for (const auto &mode : present_modes) {
        if (mode == vk::PresentModeKHR::eMailbox) {
            present_mode = mode;
            break;
        }

        if (mode == vk::PresentModeKHR::eFifoRelaxed) {
            present_mode = mode;
        }
        if (present_mode == vk::PresentModeKHR::eFifoRelaxed)
            continue;

        if (mode == vk::PresentModeKHR::eFifo) {
            present_mode = mode;
        }
    }
    LOG_INFO("Present mode: {}", vk::to_string(present_mode));

    // part of the swapchain that does not need to be rebuilt every time
    const auto builtin_shaders_path = std::string(base_path) + "shaders-builtin/vulkan/";
    shader_vertex = vkutil::load_shader(state.device, builtin_shaders_path + "render_main.vert.spv");
    shader_fragment = vkutil::load_shader(state.device, builtin_shaders_path + "render_main.frag.spv");
    shader_fragment_fxaa = vkutil::load_shader(state.device, builtin_shaders_path + "render_main_fxaa.frag.spv");
    create_render_pass();

    create_swapchain();

    // these functions do not need to be called when the swapchain is resized
    create_layout_sync();
    create_surface_image();

    if (!create_graphics_pipelines())
        return false;

    return true;
}

void ScreenRenderer::create_swapchain() {
    // refresh the capabilities
    surface_capabilities = state.physical_device.getSurfaceCapabilitiesKHR(surface);

    if (surface_capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        extent = surface_capabilities.currentExtent;
    } else {
        int width, height;
        SDL_Vulkan_GetDrawableSize(window, &width, &height);
        extent.width = std::clamp<uint32_t>(width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
        extent.height = std::clamp<uint32_t>(height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
    }

    swapchain_size = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount != 0)
        swapchain_size = std::min(swapchain_size, surface_capabilities.maxImageCount);

    // Create Swapchain
    {
        vk::SwapchainCreateInfoKHR swapchain_info{
            .surface = surface,
            .minImageCount = swapchain_size,
            .imageFormat = surface_format.format,
            .imageColorSpace = surface_format.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = surface_capabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = present_mode,
            .clipped = true,
        };

        swapchain = state.device.createSwapchainKHR(swapchain_info);
    }

    // Get Swapchain Images
    swapchain_images = state.device.getSwapchainImagesKHR(swapchain);
    swapchain_size = swapchain_images.size();

    // Get Image views
    swapchain_views.resize(swapchain_size);
    for (uint32_t i = 0; i < swapchain_size; i++) {
        vk::ImageViewCreateInfo view_info{
            .image = swapchain_images[i],
            .viewType = vk::ImageViewType::e2D,
            .format = surface_format.format,
            .components = vkutil::default_comp_mapping,
            .subresourceRange = vkutil::color_subresource_range
        };

        swapchain_views[i] = state.device.createImageView(view_info);
    }

    swapchain_framebuffers.resize(swapchain_size);
    for (int i = 0; i < swapchain_size; i++) {
        vk::FramebufferCreateInfo fb_info{
            .renderPass = render_pass,
            .width = extent.width,
            .height = extent.height,
            .layers = 1
        };
        fb_info.setAttachments(swapchain_views[i]);

        swapchain_framebuffers[i] = state.device.createFramebuffer(fb_info);
    }
}

void ScreenRenderer::destroy_swapchain() {
    state.device.destroy(pipeline);

    for (vk::Framebuffer framebuffer : swapchain_framebuffers)
        state.device.destroy(framebuffer);
    for (vk::ImageView view : swapchain_views)
        state.device.destroy(view);

    state.device.destroySwapchainKHR(swapchain);
}

void ScreenRenderer::cleanup() {
    for (vk::Framebuffer fb : swapchain_framebuffers)
        state.device.destroy(fb);

    state.device.destroy(pipeline);
    state.device.destroy(pipeline_layout);
    state.device.destroy(shader_fragment);
    state.device.destroy(shader_vertex);
    state.device.destroy(render_pass);

    for (vk::ImageView view : swapchain_views)
        state.device.destroy(view);
    state.device.destroy(swapchain);

    state.device.destroy(image_acquired_semaphore);
    state.allocator.destroyBuffer(vao, vao_allocation);

    state.instance.destroy(surface);
}

static constexpr uint64_t next_image_timeout = std::numeric_limits<uint64_t>::max();

bool ScreenRenderer::acquire_swapchain_image(bool start_render_pass) {
    vk::Result acquire_result = state.device.acquireNextImageKHR(swapchain,
        next_image_timeout, image_acquired_semaphore, vk::Fence(), &swapchain_image_idx);

    if (acquire_result != vk::Result::eSuccess) {
        if (acquire_result == vk::Result::eErrorOutOfDateKHR || acquire_result == vk::Result::eSuboptimalKHR) {
            state.device.waitIdle();
            int width, height;
            SDL_Vulkan_GetDrawableSize(window, &width, &height);
            destroy_swapchain();
            create_swapchain();
            create_graphics_pipelines();
            need_rebuild = true;
        } else {
            LOG_WARN("Failed to get next image. Error: {}", vk::to_string(acquire_result));
        }

        // don't set it to ~0 so that imgui can differentiate when we are in game selection and when acquiring the image failed
        swapchain_image_idx = 0xDEADBEAF;
        return false;
    }

    // wait for the previous frame using this image to finish
    auto result = state.device.waitForFences(fences[swapchain_image_idx], VK_TRUE, next_image_timeout);
    if (result != vk::Result::eSuccess) {
        LOG_ERROR("Could not wait for fences.");
        assert(false);
        return false;
    }
    state.device.resetFences(fences[swapchain_image_idx]);

    // begin the render command
    current_cmd_buffer = command_buffers[swapchain_image_idx];
    current_cmd_buffer.reset();

    {
        vk::CommandBufferBeginInfo begin_info{
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
        };
        current_cmd_buffer.begin(begin_info);
    }

    if (start_render_pass) {
        vk::RenderPassBeginInfo pass_info{
            .renderPass = render_pass,
            .framebuffer = swapchain_framebuffers[swapchain_image_idx],
            .renderArea = {
                .offset = { 0, 0 },
                .extent = extent }
        };
        vk::ClearValue clear_color{
            .color = { std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f } }
        };
        pass_info.setClearValues(clear_color);
        current_cmd_buffer.beginRenderPass(pass_info, vk::SubpassContents::eInline);
    }

    return true;
}

void ScreenRenderer::render(vk::ImageView image_view, vk::ImageLayout layout, std::array<float, 4> &uvs, SceFVector2 &texture_size) {
    if (swapchain_image_idx == ~0 && !acquire_swapchain_image())
        return;

    {
        // if necessary update vao (should not happen often)
        if (uvs != last_uvs[swapchain_image_idx]) {
            screen_vertices_t vertex_buffer_data = {
                { { 1.f, -1.f, 0.0f }, { 1.f, 0.f } },
                { { -1.f, -1.f, 0.0f }, { 0.f, 0.f } },
                { { 1.f, 1.f, 0.0f }, { 1.f, 1.f } },
                { { -1.f, 1.f, 0.0f }, { 0.f, 1.f } },
            };
            vertex_buffer_data[0].uv[0] = uvs[2];
            vertex_buffer_data[0].uv[1] = uvs[1];

            vertex_buffer_data[1].uv[0] = uvs[0];
            vertex_buffer_data[1].uv[1] = uvs[1];

            vertex_buffer_data[2].uv[0] = uvs[2];
            vertex_buffer_data[2].uv[1] = uvs[3];

            vertex_buffer_data[3].uv[0] = uvs[0];
            vertex_buffer_data[3].uv[1] = uvs[3];

            current_cmd_buffer.updateBuffer(vao, swapchain_image_idx * sizeof(screen_vertices_t), sizeof(screen_vertices_t), &vertex_buffer_data);
            last_uvs[swapchain_image_idx] = uvs;
        }
    }

    {
        // update descriptor set
        vk::DescriptorImageInfo descr_image_info{
            .sampler = vita_surface_sampler,
            .imageView = image_view,
            .imageLayout = layout,
        };
        vk::WriteDescriptorSet write_descr{
            .dstSet = descriptor_sets[swapchain_image_idx],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        };
        write_descr.setImageInfo(descr_image_info);
        state.device.updateDescriptorSets(write_descr, {});
    }

    {
        vk::RenderPassBeginInfo pass_info{
            .renderPass = render_pass,
            .framebuffer = swapchain_framebuffers[swapchain_image_idx],
            .renderArea = {
                .offset = { 0, 0 },
                .extent = extent }
        };
        vk::ClearValue clear_color{
            .color = { std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f } }
        };
        pass_info.setClearValues(clear_color);
        current_cmd_buffer.beginRenderPass(pass_info, vk::SubpassContents::eInline);

        vk::DeviceSize offset = swapchain_image_idx * sizeof(screen_vertices_t);
        current_cmd_buffer.bindVertexBuffers(0, vao, offset);

        current_cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, enable_fxaa ? pipeline_fxaa : pipeline);
        current_cmd_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, descriptor_sets[swapchain_image_idx], {});

        if (enable_fxaa) {
            std::array<float, 2> inv_size = { 1 / texture_size.x, 1 / texture_size.y };
            current_cmd_buffer.pushConstants(pipeline_layout, vk::ShaderStageFlagBits::eFragment, 0, 2 * sizeof(float), inv_size.data());
        }

        current_cmd_buffer.draw(4, 1, 0, 0);
    }
}

void ScreenRenderer::swap_window() {
    if (!current_cmd_buffer)
        return;

    // first submit the command buffer
    current_cmd_buffer.endRenderPass();
    current_cmd_buffer.end();
    vk::SubmitInfo submit_info{};
    std::array<vk::Semaphore, 1> wait_semaphores = { image_acquired_semaphore };
    std::array<vk::PipelineStageFlags, 1> dst_masks
        = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    submit_info.setWaitSemaphores(wait_semaphores);
    submit_info.setWaitDstStageMask(dst_masks);
    submit_info.setSignalSemaphores(image_ready_semaphore);
    submit_info.setCommandBuffers(current_cmd_buffer);
    state.general_queue.submit(submit_info, fences[swapchain_image_idx]);

    // then present the surface
    vk::PresentInfoKHR present_info{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &image_ready_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &swapchain_image_idx,
    };
    try {
        auto result = state.general_queue.presentKHR(present_info);
        if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            LOG_ERROR("Could not present KHR.");
            assert(false);
            return;
        }
    } catch (vk::OutOfDateKHRError) {
        state.device.waitIdle();
        int width, height;
        SDL_Vulkan_GetDrawableSize(window, &width, &height);
        destroy_swapchain();
        create_swapchain();
        create_graphics_pipelines();
        need_rebuild = true;
    }

    swapchain_image_idx = ~0;
    current_cmd_buffer = nullptr;
}

void ScreenRenderer::create_layout_sync() {
    vk::DescriptorSetLayoutBinding sampler_layout_binding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
    };
    vk::DescriptorSetLayoutCreateInfo descriptor_info{};
    descriptor_info.setBindings(sampler_layout_binding);
    descriptor_set_layout = state.device.createDescriptorSetLayout(descriptor_info);

    vk::DescriptorPoolSize pool_size{
        .type = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = swapchain_size
    };
    vk::DescriptorPoolCreateInfo pool_info{
        .maxSets = swapchain_size,
    };
    pool_info.setPoolSizes(pool_size);
    descriptor_pool = state.device.createDescriptorPool(pool_info);

    vk::DescriptorSetAllocateInfo descr_set_info{
        .descriptorPool = descriptor_pool,
    };
    std::vector<vk::DescriptorSetLayout> descr_set_layouts(swapchain_size, descriptor_set_layout);
    descr_set_info.setSetLayouts(descr_set_layouts);
    descriptor_sets = state.device.allocateDescriptorSets(descr_set_info);

    vk::PipelineLayoutCreateInfo layout_info{};
    layout_info.setSetLayouts(descriptor_set_layout);
    // add push constant for fxaa pipeline, not used by the normal pipeline
    vk::PushConstantRange push_constant{
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
        .offset = 0,
        .size = 2 * sizeof(float),
    };
    layout_info.setPushConstantRanges(push_constant);
    pipeline_layout = state.device.createPipelineLayout(layout_info);

    vk::CommandBufferAllocateInfo cmd_buffer_info{
        .commandPool = state.general_command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = swapchain_size
    };
    command_buffers = state.device.allocateCommandBuffers(cmd_buffer_info);

    // create fences (in signaled state)
    vk::FenceCreateInfo fence_info{
        .flags = vk::FenceCreateFlagBits::eSignaled
    };
    fences.resize(swapchain_size);
    for (uint32_t i = 0; i < swapchain_size; i++)
        fences[i] = state.device.createFence(fence_info);

    // create vao
    vk::BufferCreateInfo buffer_info{
        .size = sizeof(screen_vertices_t) * swapchain_size,
        .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        .sharingMode = vk::SharingMode::eExclusive
    };
    std::tie(vao, vao_allocation) = state.allocator.createBuffer(buffer_info, vkutil::vma_auto_alloc);

    // create and zero-fill uvs
    last_uvs.resize(swapchain_size);
    std::fill(last_uvs.begin(), last_uvs.end(), std::array<float, 4>());
}

void ScreenRenderer::create_render_pass() {
    vk::AttachmentDescription color_attachment{
        .format = surface_format.format,
        .samples = vk::SampleCountFlagBits::e1,
        .loadOp = vk::AttachmentLoadOp::eClear,
        .storeOp = vk::AttachmentStoreOp::eStore,
        .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
        .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
        .initialLayout = vk::ImageLayout::eUndefined,
        .finalLayout = vk::ImageLayout::ePresentSrcKHR
    };
    vk::AttachmentReference attachment_ref{
        .attachment = 0,
        .layout = vk::ImageLayout::eColorAttachmentOptimal
    };
    vk::SubpassDescription subpass{
        .pipelineBindPoint = vk::PipelineBindPoint::eGraphics
    };
    subpass.setColorAttachments(attachment_ref);

    vk::SubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlags(),
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite
    };

    vk::RenderPassCreateInfo pass_info{};
    pass_info.setAttachments(color_attachment);
    pass_info.setSubpasses(subpass);
    pass_info.setDependencies(dependency);

    render_pass = state.device.createRenderPass(pass_info);
}

vk::Pipeline ScreenRenderer::create_graphics_pipeline_impl(std::array<vk::PipelineShaderStageCreateInfo, 2> &shader_stages) {
    vk::VertexInputBindingDescription binding_descr{
        .binding = 0,
        .stride = screen_vertex_size,
        .inputRate = vk::VertexInputRate::eVertex
    };
    std::array<vk::VertexInputAttributeDescription, 2> attr_descr;
    // pos
    attr_descr[0] = vk::VertexInputAttributeDescription{
        .location = 0,
        .binding = 0,
        .format = vk::Format::eR32G32B32Sfloat,
        .offset = offsetof(screen_vertex, pos)
    };
    // uv
    attr_descr[1] = vk::VertexInputAttributeDescription{
        .location = 1,
        .binding = 0,
        .format = vk::Format::eR32G32Sfloat,
        .offset = offsetof(screen_vertex, uv)
    };
    vk::PipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.setVertexBindingDescriptions(binding_descr);
    vertex_input.setVertexAttributeDescriptions(attr_descr);

    vk::PipelineInputAssemblyStateCreateInfo input_assembly{
        .topology = vk::PrimitiveTopology::eTriangleStrip
    };
    vk::Viewport viewport{
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    {
        // compute viewport now
        const float window_aspect = static_cast<float>(extent.width) / extent.height;
        const float vita_aspect = static_cast<float>(DEFAULT_RES_WIDTH) / DEFAULT_RES_HEIGHT;
        if (window_aspect > vita_aspect) {
            // Window is wide. Pin top and bottom.
            viewport.width = extent.height * vita_aspect;
            viewport.height = static_cast<float>(extent.height);
            viewport.x = (extent.width - viewport.width) / 2.0f;
            viewport.y = 0.0f;
        } else {
            // Window is tall. Pin left and right.
            viewport.width = static_cast<float>(extent.width);
            viewport.height = extent.width / vita_aspect;
            viewport.x = 0.0f;
            viewport.y = (extent.height - viewport.height) / 2;
        }
    }
    vk::Rect2D scissor{
        .offset = { 0, 0 },
        .extent = extent
    };
    vk::PipelineViewportStateCreateInfo viewport_state{};
    viewport_state.setViewports(viewport);
    viewport_state.setScissors(scissor);
    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eNone,
        .frontFace = vk::FrontFace::eClockwise,
        .lineWidth = 1.0f
    };
    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples = vk::SampleCountFlagBits::e1
    };
    vk::PipelineColorBlendAttachmentState blend_attachment{
        .blendEnable = VK_FALSE,
        .colorWriteMask = vkutil::default_color_mask
    };
    vk::PipelineColorBlendStateCreateInfo color_blending{};
    color_blending.setAttachments(blend_attachment);

    vk::GraphicsPipelineCreateInfo pipeline_info{
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &color_blending,
        .layout = pipeline_layout,
        .renderPass = render_pass,
        .subpass = 0
    };
    pipeline_info.setStages(shader_stages);

    const auto result = state.device.createGraphicsPipeline(VK_NULL_HANDLE, pipeline_info);
    if (result.result != vk::Result::eSuccess) {
        LOG_CRITICAL("Failed to create pipeline.");
        return nullptr;
    }

    return result.value;
}

bool ScreenRenderer::create_graphics_pipelines() {
    vk::PipelineShaderStageCreateInfo vert_info{
        .stage = vk::ShaderStageFlagBits::eVertex,
        .module = shader_vertex,
        .pName = "main"
    };
    vk::PipelineShaderStageCreateInfo frag_info{
        .stage = vk::ShaderStageFlagBits::eFragment,
        .module = shader_fragment,
        .pName = "main"
    };
    std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages = { vert_info, frag_info };

    pipeline = create_graphics_pipeline_impl(shader_stages);
    if (!pipeline)
        return false;

    shader_stages[1].module = shader_fragment_fxaa;
    pipeline_fxaa = create_graphics_pipeline_impl(shader_stages);
    if (!pipeline_fxaa)
        return false;

    return true;
}

void ScreenRenderer::create_surface_image() {
    vita_surface.resize(swapchain_size);

    vk::SamplerCreateInfo sampler_info{
        // use linear as it renders a lot better compared to nearest (although it adds a slight blur)
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge,
    };
    vita_surface_sampler = state.device.createSampler(sampler_info);

    vk::BufferCreateInfo buffer_info{
        // make sure it is big enough
        .size = 1024 * 720 * sizeof(uint32_t),
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
        .sharingMode = vk::SharingMode::eExclusive
    };
    std::tie(vita_surface_staging, vita_surface_staging_alloc) = state.allocator.createBuffer(buffer_info, vkutil::vma_mapped_alloc, vita_surface_staging_info);
}

} // namespace renderer::vulkan