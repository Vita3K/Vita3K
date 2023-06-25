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

#pragma once

#include <renderer/surface_cache.h>

#include <vkutil/objects.h>

struct SwsContext;

namespace renderer::vulkan {

struct VKRenderTarget;
struct VKState;
struct Viewport;

struct SurfaceCacheInfo {
    enum {
        FLAG_DIRTY = 1 << 0,
        FLAG_FREE = 1 << 1
    };

    std::uint32_t flags = FLAG_FREE;
    vkutil::Image texture;
};

struct Framebuffer {
    // standard framebuffer, used most of the time
    vk::Framebuffer standard;
    // framenuffer used with shader interlock
    vk::Framebuffer shader_interlock;
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
    // use a unique_ptr for the following objects as they may not be used

    // same image with a different view(swizzle) used for sampling
    std::unique_ptr<vkutil::Image> sampled_image;

    // only used when upscaling is enabled, to downscale the image first
    std::unique_ptr<vkutil::Image> blit_image;

    // only used for 3-component rgb textures which can't be copied directly
    std::unique_ptr<vkutil::Buffer> copy_buffer;

    // pointer shared with the memory trap indicating if this surface sync is needed
    std::shared_ptr<bool> need_surface_sync;

    // pointer to decoder used for surface sync (if necessary)
    SwsContext *sws_context = nullptr;

    ~ColorSurfaceCacheInfo();
};

struct DepthSurfaceView {
    vkutil::Image depth_view;
    // only contains an image view with the stencil aspect
    vkutil::Image stencil_view;
    // used so that we copy the depth stencil at most once per scene
    uint64_t scene_timestamp;
};

struct DepthStencilSurfaceCacheInfo : public SurfaceCacheInfo {
    SceGxmDepthStencilSurface surface;
    // dimensions of the depth buffer in memory
    int32_t memory_width;
    int32_t memory_height;
    SceGxmMultisampleMode multisample_mode;

    // used when reading from this depth stencil in a shader
    std::vector<DepthSurfaceView> read_surfaces;
};

class VKSurfaceCache : public SurfaceCache {
private:
    VKState &state;

    static constexpr std::uint32_t MAX_CACHE_SIZE_PER_CONTAINER = 20;

    std::map<Address, ColorSurfaceCacheInfo> color_surface_textures;
    std::array<DepthStencilSurfaceCacheInfo, MAX_CACHE_SIZE_PER_CONTAINER> depth_stencil_textures;
    std::map<std::pair<vk::ImageView, vk::ImageView>, Framebuffer> framebuffer_array;

    std::vector<Address> last_use_color_surface_index;
    std::vector<size_t> last_use_depth_stencil_surface_index;

    VKRenderTarget *target = nullptr;
    ColorSurfaceCacheInfo *last_written_surface = nullptr;

    // destroy all framebuffers using view as their color or depth-stencil
    void destroy_framebuffers(vk::ImageView view);

    void destroy_surface(ColorSurfaceCacheInfo &info);
    void destroy_surface(DepthStencilSurfaceCacheInfo &info);

public:
    // when creating a mutable image, can we pass as an argument
    // the possible format used for an image view to improve performance ?
    bool support_image_format_specifier = false;

    // can we protect mapped memory ?
    // On Windows this causes no issue, but according to my test
    // It only works with Nvidia drivers on Linux...
    bool can_mprotect_mapped_memory = true;

    explicit VKSurfaceCache(VKState &state);

    // when writing, the swizzled given to this function is inversed
    vkutil::Image *retrieve_color_surface_texture_handle(MemState &mem, uint16_t width, uint16_t height, const uint16_t pixel_stride,
        const SceGxmColorBaseFormat base_format, const SceGxmColorSurfaceType surface_type, const bool is_srgb, Ptr<void> address, SurfaceTextureRetrievePurpose purpose, vk::ComponentMapping &swizzle,
        uint16_t *stored_height = nullptr, uint16_t *stored_width = nullptr);

    vkutil::Image *retrieve_depth_stencil_texture_handle(const MemState &mem, const SceGxmDepthStencilSurface &surface, int32_t width,
        int32_t height, const bool is_reading = false);

    Framebuffer &retrieve_framebuffer_handle(MemState &mem, SceGxmColorSurface *color, SceGxmDepthStencilSurface *depth_stencil,
        vk::RenderPass standard_render_pass, vk::RenderPass interlock_render_pass, vkutil::Image **color_texture_handle, vkutil::Image **ds_texture_handle,
        uint16_t *stored_height, const uint32_t width, const uint32_t height);

    // If non-null, the return value must be sent as a PostSurfaceSyncRequest
    ColorSurfaceCacheInfo *perform_surface_sync();

    // Called after the render has been done
    void perform_post_surface_sync(const MemState &mem, ColorSurfaceCacheInfo *surface);

    // destroy all framebuffers associated with render_target
    // (meaning their color or depth-stencil surface is not backed by memory)
    void destroy_associated_framebuffers(const VKRenderTarget *render_target);

    // Return the image along with the viewport to be displayed on the screen
    // Viewport should already have its fields width and height filled
    vk::ImageView sourcing_color_surface_for_presentation(Ptr<const void> address, uint32_t pitch, Viewport &viewport);

    void set_render_target(VKRenderTarget *new_target) {
        target = new_target;
    }
};
} // namespace renderer::vulkan