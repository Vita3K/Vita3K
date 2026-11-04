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
#include <mutex>
#include <vector>
#include <vkutil/objects.h>

namespace vkutil {
// Synchronous GPU readback; guest memory mapping is not required. Output is tight.
// Caller serializes the queue/pool, ends renderpasses and keeps source alive.
bool read_image_to_host(vk::PhysicalDevice physical, vk::Device device, vk::Queue queue, std::mutex &queue_mutex,
    vk::CommandPool pool, vma::Allocator allocator, const Image &source,
    uint32_t width, uint32_t height, std::vector<uint8_t> &pixels);
// Convert supported packed565/byte RGB(A) storage without changing output on error.
bool convert_readback_pixels(vk::Format format, const vk::ComponentMapping &swizzle,
    uint32_t guest_bpp, const std::vector<uint8_t> &input, std::vector<uint8_t> &output);
} // namespace vkutil
