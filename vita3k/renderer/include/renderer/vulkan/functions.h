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

#include "state.h"

namespace renderer::vulkan {
#define VULKAN_CHECK(a) assert(a == vk::Result::eSuccess)

bool create(SDL_Window *window, std::unique_ptr<renderer::State> &state);
void close(std::unique_ptr<renderer::State> &state);

// I think I will drop this approach but this is fine for now.
enum class CommandType {
    General,
    Transfer,
};

enum class MemoryType {
    Mappable,
    Device,
};

vk::Queue select_queue(VulkanState &state, CommandType type);

bool resize_swapchain(VulkanState &state, vk::Extent2D size);

vk::CommandBuffer create_command_buffer(VulkanState &state, CommandType type);
void free_command_buffer(VulkanState &state, CommandType type, vk::CommandBuffer buffer);

vk::Buffer create_buffer(VulkanState &state, const vk::BufferCreateInfo &buffer_info, MemoryType type, VmaAllocation &allocation);
void destroy_buffer(VulkanState &state, vk::Buffer buffer, VmaAllocation allocation);
vk::Image create_image(VulkanState &state, const vk::ImageCreateInfo &image_info, MemoryType type, VmaAllocation &allocation);
void destroy_image(VulkanState &state, vk::Image image, VmaAllocation allocation);
} // namespace renderer::vulkan
