// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <cstring>
#include <limits>
#include <util/log.h>
#include <vkutil/readback.h>

namespace vkutil {
static uint32_t storage_bpp(vk::Format format) {
    switch (format) {
    case vk::Format::eR5G6B5UnormPack16: return 2;
    case vk::Format::eR8G8B8A8Unorm:
    case vk::Format::eR8G8B8A8Srgb: return 4;
    default: return 0;
    }
}

bool read_image_to_host(vk::PhysicalDevice physical, vk::Device device, vk::Queue queue, std::mutex &queue_mutex,
    vk::CommandPool pool, vma::Allocator allocator, const Image &source,
    uint32_t width, uint32_t height, std::vector<uint8_t> &pixels) {
    const uint32_t bpp = storage_bpp(source.format);
    const uint64_t size = uint64_t(width) * height * bpp;
    if (!source.image || !bpp || !width || !height || !source.width || !source.height
        || source.layout == ImageLayout::Undefined || size > 64 * 1024 * 1024)
        return false;
    const bool scaled = width != source.width || height != source.height;
    if (scaled) {
        const auto supported = physical.getFormatProperties(source.format).optimalTilingFeatures;
        if (!(supported & vk::FormatFeatureFlagBits::eBlitSrc)
            || !(supported & vk::FormatFeatureFlagBits::eBlitDst))
            return false;
    }

    Buffer staging(size);
    const vma::AllocationCreateInfo alloc{
        .flags = vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessRandom,
        .usage = vma::MemoryUsage::eAuto,
        .requiredFlags = vk::MemoryPropertyFlagBits::eHostVisible,
        .preferredFlags = vk::MemoryPropertyFlagBits::eHostCached
    };
    staging.init_buffer(vk::BufferUsageFlagBits::eTransferDst, alloc);
    if (!staging.mapped_data)
        return false;
    auto commands = device.allocateCommandBuffersUnique({ .commandPool = pool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1 });
    const vk::CommandBuffer cmd = *commands[0];
    auto fence = device.createFenceUnique({});
    Image reduced(width, height, source.format);
    if (scaled)
        reduced.init_image(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc);
    cmd.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });
    const auto original_layout = get_underlying_layout(source.layout);
    vk::ImageMemoryBarrier barrier{
        .srcAccessMask = vk::AccessFlagBits::eMemoryWrite,
        .dstAccessMask = vk::AccessFlagBits::eTransferRead,
        .oldLayout = original_layout,
        .newLayout = vk::ImageLayout::eTransferSrcOptimal,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = source.image,
        .subresourceRange = color_subresource_range
    };
    // Include storage/interlock writes, not only color-attachment writes.
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands,
        vk::PipelineStageFlagBits::eTransfer, {}, {}, {}, barrier);
    vk::Image copied = source.image;
    if (scaled) {
        reduced.transition_to(cmd, ImageLayout::TransferDst);
        const vk::ImageBlit blit{
            .srcSubresource = color_subresource_layer,
            .srcOffsets = std::array<vk::Offset3D, 2>{ vk::Offset3D{ 0, 0, 0 }, vk::Offset3D{ int32_t(source.width), int32_t(source.height), 1 } },
            .dstSubresource = color_subresource_layer,
            .dstOffsets = std::array<vk::Offset3D, 2>{ vk::Offset3D{ 0, 0, 0 }, vk::Offset3D{ int32_t(width), int32_t(height), 1 } }
        };
        cmd.blitImage(source.image, vk::ImageLayout::eTransferSrcOptimal,
            reduced.image, vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eNearest);
        reduced.transition_to(cmd, ImageLayout::TransferSrc);
        copied = reduced.image;
    }
    const vk::BufferImageCopy copy{
        .bufferOffset = 0, .bufferRowLength = 0, .bufferImageHeight = 0, .imageSubresource = color_subresource_layer, .imageOffset = { 0, 0, 0 }, .imageExtent = { width, height, 1 }
    };
    cmd.copyImageToBuffer(copied, vk::ImageLayout::eTransferSrcOptimal, staging.buffer, copy);
    const vk::BufferMemoryBarrier host{
        .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
        .dstAccessMask = vk::AccessFlagBits::eHostRead,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = staging.buffer,
        .offset = 0,
        .size = size
    };
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eHost,
        {}, {}, host, {});
    barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    barrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite;
    barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    barrier.newLayout = original_layout;
    cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
        vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, barrier);
    cmd.end();
    const vk::SubmitInfo submit{ .commandBufferCount = 1, .pCommandBuffers = &cmd };
    try {
        {
            const std::lock_guard<std::mutex> queue_lock(queue_mutex);
            queue.submit(submit, *fence);
        }
        // Follow the renderer's existing fence-wait contract. Do not release
        // staging or command resources on a failed/throwing wait before a drain.
        if (device.waitForFences(*fence, true, std::numeric_limits<uint64_t>::max()) != vk::Result::eSuccess) {
            const std::lock_guard<std::mutex> queue_lock(queue_mutex);
            device.waitIdle();
            LOG_ERROR("Unmapped surface readback fence failed after device drain");
            return false;
        }
    } catch (...) {
        try {
            const std::lock_guard<std::mutex> queue_lock(queue_mutex);
            device.waitIdle();
        } catch (...) {
            // Completion is unknowable: terminating preserves pending-resource
            // ownership until process/device teardown instead of destroying it.
            LOG_CRITICAL("GPU readback could not drain failed device");
            std::terminate();
        }
        throw;
    }
    allocator.invalidateAllocation(staging.allocation, 0, size);
    std::vector<uint8_t> result(size);
    std::memcpy(result.data(), staging.mapped_data, size);
    pixels.swap(result);
    return true;
}

