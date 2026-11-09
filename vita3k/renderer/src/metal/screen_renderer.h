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

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace renderer::metal {

struct MetalState;
class ScreenRenderer;

struct Viewport {
    uint32_t offset_x;
    uint32_t offset_y;
    uint32_t width;
    uint32_t height;
    uint32_t texture_width;
    uint32_t texture_height;
};

class ScreenFilter {
protected:
    ScreenRenderer &screen;

public:
    ScreenFilter(ScreenRenderer &screen_renderer)
        : screen(screen_renderer) {}
    virtual ~ScreenFilter() = default;
    virtual bool init() = 0;
    virtual void on_resize() {}
    // called twice per frame: before the presentation render pass starts (compute filters), and inside it
    virtual void render(bool is_pre_renderpass, id<MTLTexture> src, const Viewport &viewport) = 0;
    virtual std::string_view get_name() = 0;
    // does the presentation render pass need to keep the drawable content?
    virtual bool need_post_processing_render_pass() {
        return false;
    }
};

class SinglePassScreenFilter : public ScreenFilter {
private:
    id<MTLRenderPipelineState> pipeline = nil;
    id<MTLSamplerState> sampler = nil;
    std::vector<id<MTLBuffer>> vertex_buffers;
    std::vector<std::array<float, 4>> last_uvs;

protected:
    virtual std::string_view get_vertex_name();
    virtual std::string_view get_fragment_name();
    virtual id<MTLSamplerState> create_sampler() = 0;

public:
    SinglePassScreenFilter(ScreenRenderer &screen)
        : ScreenFilter(screen) {}
    bool init() override;
    void render(bool is_pre_renderpass, id<MTLTexture> src, const Viewport &viewport) override;
};

class NearestScreenFilter : public SinglePassScreenFilter {
protected:
    id<MTLSamplerState> create_sampler() override;

public:
    NearestScreenFilter(ScreenRenderer &screen)
        : SinglePassScreenFilter(screen) {}
    std::string_view get_name() override {
        return "Nearest";
    }
};

class BilinearScreenFilter : public SinglePassScreenFilter {
protected:
    id<MTLSamplerState> create_sampler() override;

public:
    BilinearScreenFilter(ScreenRenderer &screen)
        : SinglePassScreenFilter(screen) {}
    std::string_view get_name() override {
        return "Bilinear";
    }
};

class BicubicScreenFilter : public SinglePassScreenFilter {
protected:
    std::string_view get_fragment_name() override;
    id<MTLSamplerState> create_sampler() override;

public:
    BicubicScreenFilter(ScreenRenderer &screen)
        : SinglePassScreenFilter(screen) {}
    std::string_view get_name() override {
        return "Bicubic";
    }
};

class FXAAScreenFilter : public SinglePassScreenFilter {
protected:
    std::string_view get_fragment_name() override;
    id<MTLSamplerState> create_sampler() override;

public:
    FXAAScreenFilter(ScreenRenderer &screen)
        : SinglePassScreenFilter(screen) {}
    std::string_view get_name() override {
        return "FXAA";
    }
};

class FSRScreenFilter : public ScreenFilter {
private:
    id<MTLComputePipelineState> pipeline_easu = nil;
    id<MTLComputePipelineState> pipeline_rcas = nil;
    id<MTLSamplerState> sampler = nil;
    // dst of the easu shader, src of the rcas shader
    std::vector<id<MTLTexture>> intermediate_images;

    uint32_t output_offset_x = 0;
    uint32_t output_offset_y = 0;
    uint32_t output_width = 0;
    uint32_t output_height = 0;

public:
    FSRScreenFilter(ScreenRenderer &screen)
        : ScreenFilter(screen) {}
    bool init() override;
    void on_resize() override;
    void render(bool is_pre_renderpass, id<MTLTexture> src, const Viewport &viewport) override;
    std::string_view get_name() override {
        return "FSR";
    }
    bool need_post_processing_render_pass() override {
        return true;
    }
};

class ScreenRenderer {
public:
    MetalState &state;

    CAMetalLayer *layer = nil;
    MTLPixelFormat format = MTLPixelFormatBGRA8Unorm;
    uint32_t width = 0;
    uint32_t height = 0;
    static constexpr uint32_t frame_slots = 3;
    // index in [0, frame_slots) of the frame being rendered
    uint32_t current_slot = 0;

    id<CAMetalDrawable> drawable = nil;
    id<MTLCommandBuffer> current_cmd_buffer = nil;
    id<MTLRenderCommandEncoder> current_encoder = nil;

    std::unique_ptr<ScreenFilter> filter;

    // textures used when the surface is not in the surface cache
    std::array<id<MTLTexture>, frame_slots> vita_surface = {};
    // the command buffer which last used the resources of a slot
    std::array<id<MTLCommandBuffer>, frame_slots> slot_cmds = {};

    bool vsync = true;

    explicit ScreenRenderer(MetalState &state);

    bool create();
    void cleanup();

    bool acquire_drawable();
    void begin_render_pass(bool clear);
    void render(id<MTLTexture> texture, const Viewport &viewport);
    void swap_window();
    void set_filter(const std::string_view &filter);
    void set_vsync(bool enable);

    // true if the drawable size changed since the last call
    bool update_drawable_size();
};

} // namespace renderer::metal
