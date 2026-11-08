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

#include "metal_include.h"

#include <gxm/types.h>
#include <mem/ptr.h>
#include <renderer/gxm_types.h>
#include <util/containers.h>

#include <map>
#include <memory>
#include <optional>
#include <vector>

struct MemState;

namespace renderer::metal {

struct MetalState;
struct MetalRenderTarget;
struct Viewport;

struct TextureViewport {
    std::pair<float, float> ratio = { 1.0f, 1.0f };
    std::pair<float, float> offset = { 0.0f, 0.0f };
};

enum struct SurfaceTiling {
    Linear,
    Swizzled,
    Tiled
};

struct CastedTexture {
    // view (with the requested swizzle) of base_texture
    id<MTLTexture> texture = nil;
    id<MTLTexture> base_texture = nil;
    // only used if a texture to texture copy is not possible
    id<MTLBuffer> transition_buffer = nil;
    uint64_t scene_timestamp = 0;
    uint32_t cropped_x = 0;
    uint32_t cropped_y = 0;
    uint32_t cropped_width = 0;
    uint32_t cropped_height = 0;
    SceGxmColorBaseFormat format;
};

struct ColorSurfaceCacheInfo {
    // the texture stores the linear format, views are used for sRGB rendering / sampling
    id<MTLTexture> texture = nil;
    MTLPixelFormat pixel_format = MTLPixelFormatInvalid;
    SurfaceTiling tiling = SurfaceTiling::Linear;
    uint32_t total_bytes = 0;

    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t original_width = 0;
    uint16_t original_height = 0;
    uint32_t stride_bytes = 0;
    uint64_t last_frame_rendered = 0;

    SceGxmColorBaseFormat format;
    MTLTextureSwizzleChannels swizzle;

    Ptr<void> data;
    std::vector<CastedTexture> casted_textures;

    // same texture with a different swizzle / gamma used for sampling
    id<MTLTexture> alternate_view = nil;
    // sRGB view used as a render target
    id<MTLTexture> gamma_view = nil;

    std::shared_ptr<bool> dirty = std::make_shared<bool>(false);
};

struct DepthSurfaceView {
    id<MTLTexture> depth_view = nil;
    // stencil only view of depth_view
    id<MTLTexture> stencil_view = nil;
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t scene_timestamp = 0;
    uint32_t delta_col = 0;
    uint32_t delta_row = 0;
};

struct DepthStencilSurfaceCacheInfo {
    id<MTLTexture> texture = nil;
    SurfaceTiling tiling = SurfaceTiling::Linear;
    uint32_t total_bytes = 0;

    SceGxmDepthStencilSurface surface{};
    int32_t memory_width = 0;
    int32_t memory_height = 0;
    uint32_t stride_samples = 0;
    SceGxmMultisampleMode multisample_mode = SCE_GXM_MULTISAMPLE_NONE;

    // stencil only view of the texture
    id<MTLTexture> stencil_view = nil;

    // used when texture viewport is not enabled
    std::vector<DepthSurfaceView> read_surfaces;
};

struct TextureLookupResult {
    id<MTLTexture> texture;
    MTLPixelFormat format;
};

struct SurfaceRetrieveResult {
    // view used for rendering (may differ from base by its format)
    id<MTLTexture> view = nil;
    id<MTLTexture> base = nil;
    // the surface was just created and its content must be cleared by the next render pass
    bool needs_clear = false;
};

class SurfaceCache {
private:
    MetalState &state;

    static constexpr uint32_t max_surfaces_allowed = 20;

    std::map<Address, ColorSurfaceCacheInfo *> color_address_lookup;
    std::map<Address, DepthStencilSurfaceCacheInfo *> depth_address_lookup;
    std::map<Address, DepthStencilSurfaceCacheInfo *> stencil_address_lookup;

    lru::Queue<ColorSurfaceCacheInfo> color_surface_queue;
    lru::Queue<DepthStencilSurfaceCacheInfo> ds_surface_queue;

    MetalRenderTarget *target = nullptr;

    void destroy_surface(ColorSurfaceCacheInfo &info);
    void destroy_surface(DepthStencilSurfaceCacheInfo &info);

public:
    explicit SurfaceCache(MetalState &state);
    void cleanup();

    SurfaceRetrieveResult retrieve_color_surface_for_framebuffer(MemState &mem, SceGxmColorSurface *color);
    std::optional<TextureLookupResult> retrieve_color_surface_as_texture(const SceGxmTexture &texture, const SceGxmColorBaseFormat base_format, TextureViewport *texture_viewport);

    SurfaceRetrieveResult retrieve_depth_stencil_for_framebuffer(SceGxmDepthStencilSurface *depth_stencil, const uint32_t width, const uint32_t height);
    std::optional<TextureLookupResult> retrieve_depth_stencil_as_texture(const SceGxmTexture &texture, TextureViewport *texture_viewport);

    // Return the texture along with the viewport to be displayed on the screen
    id<MTLTexture> sourcing_color_surface_for_presentation(Ptr<const void> address, uint32_t pitch, Viewport &viewport);

    std::vector<uint32_t> dump_frame(Ptr<const void> address, uint32_t width, uint32_t height, uint32_t pitch);

    void set_render_target(MetalRenderTarget *new_target) {
        target = new_target;
    }
};

} // namespace renderer::metal
