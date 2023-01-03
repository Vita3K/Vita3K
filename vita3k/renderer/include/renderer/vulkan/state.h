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

#pragma once

#include <renderer/state.h>
#include <renderer/types.h>

#include <renderer/vulkan/pipeline_cache.h>
#include <renderer/vulkan/screen_renderer.h>
#include <renderer/vulkan/surface_cache.h>
#include <renderer/vulkan/types.h>

typedef void *ImTextureID;

namespace renderer::vulkan {
struct VKState : public renderer::State {
    // 0 = automatic, > 0 = order in instance.enumeratePhysicalDevices
    int gpu_idx;

    VKSurfaceCache surface_cache;
    PipelineCache pipeline_cache;
    VKTextureCacheState texture_cache;

    vk::Instance instance;
    vk::Device device;

    ScreenRenderer screen_renderer;

    // Used for memory allocation and general query later.
    vk::PhysicalDevice physical_device;
    vk::PhysicalDeviceProperties physical_device_properties;
    vk::PhysicalDeviceFeatures physical_device_features;
    vk::PhysicalDeviceMemoryProperties physical_device_memory;
    std::vector<vk::QueueFamilyProperties> physical_device_queue_families;

    vma::Allocator allocator;

    uint32_t general_family_index = 0;
    uint32_t transfer_family_index = 0;
    uint32_t general_queue_last = 0;
    uint32_t transfer_queue_last = 0;
    vk::Queue general_queue;
    vk::Queue transfer_queue;

    // These might be merged into one queue, but for now they are different.
    vk::CommandPool general_command_pool;
    // Transfer pool has transient bit set.
    vk::CommandPool transfer_command_pool;

    // only used when memory mapping is enabled
    std::map<uint64_t, MappedMemory, std::greater<uint64_t>> mapped_memories;

    vkutil::Image default_image;
    vkutil::Buffer default_buffer;

    VKState(int gpu_idx);

    bool init(const char *base_path, const bool hashless_texture_cache) override;
    bool create(SDL_Window *window, std::unique_ptr<renderer::State> &state, const char *base_path);
    void cleanup();
    void render_frame(const SceFVector2 &viewport_pos, const SceFVector2 &viewport_size, const DisplayState &display,
        const GxmState &gxm, MemState &mem) override;
    void swap_window(SDL_Window *window) override;
    void set_fxaa(bool enable_fxaa) override;
    int get_max_anisotropic_filtering() override;
    void set_anisotropic_filtering(int anisotropic_filtering) override;
    bool map_memory(void *address, uint32_t size) override;
    void unmap_memory(void *address) override;
    // return the matching buffer and offset for the memory location
    std::tuple<vk::Buffer, uint32_t> get_matching_mapping(const void *address);
    // return the GPU buffer device address matching this one
    uint64_t get_matching_device_address(const void *address);
    std::vector<std::string> get_gpu_list() override;

    void precompile_shader(const ShadersHash &hash) override;
    void preclose_action() override;
};
} // namespace renderer::vulkan
