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

#include "vkutil/vkutil.h"

#include "util/fs.h"

namespace vkutil {

vk::CommandBuffer create_single_time_command(vk::Device device, vk::CommandPool cmd_pool) {
    vk::CommandBufferAllocateInfo command_buffer_info{
        .commandPool = cmd_pool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1
    };

    vk::CommandBuffer buffer = device.allocateCommandBuffers(command_buffer_info)[0];

    vk::CommandBufferBeginInfo begin_info{
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    };
    buffer.begin(begin_info);

    return buffer;
}

void end_single_time_command(vk::Device device, vk::Queue queue, vk::CommandPool cmd_pool, vk::CommandBuffer cmd_buffer) {
    cmd_buffer.end();

    vk::SubmitInfo submit_info{};
    submit_info.setCommandBuffers(cmd_buffer);
    queue.submit(submit_info);
    queue.waitIdle();

    device.freeCommandBuffers(cmd_pool, cmd_buffer);
}

vk::ShaderModule load_shader(vk::Device device, const fs::path &shader_path) {
    std::vector<uint8_t> shader_code(0);
    auto res = fs_utils::read_data(shader_path, shader_code);
    if (!res)
        return {};
    return load_shader(device, shader_code.data(), shader_code.size());
}

vk::ShaderModule load_shader(vk::Device device, const void *data, const uint32_t size) {
    vk::ShaderModuleCreateInfo shader_info{
        .codeSize = size,
        .pCode = static_cast<const uint32_t *>(data)
    };

    return device.createShaderModule(shader_info);
}

void copy_buffer(vk::Device device, vk::CommandPool cmd_pool, vk::Queue queue, vk::Buffer src, vk::Buffer dst, vk::DeviceSize size) {
    vk::CommandBuffer cmd_buffer = create_single_time_command(device, cmd_pool);

    vk::BufferCopy copy_region{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    cmd_buffer.copyBuffer(src, dst, copy_region);

    end_single_time_command(device, queue, cmd_pool, cmd_buffer);
}

struct ImageLayoutTransition {
    vk::ImageLayout layout;
    vk::PipelineStageFlags stages;
    vk::AccessFlags access;
};

static constexpr ImageLayoutTransition layout_transitions[] = {
    // Undefined
    {
        vk::ImageLayout::eUndefined,
        vk::PipelineStageFlagBits::eTopOfPipe,
        vk::AccessFlags() },
    // TransferSrc
    {
        vk::ImageLayout::eTransferSrcOptimal,
        vk::PipelineStageFlagBits::eTransfer,
        vk::AccessFlagBits::eTransferRead },
    // TransferDst
    {
        vk::ImageLayout::eTransferDstOptimal,
        vk::PipelineStageFlagBits::eTransfer,
        vk::AccessFlagBits::eTransferWrite },
    // ColorAttachment
    {
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits::eColorAttachmentWrite },
    // DepthStencilAttachment
    {
        vk::ImageLayout::eDepthStencilAttachmentOptimal,
        vk::PipelineStageFlagBits::eEarlyFragmentTests,
        vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite },
    // ColorAttachmentReadWrite
    {
        vk::ImageLayout::eGeneral,
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead },
    // SampledImage
    {
        vk::ImageLayout::eShaderReadOnlyOptimal,
        vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader,
        vk::AccessFlagBits::eShaderRead },
    // StorageImage
    {
        vk::ImageLayout::eGeneral,
        vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eComputeShader,
        vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite },
    // DepthStencilReadOnly
    {
        vk::ImageLayout::eDepthStencilReadOnlyOptimal,
        vk::PipelineStageFlagBits::eFragmentShader,
        vk::AccessFlagBits::eShaderRead },
};

static void transition_image_layout_impl(vk::CommandBuffer cmd_buffer, vk::Image image, ImageLayout src_layout, ImageLayout dst_layout, const vk::ImageSubresourceRange &range, bool initial_undefined) {
    const ImageLayoutTransition &src_transition = layout_transitions[static_cast<int>(src_layout)];
    const ImageLayoutTransition &dst_transition = layout_transitions[static_cast<int>(dst_layout)];

    vk::ImageMemoryBarrier barrier{
        .srcAccessMask = src_transition.access,
        .dstAccessMask = dst_transition.access,
        .oldLayout = initial_undefined ? vk::ImageLayout::eUndefined : src_transition.layout,
        .newLayout = dst_transition.layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = range
    };

    cmd_buffer.pipelineBarrier(src_transition.stages, dst_transition.stages, vk::DependencyFlags(), {}, {}, barrier);
}

void transition_image_layout(vk::CommandBuffer cmd_buffer, vk::Image image, ImageLayout src_layout, ImageLayout dst_layout, const vk::ImageSubresourceRange &range) {
    transition_image_layout_impl(cmd_buffer, image, src_layout, dst_layout, range, false);
}

void transition_image_layout_discard(vk::CommandBuffer cmd_buffer, vk::Image image, ImageLayout src_layout, ImageLayout dst_layout, const vk::ImageSubresourceRange &range) {
    transition_image_layout_impl(cmd_buffer, image, src_layout, dst_layout, range, true);
}

vk::ImageLayout get_underlying_layout(ImageLayout layout) {
    return layout_transitions[static_cast<int>(layout)].layout;
}

vk::ComponentMapping color_to_texture_swizzle(const vk::ComponentMapping &swizzle_color, const vk::ComponentMapping &swizzle_texture) {
    vk::ComponentMapping result_comp;
    vk::ComponentSwizzle *result = reinterpret_cast<vk::ComponentSwizzle *>(&result_comp);
    const vk::ComponentSwizzle *color = reinterpret_cast<const vk::ComponentSwizzle *>(&swizzle_color);
    const vk::ComponentSwizzle *texture = reinterpret_cast<const vk::ComponentSwizzle *>(&swizzle_texture);

    for (int i = 0; i < 4; i++) {
        vk::ComponentSwizzle component = texture[i];
        int component_val = static_cast<int>(component);
        if (component_val < VK_COMPONENT_SWIZZLE_R) {
            result[i] = component;
        } else {
            result[i] = vk::ComponentSwizzle::eZero;
            // look in the color swizzle if there is a component writing to the same channel
            for (int j = 0; j < 4; j++) {
                if (component == color[j])
                    result[i] = static_cast<vk::ComponentSwizzle>(VK_COMPONENT_SWIZZLE_R + j);
            }
        }
    }
    return result_comp;
}

} // namespace vkutil
