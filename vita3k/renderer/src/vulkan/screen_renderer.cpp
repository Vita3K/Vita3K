// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <SDL3/SDL_vulkan.h>

#include "renderer/vulkan/state.h"
#include "util/log.h"
#include "vkutil/vkutil.h"

namespace renderer::vulkan {

ScreenRenderer::ScreenRenderer(VKState &state)
    : state(state) {
}

bool ScreenRenderer::create(SDL_Window *window) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    bool surface_error = SDL_Vulkan_CreateSurface(window, state.instance, nullptr, &surface);
    if (!surface_error) {
        const char *error = SDL_GetError();
        LOG_ERROR("Failed to create vulkan surface. SDL Error: {}.", error);
        return false;
    }
    this->window = window;
    this->surface = vk::SurfaceKHR(surface);

    return true;
}

bool ScreenRenderer::setup() {
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

    create_render_pass();

    create_swapchain();

    // these functions do not need to be called when the swapchain is resized
    create_layout_sync();
    create_surface_image();

    filter = std::make_unique<FXAAScreenFilter>(*this);
    filter->init();

    return true;
}

void ScreenRenderer::create_swapchain() {
    // refresh the capabilities
    surface_capabilities = state.physical_device.getSurfaceCapabilitiesKHR(surface);

    if (surface_capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        extent = surface_capabilities.currentExtent;
    } else {
        int width, height;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        extent.width = std::clamp<uint32_t>(width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
        extent.height = std::clamp<uint32_t>(height, surface_capabilities.minImageExtent.height, surface_capabilities.maxImageExtent.height);
    }

    if (extent.width == 0 || extent.height == 0)
        return;

    swapchain_size = surface_capabilities.minImageCount + 1;
    if (surface_capabilities.maxImageCount != 0)
        swapchain_size = std::min(swapchain_size, surface_capabilities.maxImageCount);

    // Create Swapchain
    {
        vk::ImageUsageFlags surface_usage = vk::ImageUsageFlagBits::eColorAttachment;
        if (surface_capabilities.supportedUsageFlags & vk::ImageUsageFlagBits::eStorage)
            // needed for FSR
            surface_usage |= vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage;

        vk::SwapchainCreateInfoKHR swapchain_info{
            .surface = surface,
            .minImageCount = swapchain_size,
            .imageFormat = surface_format.format,
            .imageColorSpace = surface_format.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = surface_usage,
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
    for (uint32_t i = 0; i < swapchain_size; i++) {
        vk::FramebufferCreateInfo fb_info{
            .renderPass = default_render_pass,
            .width = extent.width,
            .height = extent.height,
            .layers = 1
        };
        fb_info.setAttachments(swapchain_views[i]);

        swapchain_framebuffers[i] = state.device.createFramebuffer(fb_info);
    }

    if (filter)
        filter->on_resize();
}

void ScreenRenderer::destroy_swapchain() {
    for (vk::Framebuffer framebuffer : swapchain_framebuffers)
        state.device.destroy(framebuffer);
    swapchain_framebuffers.clear();

    for (vk::ImageView view : swapchain_views)
        state.device.destroy(view);
    swapchain_views.clear();

    if (swapchain) {
        state.device.destroySwapchainKHR(swapchain);
        swapchain = nullptr;
    }
}

void ScreenRenderer::cleanup() {
    state.device.waitIdle();
    for (vk::Framebuffer fb : swapchain_framebuffers)
        state.device.destroy(fb);

    state.device.destroy(default_render_pass);
    state.device.destroy(post_filter_render_pass);

    for (vk::ImageView view : swapchain_views)
        state.device.destroy(view);
    state.device.destroy(swapchain);

    for (uint32_t i = 0; i < swapchain_size; i++) {
        state.device.destroy(fences[i]);
        state.device.destroy(image_acquired_semaphores[i]);
        state.device.destroy(image_ready_semaphores[i]);
    }

    state.instance.destroy(surface);
}

static constexpr uint64_t next_image_timeout = std::numeric_limits<uint64_t>::max();

bool ScreenRenderer::acquire_swapchain_image(bool start_render_pass) {
    vk::Result acquire_result = vk::Result::eErrorOutOfDateKHR;

    current_frame++;
    if (current_frame == swapchain_size)
        current_frame = 0;

    if (swapchain)
        acquire_result = state.device.acquireNextImageKHR(swapchain,
            next_image_timeout, image_acquired_semaphores[current_frame], vk::Fence(), &swapchain_image_idx);

    if (acquire_result != vk::Result::eSuccess) {
        if (acquire_result == vk::Result::eErrorOutOfDateKHR || acquire_result == vk::Result::eSuboptimalKHR) {
            state.device.waitIdle();
            destroy_swapchain();
            int width, height;
            SDL_GetWindowSizeInPixels(window, &width, &height);
            // don't render anything when the window is minimized
            if (width == 0 || height == 0)
                return false;

            create_swapchain();
            if (swapchain)
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
            .renderPass = default_render_pass,
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

void ScreenRenderer::render(vk::ImageView image_view, vk::ImageLayout layout, const Viewport &viewport) {
    if (swapchain_image_idx == ~0 && !acquire_swapchain_image())
        return;

    // we need to apply the screen filter at the right moment (before or after we start the render pass depending on it)
    filter->render(true, image_view, layout, viewport);

    const auto render_pass = filter->need_post_processing_render_pass() ? post_filter_render_pass : default_render_pass;
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

    filter->render(false, image_view, layout, viewport);
}

void ScreenRenderer::swap_window() {
    if (!current_cmd_buffer) {
        swapchain_image_idx = ~0;
        return;
    }

    // first submit the command buffer
    current_cmd_buffer.endRenderPass();
    current_cmd_buffer.end();
    vk::SubmitInfo submit_info{};
    std::array<vk::Semaphore, 1> wait_semaphores = { image_acquired_semaphores[current_frame] };
    std::array<vk::PipelineStageFlags, 1> dst_masks
        = { vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eTransfer };
    submit_info.setWaitSemaphores(wait_semaphores);
    submit_info.setWaitDstStageMask(dst_masks);
    submit_info.setSignalSemaphores(image_ready_semaphores[current_frame]);
    submit_info.setCommandBuffers(current_cmd_buffer);
    state.general_queue.submit(submit_info, fences[swapchain_image_idx]);

    // then present the surface
    vk::PresentInfoKHR present_info{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &image_ready_semaphores[current_frame],
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
    } catch (vk::OutOfDateKHRError &) {
        state.device.waitIdle();
        destroy_swapchain();

        int width, height;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        if (width > 0 && height > 0) {
            create_swapchain();
            if (swapchain)
                need_rebuild = true;
        }
    }

    swapchain_image_idx = ~0;
    current_cmd_buffer = nullptr;
}

void ScreenRenderer::set_filter(const std::string_view &filter) {
    if (this->filter && filter == this->filter->get_name())
        // we are already using this filter
        return;

    this->filter.reset();
    if (filter == "FSR")
        this->filter = std::make_unique<FSRScreenFilter>(*this);
    else if (filter == "FXAA")
        this->filter = std::make_unique<FXAAScreenFilter>(*this);
    else if (filter == "Bicubic")
        this->filter = std::make_unique<BicubicScreenFilter>(*this);
    else if (filter == "Nearest")
        this->filter = std::make_unique<NearestScreenFilter>(*this);
    else
        this->filter = std::make_unique<BilinearScreenFilter>(*this);

    this->filter->init();
}

void ScreenRenderer::create_layout_sync() {
    vk::CommandBufferAllocateInfo cmd_buffer_info{
        .commandPool = state.general_command_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = swapchain_size
    };
    command_buffers = state.device.allocateCommandBuffers(cmd_buffer_info);

    // create fences (in signaled state) and semaphores
    vk::FenceCreateInfo fence_info{
        .flags = vk::FenceCreateFlagBits::eSignaled
    };
    vk::SemaphoreCreateInfo semaphore_info{};
    fences.resize(swapchain_size);
    image_acquired_semaphores.resize(swapchain_size);
    image_ready_semaphores.resize(swapchain_size);
    for (uint32_t i = 0; i < swapchain_size; i++) {
        fences[i] = state.device.createFence(fence_info);
        image_acquired_semaphores[i] = state.device.createSemaphore(semaphore_info);
        image_ready_semaphores[i] = state.device.createSemaphore(semaphore_info);
    }
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
        .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eComputeShader,
        .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
        .srcAccessMask = vk::AccessFlags(),
        // don't forget blending
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite
    };

    vk::RenderPassCreateInfo pass_info{};
    pass_info.setAttachments(color_attachment);
    pass_info.setSubpasses(subpass);
    pass_info.setDependencies(dependency);

    default_render_pass = state.device.createRenderPass(pass_info);

    // renderpass after post processing filter
    color_attachment
        .setLoadOp(vk::AttachmentLoadOp::eLoad)
        .setInitialLayout(vk::ImageLayout::eGeneral);
    post_filter_render_pass = state.device.createRenderPass(pass_info);
}

void ScreenRenderer::create_surface_image() {
    vita_surface.resize(swapchain_size);

    vk::BufferCreateInfo buffer_info{
        // make sure it is big enough
        .size = 1024 * 720 * sizeof(uint32_t),
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
        .sharingMode = vk::SharingMode::eExclusive
    };
    std::tie(vita_surface_staging, vita_surface_staging_alloc) = state.allocator.createBuffer(buffer_info, vkutil::vma_mapped_alloc, vita_surface_staging_info);
}

} // namespace renderer::vulkan
