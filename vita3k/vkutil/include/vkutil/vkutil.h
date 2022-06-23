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

#define VK_NO_PROTOTYPES
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.hpp>

#include <string>

namespace vkutil {

static constexpr vk::ImageSubresourceRange color_subresource_range = {
    .aspectMask = vk::ImageAspectFlagBits::eColor,
    .baseMipLevel = 0,
    .levelCount = 1,
    .baseArrayLayer = 0,
    .layerCount = 1
};

static constexpr vk::ImageSubresourceRange ds_subresource_range = {
    .aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
    .baseMipLevel = 0,
    .levelCount = 1,
    .baseArrayLayer = 0,
    .layerCount = 1
};

static constexpr vk::ImageSubresourceLayers color_subresource_layer = {
    .aspectMask = vk::ImageAspectFlagBits::eColor,
    .mipLevel = 0,
    .baseArrayLayer = 0,
    .layerCount = 1
};

static constexpr vk::ComponentMapping default_comp_mapping = {
    vk::ComponentSwizzle::eIdentity,
    vk::ComponentSwizzle::eIdentity,
    vk::ComponentSwizzle::eIdentity,
    vk::ComponentSwizzle::eIdentity
};

static constexpr vk::ComponentMapping rgba_mapping = {
    vk::ComponentSwizzle::eR,
    vk::ComponentSwizzle::eG,
    vk::ComponentSwizzle::eB,
    vk::ComponentSwizzle::eA
};

static constexpr vk::ColorComponentFlags default_color_mask = vk::ColorComponentFlagBits::eR
    | vk::ColorComponentFlagBits::eG
    | vk::ColorComponentFlagBits::eB
    | vk::ColorComponentFlagBits::eA;

static constexpr vma::AllocationCreateInfo vma_auto_alloc = {
    .usage = vma::MemoryUsage::eAuto
};

static constexpr vma::AllocationCreateInfo vma_mapped_alloc = {
    .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite | vma::AllocationCreateFlagBits::eMapped,
    .usage = vma::MemoryUsage::eAuto,
};

static constexpr vma::AllocationCreateInfo vma_host_visible = {
    .flags = vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
    .usage = vma::MemoryUsage::eAuto,
    .requiredFlags = vk::MemoryPropertyFlagBits::eHostVisible,
};

template <typename T>
static std::enable_if_t<vk::isVulkanHandleType<T>::value, uint64_t> &to_u64(T &vk_object) {
    return reinterpret_cast<uint64_t &>(vk_object);
}

static uint64_t &to_u64(vma::Allocation &vk_object) {
    return reinterpret_cast<uint64_t &>(vk_object);
}

template <typename T>
static std::enable_if_t<vk::isVulkanHandleType<T>::value, T> &from_u64(uint64_t &vk_object) {
    return reinterpret_cast<T &>(vk_object);
}

static vma::Allocation &from_u64(uint64_t &vk_object) {
    return reinterpret_cast<vma::Allocation &>(vk_object);
}

vk::CommandBuffer create_single_time_command(vk::Device &device, vk::CommandPool &cmd_pool);
void end_single_time_command(vk::Device &device, vk::Queue &queue, vk::CommandPool &cmd_pool, vk::CommandBuffer cmd_buffer);

vk::ShaderModule load_shader(vk::Device &device, const std::string &path);
vk::ShaderModule load_shader(vk::Device &device, const void *data, const uint32_t size);

void copy_buffer(vk::Device &device, vk::CommandPool &cmd_pool, vk::Queue &queue, vk::Buffer &src, vk::Buffer &dst, vk::DeviceSize size);

enum struct ImageLayout {
    Undefined,
    TransferSrc,
    TransferDst,
    ColorAttachment,
    DepthStencilAttachment,
    ColorAttachmentReadWrite,
    SampledImage,
    StorageImage,
    DepthReadOnly
};
void transition_image_layout(vk::CommandBuffer &cmd_buffer, vk::Image image, ImageLayout src_layout, ImageLayout dst_layout, const vk::ImageSubresourceRange &range = color_subresource_range);
// transition image layout assuming you don't care about the former image content
void transition_image_layout_discard(vk::CommandBuffer &cmd_buffer, vk::Image image, ImageLayout src_layout, ImageLayout dst_layout, const vk::ImageSubresourceRange &range = color_subresource_range);

// given the swizzle of the color surface (which was written as rgba because vulkan doesn't support swizzle on framebuffers)
// and the swizzle of the texture that's reading from the color surface
// return the swizzle the texture should to make it like the color actually had a swizzle applied to it
vk::ComponentMapping color_to_texture_swizzle(const vk::ComponentMapping &swizzle_color, const vk::ComponentMapping &swizzle_texture);
} // namespace vkutil
