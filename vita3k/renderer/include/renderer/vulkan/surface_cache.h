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

#include <renderer/surface_cache.h>

#include <vkutil/objects.h>

namespace renderer::vulkan {

struct VKRenderTarget;
struct VKState;

struct SurfaceCacheInfo {
    enum {
        FLAG_DIRTY = 1 << 0,
        FLAG_FREE = 1 << 1
    };

    std::uint32_t flags = FLAG_FREE;
    vkutil::Image texture;
};

struct CastedTexture {
    vkutil::Image texture;
    // only used if an image to image copy is not possible
    vkutil::Buffer transition_buffer;
    uint64_t last_used_time = 0; // Use for garbage collect on the next frame
    uint64_t scene_timestamp = 0;
    uint32_t cropped_x = 0;
    uint32_t cropped_y = 0;
    uint32_t cropped_width = 0;
    uint32_t cropped_height = 0;
    SceGxmColorBaseFormat format;
};

struct ColorSurfaceCacheInfo : public SurfaceCacheInfo {
    uint16_t width;
    uint16_t height;
    uint16_t original_width;
    uint16_t original_height;
    uint16_t pixel_stride;
    size_t total_bytes;

    SceGxmColorBaseFormat format;
    vk::ComponentMapping swizzle;

    Ptr<void> data;
    std::vector<CastedTexture> casted_textures;
    // same image with a different view(swizzle) used for sampling
    vkutil::Image sampled_image;
};

struct DepthStencilSurfaceCacheInfo : public SurfaceCacheInfo {
    SceGxmDepthStencilSurface surface;
    int32_t width;
    int32_t height;

    // used when reading from this depth stencil in a shader
    vkutil::Image read_only;
    // used so that we copy the depth stencil at most once per scene
    uint64_t scene_timestamp = 0;
};

class VKSurfaceCache : public SurfaceCache {
private:
    VKState &state;

    static constexpr std::uint32_t MAX_CACHE_SIZE_PER_CONTAINER = 20;

    std::map<Address, ColorSurfaceCacheInfo> color_surface_textures;
    std::array<DepthStencilSurfaceCacheInfo, MAX_CACHE_SIZE_PER_CONTAINER> depth_stencil_textures;
    std::map<std::pair<vk::ImageView, vk::ImageView>, vk::Framebuffer> framebuffer_array;

    std::vector<Address> last_use_color_surface_index;
    std::vector<size_t> last_use_depth_stencil_surface_index;

    VKRenderTarget *target = nullptr;

    // destroy all framebuffers using view as their color or depth-stencil
    void destroy_framebuffers(vk::ImageView view);

    void destroy_surface(ColorSurfaceCacheInfo &info);
    void destroy_surface(DepthStencilSurfaceCacheInfo &info);

public:
    explicit VKSurfaceCache(VKState &state);

    // when writing, the swizzled given to this function is inversed
    vkutil::Image *retrieve_color_surface_texture_handle(uint16_t width, uint16_t height, const uint16_t pixel_stride,
        const SceGxmColorBaseFormat base_format, const bool is_srgb, Ptr<void> address, SurfaceTextureRetrievePurpose purpose, vk::ComponentMapping &swizzle,
        uint16_t *stored_height = nullptr, uint16_t *stored_width = nullptr);

    vkutil::Image *retrieve_depth_stencil_texture_handle(const MemState &mem, const SceGxmDepthStencilSurface &surface, int32_t force_width = -1,
        int32_t force_height = -1, const bool is_reading = false);

    vk::Framebuffer retrieve_framebuffer_handle(const MemState &mem, SceGxmColorSurface *color, SceGxmDepthStencilSurface *depth_stencil,
        vk::RenderPass render_pass, vkutil::Image **color_texture_handle = nullptr, vkutil::Image **ds_texture_handle = nullptr,
        uint16_t *stored_height = nullptr);

    // destroy all framebuffers associated with render_target
    // (meaning their color or depth-stencil surface is not backed by memory)
    void destroy_associated_framebuffers(const VKRenderTarget *render_target);

    vk::ImageView sourcing_color_surface_for_presentation(Ptr<const void> address, uint32_t width, uint32_t height, const uint32_t pitch, std::array<float, 4> &uvs, const int res_multiplier, SceFVector2 &texture_size);

    void set_render_target(VKRenderTarget *new_target) {
        target = new_target;
    }
};
} // namespace renderer::vulkan