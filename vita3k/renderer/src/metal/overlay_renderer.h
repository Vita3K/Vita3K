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

#include <overlay/compiled_resource.h>
#include <overlay/controls.h>
#include <overlay/display_manager.h>

#include <memory>
#include <unordered_map>
#include <vector>

namespace renderer::metal {

struct MetalState;

struct OverlayPushConstants {
    float ui_scale[4];
    float albedo[4];
    float viewport[4];
    float clip_bounds[4];
    uint32_t vertex_config;
    uint32_t fragment_config;
    float timestamp;
    float blur_intensity;
    float sdf_params[4];
    float sdf_origin[4];
    float sdf_border_color[4];
};
static_assert(sizeof(OverlayPushConstants) == 128);

class OverlayRenderer {
public:
    OverlayRenderer() = default;
    ~OverlayRenderer();

    bool init(MetalState &state);
    void destroy();

    void prepare(id<MTLCommandBuffer> cmd_buffer, const overlay::display_manager &manager, float viewport_w, float viewport_h, uint32_t frame_slot);

    void render(id<MTLRenderCommandEncoder> encoder, float viewport_x, float viewport_y, float viewport_w, float viewport_h);

private:
    MetalState *m_state = nullptr;

    static constexpr int NUM_TOPOLOGIES = 4;
    id<MTLRenderPipelineState> m_pipeline = nil;
    id<MTLFunction> m_vert_function = nil;
    id<MTLFunction> m_frag_function = nil;

    uint32_t m_active_frame_slot = 0;

    struct FontAtlasEntry {
        id<MTLTexture> texture = nil;
        size_t layers = 0;
    };
    std::unordered_map<const overlay::font *, FontAtlasEntry> m_font_atlas_cache;
    id<MTLSamplerState> m_font_sampler = nil;
    id<MTLSamplerState> m_image_sampler = nil;

    std::vector<id<MTLTexture>> m_icon_images;
    std::unordered_map<const void *, id<MTLTexture>> m_raw_image_cache;

    std::vector<id<MTLBuffer>> m_vertex_buffers;
    uint32_t m_vertex_buffer_offset = 0;

    id<MTLTexture> m_dummy_texture = nil;
    id<MTLTexture> m_dummy_texture_array = nil;

    overlay::resource_config m_resources;
    bool m_resources_loaded = false;

    struct PreparedView {
        std::shared_ptr<overlay::overlay> view;
        overlay::compiled_resource compiled;
    };
    std::vector<PreparedView> m_prepared_views;

    static constexpr uint32_t MIPMAP_SIZE_THRESHOLD = 256;

    bool create_pipeline();
    id<MTLTexture> upload_font(id<MTLCommandBuffer> cmd, const overlay::font *f);
    void upload_image(id<MTLCommandBuffer> cmd, const overlay::image_info_base *img, id<MTLTexture> __strong &dst);
    void draw_command(id<MTLRenderCommandEncoder> encoder, const overlay::compiled_resource::command &draw_cmd, float viewport_w, float viewport_h, float viewport_x, float viewport_y);
};

} // namespace renderer::metal
