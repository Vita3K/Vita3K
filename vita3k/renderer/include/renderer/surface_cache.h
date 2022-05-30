// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include <cstdint>

#include <gxm/types.h>
#include <mem/ptr.h>
#include <renderer/gxm_types.h>

struct MemState;

namespace renderer {
enum SurfaceTextureRetrievePurpose {
    READING,
    WRITING
};

class SurfaceCache {
public:
    virtual std::uint64_t retrieve_color_surface_texture_handle(const std::uint16_t width, const std::uint16_t height, const std::uint16_t pixel_stride,
        const SceGxmColorBaseFormat color_format, Ptr<void> address, SurfaceTextureRetrievePurpose purpose, std::uint32_t &swizzle,
        std::uint16_t *stored_height = nullptr, std::uint16_t *stored_width = nullptr)
        = 0;
    virtual std::uint64_t retrieve_ping_pong_color_surface_texture_handle(Ptr<void> address) = 0;

    // We really can't sample this around... The only usage of this function is interally load/store from this texture.
    virtual std::uint64_t retrieve_depth_stencil_texture_handle(const MemState &mem, const SceGxmDepthStencilSurface &surface, std::int32_t force_width = -1,
        std::int32_t force_height = -1, const bool is_reading = false)
        = 0;
    virtual std::uint64_t retrieve_framebuffer_handle(const MemState &mem, SceGxmColorSurface *color, SceGxmDepthStencilSurface *depth_stencil,
        std::uint64_t *color_texture_handle = nullptr, std::uint64_t *ds_texture_handle = nullptr,
        std::uint16_t *stored_height = nullptr)
        = 0;

    virtual std::uint64_t sourcing_color_surface_for_presentation(Ptr<const void> address, const std::uint32_t width, const std::uint32_t height, const std::uint32_t pitch, float *uvs)
        = 0;
};
} // namespace renderer
