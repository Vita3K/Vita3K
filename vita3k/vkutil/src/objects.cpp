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

#include "vkutil/objects.h"

#include "vkutil/vkutil.h"

#include <util/log.h>

namespace vkutil {
Image::Image() = default;

Image::Image(Image &&other) noexcept {
    memcpy(this, &other, sizeof(Image));
    other.sampler = nullptr;
    other.view = nullptr;
    other.image = nullptr;
    other.layout = ImageLayout::Undefined;
}
Image &Image::operator=(Image &&other) noexcept {
    memcpy(this, &other, sizeof(Image));
    other.sampler = nullptr;
    other.view = nullptr;
    other.image = nullptr;
    other.layout = ImageLayout::Undefined;
    return *this;
}

Image::Image(vma::Allocator allocator, uint32_t width, uint32_t height, vk::Format format)
    : allocator(allocator)
    , width(width)
    , height(height)
    , format(format) {
}

void Image::destroy() {
    if (!destroy_on_deletion || !allocator)
        return;

    vk::Device device = allocator.getAllocatorInfo().device;
    if (sampler) {
        device.destroySampler(sampler);
        sampler = nullptr;
    }
    if (view) {
        device.destroyImageView(view);
        view = nullptr;
    }
    if (image) {
        allocator.destroyImage(image, allocation);
        image = nullptr;
    }
}

Image::~Image() {
    destroy();
}

void Image::init_image(vk::ImageUsageFlags usage, vk::ComponentMapping mapping, const vk::ImageCreateFlags image_create_flags, const void *pNext) {
    vk::ImageCreateInfo image_info{
        .pNext = pNext,
        .flags = image_create_flags,
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = vk::Extent3D{
            .width = width,
            .height = height,
            .depth = 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = vk::ImageTiling::eOptimal,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive,
        .initialLayout = vk::ImageLayout::eUndefined,
    };

    std::tie(image, allocation) = allocator.createImage(image_info, vma_auto_alloc);

    vk::ImageSubresourceRange range = (format == vk::Format::eD32SfloatS8Uint) ? vkutil::ds_subresource_range : vkutil::color_subresource_range;
    vk::ImageViewCreateInfo view_info{
        .image = image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .components = mapping,
        .subresourceRange = range
    };
    view = allocator.getAllocatorInfo().device.createImageView(view_info);
}

void Image::transition_to(vk::CommandBuffer buffer, ImageLayout new_layout, const vk::ImageSubresourceRange &range) {
    transition_image_layout(buffer, image, layout, new_layout, range);
    layout = new_layout;
}

void Image::transition_to_discard(vk::CommandBuffer buffer, ImageLayout new_layout, const vk::ImageSubresourceRange &range) {
    transition_image_layout_discard(buffer, image, layout, new_layout, range);
    layout = new_layout;
}

Buffer::Buffer() = default;

Buffer::Buffer(Buffer &&other) noexcept {
    memcpy(this, &other, sizeof(Buffer));
    other.allocation = nullptr;
    other.buffer = nullptr;
    other.size = 0;
    other.mapped_data = nullptr;
}
Buffer &Buffer::operator=(Buffer &&other) noexcept {
    memcpy(this, &other, sizeof(Buffer));
    other.allocation = nullptr;
    other.buffer = nullptr;
    other.size = 0;
    other.mapped_data = nullptr;
    return *this;
}

Buffer::Buffer(vma::Allocator allocator, vk::DeviceSize size)
    : allocator(allocator)
    , size(size) {
}

void Buffer::destroy() {
    if (!destroy_on_deletion || !allocator)
        return;

    if (buffer)
        allocator.destroyBuffer(buffer, allocation);
}

Buffer::~Buffer() {
    destroy();
}

void Buffer::init_buffer(vk::BufferUsageFlags usage_flags, const vma::AllocationCreateInfo &alloc_create_info) {
    vk::BufferCreateInfo buffer_info{
        .size = size,
        .usage = usage_flags,
        .sharingMode = vk::SharingMode::eExclusive
    };
    vma::AllocationInfo alloc_info;
    std::tie(buffer, allocation) = allocator.createBuffer(buffer_info, alloc_create_info, alloc_info);
    mapped_data = alloc_info.pMappedData;
}

RingBuffer::RingBuffer(vma::Allocator allocator, vk::BufferUsageFlags usage, const size_t capacity)
    : usage(usage)
    , capacity(capacity) {
    uint32_t buffer_capacity = capacity;
    if (usage & vk::BufferUsageFlagBits::eStorageBuffer)
        // TODO: put max size of a gxm uniform buffer
        buffer_capacity += 500 * 1024;
    if (usage & vk::BufferUsageFlagBits::eVertexBuffer)
        // for AMD GPUs, in case the buffer ends exactly with an rgb16 component (which is read as rgba)
        buffer_capacity += 2;

    buffer = Buffer(allocator, buffer_capacity);
}

void RingBuffer::allocate(const uint32_t data_size) {
    if (cursor + data_size > capacity) {
        // LOG_WARNING("End of buffer reached");
        cursor = 0;
    }

    data_offset = cursor;

    cursor += data_size;
    // align to 256 bytes, the granularity is at most this value in any gpu
    cursor = (cursor + 255) & ~255;
}

void HostRingBuffer::create() {
    buffer.init_buffer(usage, vma_mapped_alloc);

    vk::MemoryPropertyFlags memory_properties = buffer.allocator.getAllocationMemoryProperties(buffer.allocation);
    is_coherent = static_cast<bool>(memory_properties & vk::MemoryPropertyFlagBits::eHostCoherent);

    cursor = 0;
}

void HostRingBuffer::copy(vk::CommandBuffer cmd_buffer, const uint32_t size, const void *data, const uint32_t offset) {
    memcpy(reinterpret_cast<uint8_t *>(buffer.mapped_data) + data_offset + offset, data, size);

    if (!is_coherent)
        buffer.allocator.flushAllocation(buffer.allocation, data_offset + offset, size);
}

void LocalRingBuffer::create() {
    // the auto_alloc default behavior should give us memory on the gpu
    // UpdateBuffer needs the buffer to have TransferDst specified
    buffer.init_buffer(usage | vk::BufferUsageFlagBits::eTransferDst);
    cursor = 0;
}

void LocalRingBuffer::copy(vk::CommandBuffer cmd_buffer, const uint32_t size, const void *data, const uint32_t offset) {
    cmd_buffer.updateBuffer(buffer.buffer, data_offset + offset, size, data);
}

void DestroyQueue::init(vk::Device device, vma::Allocator allocator) {
    this->device = device;
    this->allocator = allocator;
}

void DestroyQueue::add_image(Image &image) {
    if (image.sampler) {
        add(image.sampler);
        image.sampler = nullptr;
    }
    if (image.view) {
        add(image.view);
        image.view = nullptr;
    }

    if (image.image) {
        add(image.image);
        image.image = nullptr;
        destroy_list.push_back(to_u64(image.allocation));
    }
}

void DestroyQueue::add_buffer(Buffer &buffer) {
    if (buffer.buffer) {
        add(buffer.buffer);
        buffer.buffer = nullptr;
        destroy_list.push_back(to_u64(buffer.allocation));
    }
}

void DestroyQueue::add_cmd_buffer(vk::CommandBuffer cmd_buffer, vk::CommandPool cmd_pool) {
    add(cmd_buffer);
    destroy_list.push_back(to_u64(cmd_pool));
}

#define HANDLE_DESTROY(type)                     \
    case vk::ObjectType::e##type: {              \
        auto vk_object = from_u64<vk::type>(el); \
        device.destroy(vk_object);               \
        break;                                   \
    }

void DestroyQueue::destroy_objects() {
    if (destroy_list.empty())
        return;

    int idx = 0;
    while (idx < destroy_list.size()) {
        const vk ::ObjectType type = static_cast<vk::ObjectType>(destroy_list[idx++]);
        uint64_t el = destroy_list[idx++];
        switch (type) {
            // handle special cases apart

        case vk::ObjectType::eImage: {
            // special case: this is a vma allocation
            auto image = from_u64<vk::Image>(el);
            vma::Allocation allocation = from_u64(destroy_list[idx++]);
            allocator.destroyImage(image, allocation);
            break;
        }

        case vk::ObjectType::eBuffer: {
            // special case: this is a vma allocation
            auto buffer = from_u64<vk::Buffer>(el);
            vma::Allocation allocation = from_u64(destroy_list[idx++]);
            allocator.destroyBuffer(buffer, allocation);
            break;
        }

        case vk::ObjectType::eCommandBuffer: {
            // special case: we must specify the command pool
            auto cmd_buffer = from_u64<vk::CommandBuffer>(el);
            auto cmd_pool = from_u64<vk::CommandPool>(destroy_list[idx++]);
            device.freeCommandBuffers(cmd_pool, cmd_buffer);
            break;
        }

            HANDLE_DESTROY(ImageView)
            HANDLE_DESTROY(Sampler)
            HANDLE_DESTROY(Fence)
            HANDLE_DESTROY(Semaphore)
            HANDLE_DESTROY(Framebuffer)

        default:
            LOG_ERROR("Unknown object type {}", vk::to_string(type));
        }
    }

    destroy_list.clear();
}
} // namespace vkutil
