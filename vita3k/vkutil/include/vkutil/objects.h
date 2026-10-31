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

#pragma once

#include <functional>

#include <vkutil/vkutil.h>

#include <bit>

namespace vkutil {

void init(vma::Allocator vma_allocator);
void deinit();

struct Image {
    vma::Allocation allocation;
    vk::Image image{};
    vk::ImageView view{};
    vk::Sampler sampler{};

    uint32_t width;
    uint32_t height;
    vk::Format format;
    ImageLayout layout = ImageLayout::Undefined;

    // should the existing image, view, sampler be destroyed when this image is destroyed?
    bool destroy_on_deletion = true;

    Image();
    Image(uint32_t width, uint32_t height, vk::Format format);
    ~Image();

    // allow the object to be moved
    Image(Image &&other) noexcept;
    Image &operator=(Image &&other) noexcept;

    // make sure an image is never copied
    Image(const Image &) = delete;
    Image &operator=(Image const &) = delete;

    void init_image(vk::ImageUsageFlags usage, vk::ComponentMapping mapping = default_comp_mapping, const vk::ImageCreateFlags image_create_flags = vk::ImageCreateFlags(), const void *pNext = nullptr);
    // called by ~Image
    void destroy();

    void transition_to(vk::CommandBuffer buffer, ImageLayout new_layout, const vk::ImageSubresourceRange &range = color_subresource_range);
    // use this when you don't care about the former content of the image
    void transition_to_discard(vk::CommandBuffer buffer, ImageLayout new_layout, const vk::ImageSubresourceRange &range = color_subresource_range);
};

struct Buffer {
    vma::Allocation allocation;
    vk::Buffer buffer;

    vk::DeviceSize size = 0;
    // only useful is buffer is host visible
    void *mapped_data = nullptr;

    bool destroy_on_deletion = true;

    Buffer();
    Buffer(vk::DeviceSize size);
    ~Buffer();

    // allow the object to be moved
    Buffer(Buffer &&other) noexcept;
    Buffer &operator=(Buffer &&other) noexcept;

    // make sure an image is never copied
    Buffer(const Buffer &) = delete;
    Buffer &operator=(Buffer const &) = delete;

    void init_buffer(vk::BufferUsageFlags usage_flags, const vma::AllocationCreateInfo &alloc_create_info = vma_auto_alloc);
    // called by ~Image
    void destroy();
};

// structure that holds contiguous data
// if the end is reached, it starts back at the beginning
// Ring heap in the style of RPCS3's data_heap: [tail, head) (monotonic byte counters) may still be read
// by submitted or not yet submitted GPU work. The owner records head with every submission and moves
// tail forward once that submission's fence has signaled; when an allocation does not fit, allocate()
// calls make_room() (wait for the oldest pending submission) until it does.
class RingBuffer {
protected:
    vkutil::Buffer buffer;
    vk::BufferUsageFlags usage;

    uint64_t head = 0;
    uint64_t tail = 0;
    // head when the current buffer was created (offsets are relative to it, see grow)
    uint64_t base = 0;
    uint32_t capacity;
    // buffers replaced by grow(), kept until the work recorded before the replacement has completed
    std::vector<std::pair<vkutil::Buffer, uint64_t>> retired;

    // Replace the buffer with a larger empty one (data in flight stays in the retired buffer).
    virtual bool grow(const uint32_t needed) {
        return false;
    }

public:
    // any buffer alignment on vulkan is at most 256 on 99% of instances
    uint32_t alignment = 256;
    uint32_t data_offset = 0;
    const char *name = "ring";
    // Waits for the oldest pending submission and releases its data; false when nothing is pending.
    std::function<bool()> make_room;
    // When the work not yet submitted alone fills the ring, grow up to this size instead of overwriting
    // (only for rings bound per draw: descriptor sets keep referencing the buffer of the others).
    uint32_t max_capacity = 0;

    uint64_t get_head() const {
        return head;
    }
    // everything allocated before position has been consumed by the GPU
    void release_until(const uint64_t position) {
        tail = std::max(tail, position);
        std::erase_if(retired, [position](const auto &entry) { return position >= entry.second; });
    }

    explicit RingBuffer(vk::BufferUsageFlags usage, const size_t capacity);
    virtual ~RingBuffer() = default;
    virtual void create() = 0;

    // Allocate new data from ring buffer
    void allocate(const uint32_t data_size);
    // copy the content to the framebuffer
    // cmd_buffer may not be used
    virtual void copy(vk::CommandBuffer cmd_buffer, const uint32_t size, const void *data, const uint32_t offset = 0) = 0;

    // allocate then copy
    void allocate(vk::CommandBuffer cmd_buffer, const uint32_t data_size, const void *data) {
        allocate(data_size);
        copy(cmd_buffer, data_size, data);
    }

    vk::Buffer handle() const {
        return buffer.buffer;
    }
};

// RingBuffer allocated in the GPU memory, may not be accessible from the host
// updates are done with updateBuffer, so each data chunk should be small
// this is used for our uniform buffers
class LocalRingBuffer : public RingBuffer {
public:
    explicit LocalRingBuffer(vk::BufferUsageFlags usage, const size_t capacity)
        : RingBuffer(usage, capacity) {
    }
    void create() override;

    void copy(vk::CommandBuffer cmd_buffer, const uint32_t size, const void *data, const uint32_t offset = 0) override;
};

// RingBuffer using host visible memory
// Should be used for content that is only read a few times and not worth
// transferring to the GPU memory
class HostRingBuffer : public RingBuffer {
protected:
    bool is_coherent;

public:
    explicit HostRingBuffer(vk::BufferUsageFlags usage, const size_t capacity)
        : RingBuffer(usage, capacity) {
    }
    void create() override;
    bool grow(const uint32_t needed) override;

    void copy(vk::CommandBuffer cmd_buffer, const uint32_t size, const void *data, const uint32_t offset = 0) override;
};

// Queue that contains GPU objects that are planned to be destroyed (deferred destruction)
class DestroyQueue {
private:
    vk::Device device;
    std::vector<uint64_t> destroy_list;

public:
    void init(vk::Device device);

    template <typename T>
    std::enable_if_t<vk::isVulkanHandleType<T>::value> add(T &vk_object) {
        if (!vk_object)
            return;

        destroy_list.push_back(static_cast<uint64_t>(T::objectType));
        destroy_list.push_back(std::bit_cast<uint64_t>(vk_object));
        vk_object = nullptr;
    }

    void add_image(Image &image);
    void add_buffer(Buffer &buffer);
    void add_cmd_buffer(vk::CommandBuffer cmd_buffer, vk::CommandPool cmd_pool);

    void destroy_objects();
};

} // namespace vkutil
