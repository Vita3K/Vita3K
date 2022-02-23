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

#include <glutil/object_array.h>
#include <renderer/surface_cache.h>

#include <array>
#include <unordered_map>

#include <glad/glad.h>

struct MemState;

namespace renderer::gl {
struct GLRenderTarget;

struct GLSurfaceCacheInfo {
    enum {
        FLAG_DIRTY = 1 << 0,
        FLAG_FREE = 1 << 1
    };

    std::uint32_t flags = FLAG_FREE;
};

struct GLColorSurfaceCacheInfo : public GLSurfaceCacheInfo {
    std::uint16_t width;
    std::uint16_t height;
    std::uint16_t pixel_stride;

    Ptr<void> data;
    GLObjectArray<1> gl_texture;
    GLObjectArray<1> gl_ping_pong_texture;
};

struct GLDepthStencilSurfaceCacheInfo : public GLSurfaceCacheInfo {
    SceGxmDepthStencilSurface surface;
    GLObjectArray<1> gl_texture;
};

class GLSurfaceCache : public SurfaceCache {
private:
    static constexpr std::uint32_t MAX_CACHE_SIZE_PER_CONTAINER = 20;

    std::unordered_map<std::uint64_t, GLColorSurfaceCacheInfo> color_surface_textures;
    std::array<GLDepthStencilSurfaceCacheInfo, MAX_CACHE_SIZE_PER_CONTAINER> depth_stencil_textures;
    std::unordered_map<std::uint64_t, GLObjectArray<1>> framebuffer_array;

    std::vector<std::uint64_t> last_use_color_surface_index;
    std::vector<std::size_t> last_use_depth_stencil_surface_index;

    const GLRenderTarget *target = nullptr;

public:
    explicit GLSurfaceCache();

    std::uint64_t retrieve_color_surface_texture_handle(const std::uint16_t width, const std::uint16_t height, const std::uint16_t pixel_stride,
        Ptr<void> address, SurfaceTextureRetrievePurpose purpose, std::uint16_t *stored_height = nullptr,
        std::uint16_t *stored_width = nullptr) override;
    std::uint64_t retrieve_ping_pong_color_surface_texture_handle(Ptr<void> address) override;

    // We really can't sample this around... The only usage of this function is interally load/store from this texture.
    std::uint64_t retrieve_depth_stencil_texture_handle(const SceGxmDepthStencilSurface &surface) override;
    std::uint64_t retrieve_framebuffer_handle(SceGxmColorSurface *color, SceGxmDepthStencilSurface *depth_stencil,
        std::uint64_t *color_texture_handle = nullptr, std::uint64_t *ds_texture_handle = nullptr,
        std::uint16_t *stored_height = nullptr) override;

    void set_render_target(const GLRenderTarget *new_target) {
        target = new_target;
    }

    std::uint64_t sourcing_color_surface_for_presentation(Ptr<const void> address, const std::uint32_t width, const std::uint32_t height, const std::uint32_t pitch, float *uvs) override;
};
} // namespace renderer::gl
