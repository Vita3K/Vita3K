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

#include <gxm/types.h>
#include <mem/ptr.h>
#include <renderer/gxm_types.h>
#include <util/containers.h>
#include <vkutil/objects.h>

#include <optional>

struct SwsContext;

namespace renderer::vulkan {

struct VKRenderTarget;
struct VKState;
struct Viewport;
using CallbackRequestFunction = std::function<void()>;

// used for in-shader texture viewport
struct TextureViewport {
    std::pair<float, float> ratio = { 1.0f, 1.0f };
    std::pair<float, float> offset = { 0.0f, 0.0f };
};

enum struct SurfaceTiling {
    Linear,
    Swizzled,
    Tiled
};

struct SurfaceCacheInfo {
    vkutil::Image texture;
    SurfaceTiling tiling;
    // for d32s8 surfaces, this is the size of the depth part
    uint32_t total_bytes;
};

struct Framebuffer {
    // standard framebuffer, used most of the time
    vk::Framebuffer standard;
    // framebuffer used with shader interlock
    vk::Framebuffer shader_interlock;
    // base color image used by the framebuffer
    vkutil::Image *base_image;
};

struct CastedTexture {
    vkutil::Image texture;
    // only used if an image to image copy is not possible
    vkutil::Buffer transition_buffer;
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
    uint32_t stride_bytes;
    uint64_t last_frame_rendered;

    SceGxmColorBaseFormat format;
    vk::ComponentMapping swizzle;

    Ptr<void> data;
    std::vector<CastedTexture> casted_textures;
    // use a unique_ptr for the following objects as they may not be used

    // same image with a different view(swizzle) used for sampling
    vk::ImageView alternate_view = nullptr;

    // only used when upscaling is enabled, to downscale the image first
    std::unique_ptr<vkutil::Image> blit_image;

    // only used for 3-component rgb textures which can't be copied directly
    std::unique_ptr<vkutil::Buffer> copy_buffer;

    // pointer shared with the memory trap indicating if this surface sync is needed
    std::shared_ptr<bool> need_surface_sync;

    // pointer to decoder used for surface sync (if necessary)
    SwsContext *sws_context = nullptr;

    // do we need some CPU convert/unswizzling part for surface sync
    bool need_post_surface_sync = false;

    // only for double buffer, do we need to sync the two views?
    bool need_buffer_sync = false;

    ColorSurfaceCacheInfo() = default;
    ~ColorSurfaceCacheInfo();
};

struct DepthSurfaceView {
    vkutil::Image depth_view;
    // only contains an image view with the stencil aspect
    vkutil::Image stencil_view;
    // used so that we copy the depth stencil at most once per scene
    uint64_t scene_timestamp;
    uint32_t delta_col;
    uint32_t delta_row;
};

struct DepthStencilSurfaceCacheInfo : public SurfaceCacheInfo {
    SceGxmDepthStencilSurface surface;
    // dimensions of the depth buffer in memory
    int32_t memory_width;
    int32_t memory_height;
    // stride in samples
    uint32_t stride_samples;
    SceGxmMultisampleMode multisample_mode;

    // used when reading from this depth stencil in a shader with texture viewport enabled
    vk::ImageView depth_view = nullptr;
    vk::ImageView stencil_view = nullptr;

    // used when texture viewport is not enabled
    std::vector<DepthSurfaceView> read_surfaces;
};

// result when looking in the surface cache for a texture
struct TextureLookupResult {
    vk::ImageView view;
    vkutil::ImageLayout layout;
    vk::Format format;
};

// result when trying to retrieve a surface from the surface cache
struct SurfaceRetrieveResult {
    vk::ImageView view;
    vkutil::Image *base_image;
};

class VKSurfaceCache {
private:
    VKState &state;

    // only have 20 color surfaces and 20 depth surfaces allocated at most at a given time
    static constexpr uint32_t max_surfaces_allowed = 20;

    std::map<Address, ColorSurfaceCacheInfo *> color_address_lookup;

    std::map<Address, DepthStencilSurfaceCacheInfo *> depth_address_lookup;
    std::map<Address, DepthStencilSurfaceCacheInfo *> stencil_address_lookup;

    // structure allowing to set the lru surface with a good complexity
    lru::Queue<ColorSurfaceCacheInfo> color_surface_queue;
    lru::Queue<DepthStencilSurfaceCacheInfo> ds_surface_queue;

    std::map<std::pair<vk::ImageView, vk::ImageView>, Framebuffer> framebuffer_array;

    // used with check_for_surface
    // contains the addresses of the surfaces that are the target
    // of a transfer operation from a surface in the GPU in the current frame
    // use a vector instead of a set because expect it to be always quite small
    std::vector<Address> cpu_surfaces_changed;

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

    SurfaceRetrieveResult retrieve_color_surface_for_framebuffer(MemState &mem, SceGxmColorSurface *color);
    std::optional<TextureLookupResult> retrieve_color_surface_as_texture(const SceGxmTexture &texture, const SceGxmColorBaseFormat base_format, TextureViewport *texture_viewport);

    SurfaceRetrieveResult retrieve_depth_stencil_for_framebuffer(SceGxmDepthStencilSurface *depth_stencil, const uint32_t width, const uint32_t height);
    std::optional<TextureLookupResult> retrieve_depth_stencil_as_texture(const SceGxmTexture &texture, TextureViewport *texture_viewport);

    Framebuffer &retrieve_framebuffer_handle(MemState &mem, SceGxmColorSurface *color, SceGxmDepthStencilSurface *depth_stencil,
        vk::RenderPass standard_render_pass, vk::RenderPass interlock_render_pass, vk::ImageView &color_view, vk::ImageView &ds_view);

    // Check if the address is one of a used surface
    // If it is the case, this function returns true, moves the callback
    // synchronize the surface back to the RAM then only call the callback
    // if this call is used for a copy or similar operation set the changed address to the destination
    // so that subsequent calls to check_for_surface with the target destination also get delayed
    bool check_for_surface(MemState &mem, Address source_address, CallbackRequestFunction &callback, Address target_address);

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

    // Dump an rgba8 frame with the given properties to the returned vector
    // if this function fails, the vector will be empty
    std::vector<uint32_t> dump_frame(Ptr<const void> address, uint32_t width, uint32_t height, uint32_t pitch);

    void set_render_target(VKRenderTarget *new_target) {
        target = new_target;
    }

    void clear_surfaces_changed() {
        cpu_surfaces_changed.clear();
    }
};
} // namespace renderer::vulkan
