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

#include <renderer/state.h>
#include <renderer/types.h>

#include <renderer/vulkan/types.h>

typedef void *ImTextureID;

namespace renderer::vulkan {
struct VulkanState : public renderer::State {
    vk::Instance instance;
    vk::Device device;

    // Used for memory allocation and general query later.
    vk::PhysicalDevice physical_device;
    vk::PhysicalDeviceProperties physical_device_properties;
    vk::PhysicalDeviceFeatures physical_device_features;
    vk::SurfaceCapabilitiesKHR physical_device_surface_capabilities;
    std::vector<vk::SurfaceFormatKHR> physical_device_surface_formats;
    vk::PhysicalDeviceMemoryProperties physical_device_memory;
    std::vector<vk::QueueFamilyProperties> physical_device_queue_families;

    VmaAllocator allocator;

    uint32_t general_family_index = 0;
    uint32_t transfer_family_index = 0;
    uint32_t general_queue_last = 0;
    uint32_t transfer_queue_last = 0;
    std::vector<vk::Queue> general_queues;
    std::vector<vk::Queue> transfer_queues;

    // These might be merged into one queue, but for now they are different.
    vk::CommandPool general_command_pool;
    // Transfer pool has transient bit set.
    vk::CommandPool transfer_command_pool;

    vk::CommandBuffer general_command_buffer;

    vk::SurfaceKHR surface;
    vk::SwapchainKHR swapchain;

    // These would be vectors...
    uint32_t swapchain_width = 0, swapchain_height = 0;
    vk::Image swapchain_images[2];
    vk::ImageView swapchain_views[2];
};
} // namespace renderer::vulkan