bool convert_readback_pixels(vk::Format format, const vk::ComponentMapping &swizzle,
    uint32_t guest_bpp, const std::vector<uint8_t> &input, std::vector<uint8_t> &output) {
    const auto r = swizzle.r;
    const auto is_mapping = [&](vk::ComponentSwizzle a, vk::ComponentSwizzle b,
                                vk::ComponentSwizzle c, vk::ComponentSwizzle d) {
        return swizzle.r == a && swizzle.g == b && swizzle.b == c && swizzle.a == d;
    };
    using S = vk::ComponentSwizzle;
    if (format == vk::Format::eR5G6B5UnormPack16) {
        if (guest_bpp != 2 || input.size() % 2 || (r != vk::ComponentSwizzle::eR && r != vk::ComponentSwizzle::eB))
            return false;
        if (!is_mapping(S::eR, S::eG, S::eB, S::eOne) && !is_mapping(S::eB, S::eG, S::eR, S::eOne))
            return false;
        auto result = input;
        if (r == vk::ComponentSwizzle::eB) {
            for (size_t i = 0; i < result.size(); i += 2) {
                uint16_t p;
                std::memcpy(&p, result.data() + i, 2);
                p = uint16_t(((p & 31) << 11) | (p & 0x7e0) | ((p >> 11) & 31));
                std::memcpy(result.data() + i, &p, 2);
            }
        }
        output.swap(result);
        return true;
    }
    if ((format != vk::Format::eR8G8B8A8Unorm && format != vk::Format::eR8G8B8A8Srgb)
        || (guest_bpp != 3 && guest_bpp != 4) || input.size() % 4)
        return false;
    std::array<uint8_t, 4> order{};
    switch (r) {
    case vk::ComponentSwizzle::eR: order = { 0, 1, 2, 3 }; break;
    case vk::ComponentSwizzle::eB: order = { 2, 1, 0, 3 }; break;
    case vk::ComponentSwizzle::eA:
        if (guest_bpp == 3)
            return false;
        order = { 3, 2, 1, 0 };
        break;
    case vk::ComponentSwizzle::eG:
        if (guest_bpp == 3)
            return false;
        order = { 3, 0, 1, 2 };
        break;
    default: return false;
    }
    if (guest_bpp == 3) {
        if (!is_mapping(S::eR, S::eG, S::eB, S::eOne) && !is_mapping(S::eB, S::eG, S::eR, S::eOne))
            return false;
    } else if (!is_mapping(S::eR, S::eG, S::eB, S::eA) && !is_mapping(S::eB, S::eG, S::eR, S::eA)
        && !is_mapping(S::eA, S::eB, S::eG, S::eR) && !is_mapping(S::eG, S::eB, S::eA, S::eR))
        return false;
    std::vector<uint8_t> result((input.size() / 4) * guest_bpp);
    for (size_t i = 0, j = 0; i < input.size(); i += 4, j += guest_bpp)
        for (uint32_t c = 0; c < guest_bpp; ++c)
            result[j + c] = input[i + order[c]];
    output.swap(result);
    return true;
}
} // namespace vkutil
