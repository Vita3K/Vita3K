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

#include <gxm/types.h>
#include <mutex>
#include <vkutil/objects.h>

struct MemState;
namespace renderer::vulkan {
struct ColorSurfaceCacheInfo;

// Dependencies of the production cached-surface selector, isolated from the
// window/context lifecycle so offscreen tests execute the same crop/swizzle path.
struct SurfaceReadbackContext {
    vk::PhysicalDevice physical;
    vk::Device device;
    vk::Queue queue;
    std::mutex &queue_mutex;
    vk::CommandPool pool;
    std::mutex &pool_mutex;
    vma::Allocator allocator;
    float res_multiplier;
    bool memory_mapping = false;
    bool disable_surface_sync = false;
};

bool readback_cached_surface(MemState &mem, const ColorSurfaceCacheInfo &cached,
    const SceGxmColorSurface &surface, SurfaceReadbackContext &context);
} // namespace renderer::vulkan
