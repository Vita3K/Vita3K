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

#pragma once

#include <vkutil/objects.h>

#include "screen_filters.h"

#include <memory>

struct SDL_Window;

namespace renderer::vulkan {

struct VKState;

class ScreenRenderer {
public:
    VKState &state;
    SDL_Window *window{};

    vk::SurfaceKHR surface;
    vk::SwapchainKHR swapchain;

    vk::SurfaceCapabilitiesKHR surface_capabilities;
    vk::SurfaceFormatKHR surface_format;
    vk::PresentModeKHR present_mode{};

    vk::Extent2D extent;
    uint32_t swapchain_size{};
    std::vector<vk::Image> swapchain_images;
    std::vector<vk::ImageView> swapchain_views;
    std::vector<vk::Framebuffer> swapchain_framebuffers;
    std::vector<vk::CommandBuffer> command_buffers;
    std::vector<vk::Fence> fences;
    std::vector<vk::Semaphore> image_acquired_semaphores;
    std::vector<vk::Semaphore> image_ready_semaphores;

    // renderpass used when no effect is done previously (clear the swapchain content)
    vk::RenderPass default_render_pass;
    // renderpass used after a post-processing pass clearing the swapchain, compatible with the default render pass
    vk::RenderPass post_filter_render_pass;
#ifdef __ANDROID__
    // renderpass used to (partially) prevent a driver bug using stock adreno drivers
    vk::RenderPass stock_adreno_pass;
#endif

    std::unique_ptr<ScreenFilter> filter;

    std::vector<vkutil::Image> vita_surface;
    vma::Allocation vita_surface_staging_alloc;
    vma::AllocationInfo vita_surface_staging_info;
    vk::Buffer vita_surface_staging;

    vk::CommandBuffer current_cmd_buffer;

    // these are used by the gui
    uint32_t swapchain_image_idx = ~0;
    // between 0 and swapchain_size - 1, used as the index for semaphores
    int current_frame = 0;
    // set to true after a window resize, in this case the pipeline needs to be rebuilt
    bool need_rebuild = false;

    ScreenRenderer(VKState &state);

    bool create(SDL_Window *window);
    // called after the logical device has been created
    bool setup();
    void cleanup();

    bool acquire_swapchain_image(bool start_render_pass = false);
    void render(vk::ImageView image_view, vk::ImageLayout layout, const Viewport &viewport);
    void swap_window();
    void set_filter(const std::string_view &filter);

private:
    void create_render_pass();
    void create_layout_sync();
    void create_swapchain();
    vk::Pipeline create_graphics_pipeline_impl(std::array<vk::PipelineShaderStageCreateInfo, 2> &shader_stages);
    bool create_graphics_pipelines();
    void copy_to_vao(const void *data);
    void create_surface_image();
    void destroy_swapchain();
};
} // namespace renderer::vulkan
