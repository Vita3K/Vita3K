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

#pragma once

#include <glutil/object_array.h>
#include <gxm/types.h>
#include <mem/ptr.h>
#include <renderer/gxm_types.h>

#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

#include <glad/glad.h>

struct MemState;

namespace renderer {
struct State;

namespace gl {

struct GLRenderTarget;

enum SurfaceTextureRetrievePurpose {
    READING,
    WRITING,
};

struct GLSurfaceCacheInfo {
    enum {
        FLAG_DIRTY = 1 << 0,
        FLAG_FREE = 1 << 1
    };

    std::uint32_t flags = FLAG_FREE;
};

struct GLCastedTexture {
    GLObjectArray<1> texture;
    std::uint64_t last_used_time = 0; // Use for garbage collect on the next frame
    std::uint32_t cropped_x = 0;
    std::uint32_t cropped_y = 0;
    std::uint32_t cropped_width = 0;
    std::uint32_t cropped_height = 0;
    SceGxmColorBaseFormat format;
};

struct GLColorSurfaceCacheInfo : public GLSurfaceCacheInfo {
    std::uint16_t width;
    std::uint16_t height;
    uint16_t original_width;
    uint16_t original_height;
    std::uint16_t pixel_stride;
    std::size_t total_bytes;

    SceGxmColorBaseFormat format;
    std::uint32_t swizzle;

    Ptr<void> data;
    bool is_ping_pong_dirty;
    GLObjectArray<1> gl_texture;
    GLObjectArray<1> gl_ping_pong_texture;
    GLObjectArray<1> gl_expected_read_texture_view;

    std::vector<std::unique_ptr<GLCastedTexture>> casted_textures;
};

struct GLDepthStencilSurfaceCacheInfo : public GLSurfaceCacheInfo {
    SceGxmDepthStencilSurface surface;
    GLObjectArray<1> gl_texture;
    std::int32_t width;
    std::int32_t height;
};

class GLSurfaceCache {
private:
    static constexpr std::uint32_t MAX_CACHE_SIZE_PER_CONTAINER = 20;

    std::map<std::uint64_t, std::unique_ptr<GLColorSurfaceCacheInfo>, std::greater<std::uint64_t>> color_surface_textures;
    std::array<GLDepthStencilSurfaceCacheInfo, MAX_CACHE_SIZE_PER_CONTAINER> depth_stencil_textures;
    std::unordered_map<std::uint64_t, GLObjectArray<1>> framebuffer_array;

    std::vector<size_t> last_use_color_surface_index;
    std::vector<size_t> last_use_depth_stencil_surface_index;

    GLObjectArray<1> typeless_copy_buffer;
    std::size_t typeless_copy_buffer_size = 0;

    const GLRenderTarget *target = nullptr;

private:
    void do_typeless_copy(const GLuint dest_texture, const GLuint source_texture, const GLenum dest_internal,
        const GLenum dest_upload_format, const GLenum dest_type, const GLenum source_format, const GLenum source_type,
        const int offset_x, const int offset_y, const int width, const int height, const int dest_width, const int dest_height, const std::size_t total_source_size);

public:
    explicit GLSurfaceCache();

    GLuint retrieve_color_surface_texture_handle(const State &state, std::uint16_t width, std::uint16_t height, const std::uint16_t pixel_stride,
        const SceGxmColorBaseFormat color_format, Ptr<void> address, SurfaceTextureRetrievePurpose purpose, std::uint32_t &swizzle,
        std::uint16_t *stored_height = nullptr, std::uint16_t *stored_width = nullptr);
    GLuint retrieve_ping_pong_color_surface_texture_handle(Ptr<void> address);

    // We really can't sample this around... The only usage of this function is interally load/store from this texture.
    GLuint retrieve_depth_stencil_texture_handle(const State &state, const MemState &mem, const SceGxmDepthStencilSurface &surface, std::int32_t force_width = -1,
        std::int32_t force_height = -1, const bool is_reading = false);
    GLuint retrieve_framebuffer_handle(const State &state, const MemState &mem, SceGxmColorSurface *color, SceGxmDepthStencilSurface *depth_stencil,
        GLuint *color_texture_handle = nullptr, GLuint *ds_texture_handle = nullptr,
        std::uint16_t *stored_height = nullptr);

    void set_render_target(const GLRenderTarget *new_target) {
        target = new_target;
    }

    GLuint sourcing_color_surface_for_presentation(Ptr<const void> address, uint32_t width, uint32_t height, const std::uint32_t pitch, float *uvs, const float res_multiplier, SceFVector2 &texture_size);
    std::vector<uint32_t> dump_frame(Ptr<const void> address, uint32_t width, uint32_t height, uint32_t pitch, float res_multiplier, bool support_get_texture_sub_image);
};
} // namespace gl
} // namespace renderer
