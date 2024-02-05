// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <util/align.h>
#include <util/log.h>

namespace vkutil {

static vma::Allocator allocator = nullptr;

void init(vma::Allocator vma_allocator) {
    allocator = vma_allocator;
}

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

Image::Image(uint32_t width, uint32_t height, vk::Format format)
    : width(width)
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

    // only create a view if one of these flags is set
    constexpr vk::ImageUsageFlags view_usages = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eStorage;
    if (!(usage & view_usages))
        return;

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

Buffer::Buffer(vk::DeviceSize size)
    : size(size) {
}

void Buffer::destroy() {
    if (!destroy_on_deletion || !allocator)
        return;

    if (buffer) {
        allocator.destroyBuffer(buffer, allocation);
        buffer = nullptr;
    }
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

RingBuffer::RingBuffer(vk::BufferUsageFlags usage, const size_t capacity)
    : usage(usage)
    , capacity(capacity) {
    uint32_t buffer_capacity = capacity;
    if (usage & vk::BufferUsageFlagBits::eStorageBuffer)
        // TODO: put max size of a gxm uniform buffer
        buffer_capacity += 500 * 1024;
    if (usage & vk::BufferUsageFlagBits::eVertexBuffer)
        // for AMD GPUs, in case the buffer ends exactly with an rgb16 component (which is read as rgba)
        buffer_capacity += 2;
    if (usage & vk::BufferUsageFlagBits::eUniformBuffer)
        // the descriptor set for the uniform buffers specify the max possible size while we only allocate
        // the actual size, this prevents validation errors
        buffer_capacity += 512;

    buffer = Buffer(buffer_capacity);
}

void RingBuffer::allocate(const uint32_t data_size) {
    if (cursor + data_size > capacity)
        cursor = 0;

    data_offset = cursor;

    cursor += data_size;

    cursor = align(cursor, alignment);
}

void HostRingBuffer::create() {
    buffer.init_buffer(usage, vma_mapped_alloc);

    vk::MemoryPropertyFlags memory_properties = allocator.getAllocationMemoryProperties(buffer.allocation);
    is_coherent = static_cast<bool>(memory_properties & vk::MemoryPropertyFlagBits::eHostCoherent);

    cursor = 0;
}

void HostRingBuffer::copy(vk::CommandBuffer cmd_buffer, const uint32_t size, const void *data, const uint32_t offset) {
    memcpy(static_cast<uint8_t *>(buffer.mapped_data) + data_offset + offset, data, size);

    if (!is_coherent)
        allocator.flushAllocation(buffer.allocation, data_offset + offset, size);
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

void DestroyQueue::init(vk::Device device) {
    this->device = device;
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
        destroy_list.push_back(std::bit_cast<uint64_t>(image.allocation));
    }
}

void DestroyQueue::add_buffer(Buffer &buffer) {
    if (buffer.buffer) {
        add(buffer.buffer);
        buffer.buffer = nullptr;
        destroy_list.push_back(std::bit_cast<uint64_t>(buffer.allocation));
    }
}

void DestroyQueue::add_cmd_buffer(vk::CommandBuffer cmd_buffer, vk::CommandPool cmd_pool) {
    add(cmd_buffer);
    destroy_list.push_back(std::bit_cast<uint64_t>(cmd_pool));
}

#define HANDLE_DESTROY(type)                          \
    case vk::ObjectType::e##type: {                   \
        auto vk_object = std::bit_cast<vk::type>(el); \
        device.destroy(vk_object);                    \
        break;                                        \
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
            auto image = std::bit_cast<vk::Image>(el);
            auto allocation = std::bit_cast<vma::Allocation>(destroy_list[idx++]);
            allocator.destroyImage(image, allocation);
            break;
        }

        case vk::ObjectType::eBuffer: {
            // special case: this is a vma allocation
            auto buffer = std::bit_cast<vk::Buffer>(el);
            auto allocation = std::bit_cast<vma::Allocation>(destroy_list[idx++]);
            allocator.destroyBuffer(buffer, allocation);
            break;
        }

        case vk::ObjectType::eCommandBuffer: {
            // special case: we must specify the command pool
            auto cmd_buffer = std::bit_cast<vk::CommandBuffer>(el);
            auto cmd_pool = std::bit_cast<vk::CommandPool>(destroy_list[idx++]);
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
