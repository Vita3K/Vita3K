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

#pragma once

#include <vkutil/objects.h>

struct SDL_Window;
struct SceFVector2;

namespace renderer::vulkan {

struct VKState;

class ScreenRenderer {
public:
    VKState &state;
    SDL_Window *window;

    vk::SurfaceKHR surface;
    vk::SwapchainKHR swapchain;

    vk::SurfaceCapabilitiesKHR surface_capabilities;
    vk::SurfaceFormatKHR surface_format;
    vk::PresentModeKHR present_mode;

    vk::Extent2D extent;
    uint32_t swapchain_size;
    std::vector<vk::Image> swapchain_images;
    std::vector<vk::ImageView> swapchain_views;
    std::vector<vk::Framebuffer> swapchain_framebuffers;
    std::vector<vk::CommandBuffer> command_buffers;
    std::vector<vk::Fence> fences;

    vk::ShaderModule shader_vertex;
    vk::ShaderModule shader_fragment;
    vk::ShaderModule shader_fragment_fxaa;
    vk::RenderPass render_pass;
    vk::DescriptorSetLayout descriptor_set_layout;
    std::vector<vk::DescriptorSet> descriptor_sets;
    vk::DescriptorPool descriptor_pool;
    vk::PipelineLayout pipeline_layout;

    vk::Pipeline pipeline;
    vk::Pipeline pipeline_fxaa;

    vk::Semaphore image_acquired_semaphore;
    vk::Semaphore image_ready_semaphore;

    vma::Allocation vao_allocation;
    vk::Buffer vao;
    std::vector<std::array<float, 4>> last_uvs;

    std::vector<vkutil::Image> vita_surface;
    vk::Sampler vita_surface_sampler;
    vma::Allocation vita_surface_staging_alloc;
    vma::AllocationInfo vita_surface_staging_info;
    vk::Buffer vita_surface_staging;

    vk::CommandBuffer current_cmd_buffer;

    // these are used by the gui
    uint32_t swapchain_image_idx = ~0;
    // set to true after a window resize, in this case the pipeline needs to be rebuilt
    bool need_rebuild = false;
    // is fxaa used
    bool enable_fxaa = false;

    ScreenRenderer(VKState &state);

    bool create(SDL_Window *window);
    // called after the logical device has been created
    bool setup(const char *base_path);
    void cleanup();

    bool acquire_swapchain_image(bool start_render_pass = false);
    void render(vk::ImageView image_view, vk::ImageLayout layout, std::array<float, 4> &uvs, SceFVector2 &texture_size);
    void swap_window();

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
