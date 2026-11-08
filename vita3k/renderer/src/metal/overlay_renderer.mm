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

#include "overlay_renderer.h"

#include "mtl_types.h"
#include "shader_translate.h"

#include <overlay/font.h>
#include <util/log.h>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace renderer::metal {

static constexpr float VIRTUAL_WIDTH = 960.f;
static constexpr float VIRTUAL_HEIGHT = 544.f;

OverlayRenderer::~OverlayRenderer() {
    destroy();
}

static id<MTLFunction> compile_builtin(id<MTLDevice> device, const fs::path &path, msl::Stage stage, const std::vector<msl::BindingRemap> &remaps, const char *label) {
    const std::vector<uint32_t> spirv = msl::load_spirv_file(path);
    if (spirv.empty()) {
        LOG_ERROR("Failed to load builtin shader {}", path.string());
        return nil;
    }

    const msl::TranslateResult result = msl::translate_builtin_shader(spirv, stage, remaps, msl::BUILTIN_PUSH_CONSTANT_BUFFER_INDEX);
    if (!result.ok) {
        LOG_ERROR("Failed to translate builtin shader {} to MSL: {}", label, result.error);
        return nil;
    }

    NSError *error = nil;
    MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
    options.languageVersion = MTLLanguageVersion2_3;
    id<MTLLibrary> library = [device newLibraryWithSource:[NSString stringWithUTF8String:result.source.c_str()] options:options error:&error];
    if (!library) {
        LOG_ERROR("Failed to compile builtin shader {}: {}", label, error ? error.localizedDescription.UTF8String : "unknown error");
        return nil;
    }

    id<MTLFunction> function = [library newFunctionWithName:[NSString stringWithUTF8String:result.entry_point.c_str()]];
    if (!function && library.functionNames.count > 0)
        function = [library newFunctionWithName:library.functionNames[0]];
    return function;
}

bool OverlayRenderer::init(MetalState &state) {
    @autoreleasepool {
        m_state = &state;
        id<MTLDevice> device = state.device->device;

        const fs::path builtin_shaders_path = state.static_assets / "shaders-builtin" / "overlay";
        m_vert_function = compile_builtin(device, builtin_shaders_path / "overlay.vert.spv", msl::Stage::Vertex, {}, "overlay.vert");
        m_frag_function = compile_builtin(device, builtin_shaders_path / "overlay.frag.spv", msl::Stage::Fragment,
            { { 0, 0, 0, 0, 0 }, { 0, 1, 0, 1, 1 } }, "overlay.frag");

        if (!m_vert_function || !m_frag_function) {
            LOG_ERROR("Failed to load overlay shaders");
            return false;
        }

        if (!create_pipeline())
            return false;

        MTLSamplerDescriptor *sampler_desc = [[MTLSamplerDescriptor alloc] init];
        sampler_desc.magFilter = MTLSamplerMinMagFilterLinear;
        sampler_desc.minFilter = MTLSamplerMinMagFilterLinear;
        sampler_desc.mipFilter = MTLSamplerMipFilterNearest;
        sampler_desc.sAddressMode = MTLSamplerAddressModeClampToEdge;
        sampler_desc.tAddressMode = MTLSamplerAddressModeClampToEdge;
        sampler_desc.rAddressMode = MTLSamplerAddressModeClampToEdge;
        m_font_sampler = [device newSamplerStateWithDescriptor:sampler_desc];

        sampler_desc.mipFilter = MTLSamplerMipFilterLinear;
        sampler_desc.lodMaxClamp = 1000.0f;
        m_image_sampler = [device newSamplerStateWithDescriptor:sampler_desc];

        const uint32_t white_pixel = 0xFFFFFFFF;
        m_dummy_texture = create_texture(device, MTLPixelFormatRGBA8Unorm, 1, 1, MTLTextureUsageShaderRead, 1, false, 1, MTLStorageModeManaged, "overlay dummy");
        [m_dummy_texture replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&white_pixel bytesPerRow:4];

        m_dummy_texture_array = create_texture(device, MTLPixelFormatRGBA8Unorm, 1, 1, MTLTextureUsageShaderRead, 1, false, 2, MTLStorageModeManaged, "overlay dummy array");
        [m_dummy_texture_array replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 slice:0 withBytes:&white_pixel bytesPerRow:4 bytesPerImage:4];
        [m_dummy_texture_array replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 slice:1 withBytes:&white_pixel bytesPerRow:4 bytesPerImage:4];

        m_vertex_buffers.resize(ScreenRenderer::frame_slots);

        return true;
    }
}

bool OverlayRenderer::create_pipeline() {
    id<MTLDevice> device = m_state->device->device;

    MTLVertexDescriptor *vertex_descriptor = [MTLVertexDescriptor vertexDescriptor];
    vertex_descriptor.attributes[0].format = MTLVertexFormatFloat4;
    vertex_descriptor.attributes[0].offset = 0;
    vertex_descriptor.attributes[0].bufferIndex = msl::BUILTIN_VERTEX_BUFFER_INDEX;
    vertex_descriptor.layouts[msl::BUILTIN_VERTEX_BUFFER_INDEX].stride = sizeof(overlay::vertex);
    vertex_descriptor.layouts[msl::BUILTIN_VERTEX_BUFFER_INDEX].stepFunction = MTLVertexStepFunctionPerVertex;

    MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
    desc.vertexFunction = m_vert_function;
    desc.fragmentFunction = m_frag_function;
    desc.vertexDescriptor = vertex_descriptor;
    desc.colorAttachments[0].pixelFormat = m_state->device->screen_renderer.format;
    desc.colorAttachments[0].blendingEnabled = YES;
    desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    desc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorZero;
    desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
    desc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    desc.colorAttachments[0].writeMask = MTLColorWriteMaskAll;

    NSError *error = nil;
    m_pipeline = [device newRenderPipelineStateWithDescriptor:desc error:&error];
    if (!m_pipeline) {
        LOG_ERROR("Failed to create the overlay Metal pipeline: {}", error ? error.localizedDescription.UTF8String : "unknown error");
        return false;
    }
    return true;
}

void OverlayRenderer::destroy() {
    if (!m_state)
        return;

    m_pipeline = nil;
    m_vert_function = nil;
    m_frag_function = nil;
    m_font_sampler = nil;
    m_image_sampler = nil;
    m_font_atlas_cache.clear();
    m_dummy_texture = nil;
    m_dummy_texture_array = nil;
    m_vertex_buffers.clear();
    m_icon_images.clear();
    m_raw_image_cache.clear();
    m_prepared_views.clear();

    m_resources.free_resources();
    m_resources_loaded = false;

    m_state = nullptr;
}

id<MTLTexture> OverlayRenderer::upload_font(id<MTLCommandBuffer> cmd, const overlay::font *f) {
    if (!f || !m_state)
        return nil;

    const auto dims = f->get_glyph_data_dimensions();
    if (dims.depth == 0)
        return nil;

    auto &entry = m_font_atlas_cache[f];

    if (entry.texture && entry.layers == dims.depth)
        return entry.texture;

    const auto &glyph_data = f->get_glyph_data();
    if (glyph_data.empty())
        return nil;

    id<MTLDevice> device = m_state->device->device;

    MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
    desc.textureType = MTLTextureType2DArray;
    desc.pixelFormat = MTLPixelFormatR8Unorm;
    desc.width = dims.width;
    desc.height = dims.height;
    desc.mipmapLevelCount = 1;
    desc.arrayLength = dims.depth;
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeManaged;
    desc.swizzle = MTLTextureSwizzleChannels{ MTLTextureSwizzleRed, MTLTextureSwizzleRed, MTLTextureSwizzleRed, MTLTextureSwizzleRed };
    id<MTLTexture> texture = [device newTextureWithDescriptor:desc];
    if (!texture)
        return nil;
    texture.label = @"overlay font";

    const size_t page_size = static_cast<size_t>(dims.width) * dims.height;
    for (uint32_t layer = 0; layer < dims.depth; ++layer) {
        [texture replaceRegion:MTLRegionMake2D(0, 0, dims.width, dims.height)
                   mipmapLevel:0
                         slice:layer
                     withBytes:glyph_data.data() + layer * page_size
                   bytesPerRow:dims.width
                 bytesPerImage:page_size];
    }

    entry.texture = texture;
    entry.layers = dims.depth;
    return texture;
}

void OverlayRenderer::upload_image(id<MTLCommandBuffer> cmd, const overlay::image_info_base *img, id<MTLTexture> __strong &dst) {
    if (!img || !img->get_data() || !m_state)
        return;

    id<MTLDevice> device = m_state->device->device;

    const MTLPixelFormat fmt = (img->channels == 4) ? MTLPixelFormatRGBA8Unorm : MTLPixelFormatR8Unorm;
    const uint32_t w = static_cast<uint32_t>(img->w);
    const uint32_t h = static_cast<uint32_t>(img->h);
    const bool need_mipmaps = std::max(w, h) > MIPMAP_SIZE_THRESHOLD;
    const uint32_t mip_levels = need_mipmaps
        ? static_cast<uint32_t>(std::floor(std::log2(std::max(w, h)))) + 1
        : 1;

    if (dst && (dst.width != w || dst.height != h || dst.pixelFormat != fmt))
        dst = nil;

    if (!dst) {
        dst = create_texture(device, fmt, w, h, MTLTextureUsageShaderRead, mip_levels, false, 1, MTLStorageModeManaged, "overlay image");
        if (!dst)
            return;
    }

    const uint32_t bytes_per_pixel = (img->channels == 4) ? 4 : 1;
    [dst replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 withBytes:img->get_data() bytesPerRow:w * bytes_per_pixel];

    if (need_mipmaps) {
        id<MTLBlitCommandEncoder> blit = [cmd blitCommandEncoder];
        [blit generateMipmapsForTexture:dst];
        [blit endEncoding];
    }

    img->dirty = false;
}

void OverlayRenderer::draw_command(id<MTLRenderCommandEncoder> encoder,
    const overlay::compiled_resource::command &draw_cmd,
    float viewport_w, float viewport_h,
    float viewport_x, float viewport_y) {
    if (draw_cmd.verts.empty())
        return;

    const auto &config = draw_cmd.config;

    const uint32_t data_size = static_cast<uint32_t>(draw_cmd.verts.size() * sizeof(overlay::vertex));
    const uint32_t aligned_size = (data_size + 255) & ~255u;
    const uint32_t required = m_vertex_buffer_offset + aligned_size;

    id<MTLBuffer> __strong &vb = m_vertex_buffers[m_active_frame_slot];
    if (!vb || required > vb.length) {
        vb = create_shared_buffer(m_state->device->device, std::max<size_t>(static_cast<size_t>(required) * 2, 256 * 1024), "overlay vertices");
        m_vertex_buffer_offset = 0;
    }

    memcpy(static_cast<uint8_t *>(vb.contents) + m_vertex_buffer_offset, draw_cmd.verts.data(), data_size);
    const uint32_t this_offset = m_vertex_buffer_offset;
    m_vertex_buffer_offset += aligned_size;

    id<MTLTexture> tex_2d = m_dummy_texture;
    id<MTLTexture> tex_array = m_dummy_texture_array;

    const uint8_t ref = config.texture_ref;
    if (ref == overlay::image_resource_none
        || ref == overlay::game_icon
        || ref == overlay::backbuffer) {
    } else if (config.font_ref) {
        auto it = m_font_atlas_cache.find(config.font_ref);
        if (it != m_font_atlas_cache.end() && it->second.texture)
            tex_array = it->second.texture;
    } else if (ref == overlay::raw_image && config.external_data_ref) {
        auto it = m_raw_image_cache.find(config.external_data_ref);
        if (it != m_raw_image_cache.end() && it->second)
            tex_2d = it->second;
    } else {
        const size_t idx = static_cast<size_t>(ref - 1);
        if (idx < m_icon_images.size() && m_icon_images[idx])
            tex_2d = m_icon_images[idx];
    }

    [encoder setFragmentTexture:tex_2d atIndex:0];
    [encoder setFragmentSamplerState:m_image_sampler atIndex:0];
    [encoder setFragmentTexture:tex_array atIndex:1];
    [encoder setFragmentSamplerState:m_font_sampler atIndex:1];

    OverlayPushConstants pc{};

    pc.ui_scale[0] = VIRTUAL_WIDTH;
    pc.ui_scale[1] = VIRTUAL_HEIGHT;
    pc.ui_scale[2] = 1.f;
    pc.ui_scale[3] = 1.f;

    pc.albedo[0] = config.color.r;
    pc.albedo[1] = config.color.g;
    pc.albedo[2] = config.color.b;
    pc.albedo[3] = config.color.a;

    pc.viewport[0] = viewport_w;
    pc.viewport[1] = viewport_h;
    pc.viewport[2] = viewport_x;
    pc.viewport[3] = viewport_y;

    pc.clip_bounds[0] = config.clip_rect.x1;
    pc.clip_bounds[1] = config.clip_rect.y1;
    pc.clip_bounds[2] = config.clip_rect.x2;
    pc.clip_bounds[3] = config.clip_rect.y2;

    uint32_t vert_cfg = 0;
    if (config.disable_vertex_snap)
        vert_cfg |= 1u;
    pc.vertex_config = vert_cfg;

    uint32_t frag_cfg = 0;
    if (config.clip_region)
        frag_cfg |= 1u;
    if (config.pulse_glow)
        frag_cfg |= 2u;

    uint32_t sampler_mode = 0;
    if (config.font_ref) {
        sampler_mode = 2;
    } else if (config.texture_ref == overlay::font_file) {
        sampler_mode = 1;
    } else if (config.texture_ref != overlay::image_resource_none
        && config.texture_ref != overlay::game_icon
        && config.texture_ref != overlay::backbuffer) {
        sampler_mode = 3;
    }
    frag_cfg |= (sampler_mode & 3u) << 2u;

    const bool is_sdf = config.active_effect == overlay::compiled_resource::effect_type::sdf
        && config.effect.sdf.func != overlay::sdf_function::none;
    const bool is_gloss = config.active_effect == overlay::compiled_resource::effect_type::gloss;
    const bool is_btn_gloss = config.active_effect == overlay::compiled_resource::effect_type::btn_gloss;
    const bool has_sdf_btn_gloss = is_sdf && config.effect.sdf.btn_gloss_height > 0.f;

    const uint32_t sdf_type = is_sdf ? static_cast<uint32_t>(config.effect.sdf.func) : 0u;
    frag_cfg |= (sdf_type & 3u) << 4u;
    if (is_gloss)
        frag_cfg |= 64u;
    if (is_btn_gloss || has_sdf_btn_gloss)
        frag_cfg |= 128u;
    pc.fragment_config = frag_cfg;

    pc.timestamp = config.get_sinus_value();

    pc.blur_intensity = static_cast<float>(config.blur_strength);

    // SDF parameters - transform from virtual space to viewport pixel space
    if (is_sdf) {
        auto sdf = config.effect.sdf;
        overlay::areaf target_vp;
        target_vp.x1 = viewport_x;
        target_vp.y1 = viewport_y;
        target_vp.x2 = viewport_x + viewport_w;
        target_vp.y2 = viewport_y + viewport_h;
        sdf.transform(target_vp, { VIRTUAL_WIDTH, VIRTUAL_HEIGHT });

        pc.sdf_params[0] = sdf.hx;
        pc.sdf_params[1] = sdf.hy;
        pc.sdf_params[2] = sdf.br;
        pc.sdf_params[3] = sdf.bw;
        pc.sdf_origin[0] = sdf.cx;
        pc.sdf_origin[1] = sdf.cy;
        if (has_sdf_btn_gloss) {
            pc.sdf_origin[2] = sdf.btn_gloss_height;
            pc.sdf_origin[3] = std::round(sdf.btn_gloss_opacity * 100.f) + sdf.btn_gloss_bottom_opacity;
        }
        pc.sdf_border_color[0] = sdf.border_color.r;
        pc.sdf_border_color[1] = sdf.border_color.g;
        pc.sdf_border_color[2] = sdf.border_color.b;
        pc.sdf_border_color[3] = sdf.border_color.a;
    } else if (is_gloss) {
        const auto &g = config.effect.gloss;
        pc.sdf_params[0] = g.height;
        pc.sdf_params[1] = g.feather;
        pc.sdf_params[2] = g.opacity;
    } else if (is_btn_gloss) {
        const auto &bg = config.effect.btn_gloss;
        pc.sdf_params[0] = bg.height;
        pc.sdf_params[1] = bg.curve_lift;
        pc.sdf_params[2] = bg.opacity;
        pc.sdf_params[3] = bg.border_radius_frac;
        pc.sdf_origin[0] = bg.aspect;
        pc.sdf_origin[1] = bg.bottom_opacity;
    }

    [encoder setVertexBytes:&pc length:sizeof(pc) atIndex:msl::BUILTIN_PUSH_CONSTANT_BUFFER_INDEX];
    [encoder setFragmentBytes:&pc length:sizeof(pc) atIndex:msl::BUILTIN_PUSH_CONSTANT_BUFFER_INDEX];

    [encoder setVertexBuffer:vb offset:this_offset atIndex:msl::BUILTIN_VERTEX_BUFFER_INDEX];

    const uint32_t vert_count = static_cast<uint32_t>(draw_cmd.verts.size());
    switch (config.primitives) {
    case overlay::primitive_type::quad_list: {
        const uint32_t num_quads = vert_count / 4;
        for (uint32_t i = 0; i < num_quads; ++i)
            [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:i * 4 vertexCount:4];
        break;
    }
    case overlay::primitive_type::triangle_strip:
        [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:vert_count];
        break;
    case overlay::primitive_type::line_list:
        [encoder drawPrimitives:MTLPrimitiveTypeLine vertexStart:0 vertexCount:vert_count];
        break;
    case overlay::primitive_type::line_strip:
        [encoder drawPrimitives:MTLPrimitiveTypeLineStrip vertexStart:0 vertexCount:vert_count];
        break;
    case overlay::primitive_type::triangle_fan: {
        // Metal has no triangle fans
        for (uint32_t i = 2; i < vert_count; i++) {
            // draw each triangle (0, i-1, i) as a strip of 3 vertices starting at i - 1 is not possible, use the first vertex
            const uint32_t indices[3] = { 0, i - 1, i };
            overlay::vertex fan[3] = { draw_cmd.verts[indices[0]], draw_cmd.verts[indices[1]], draw_cmd.verts[indices[2]] };
            [encoder setVertexBytes:fan length:sizeof(fan) atIndex:msl::BUILTIN_VERTEX_BUFFER_INDEX];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        }
        break;
    }
    }
}

void OverlayRenderer::prepare(id<MTLCommandBuffer> cmd_buffer,
    const overlay::display_manager &manager,
    float viewport_w, float viewport_h,
    uint32_t frame_slot) {
    m_prepared_views.clear();

    if (!m_state)
        return;

    if (m_vertex_buffers.size() <= frame_slot)
        m_vertex_buffers.resize(frame_slot + 1);
    m_active_frame_slot = frame_slot;
    m_vertex_buffer_offset = 0;

    manager.lock_shared();

    for (const auto &view : manager.get_views()) {
        if (!view || !view->visible.load())
            continue;

        view->set_render_viewport(
            static_cast<uint16_t>(std::min<uint32_t>(static_cast<uint32_t>(viewport_w), UINT16_MAX)),
            static_cast<uint16_t>(std::min<uint32_t>(static_cast<uint32_t>(viewport_h), UINT16_MAX)));

        auto compiled = view->get_compiled();

        for (const auto &draw_cmd : compiled.draw_commands) {
            const auto &config = draw_cmd.config;

            if (config.font_ref) {
                upload_font(cmd_buffer, config.font_ref);
            } else if (config.texture_ref == overlay::raw_image && config.external_data_ref) {
                id<MTLTexture> __strong &img = m_raw_image_cache[config.external_data_ref];
                const auto *info = static_cast<const overlay::image_info_base *>(config.external_data_ref);
                if (!img || info->dirty)
                    upload_image(cmd_buffer, info, img);
            } else if (config.texture_ref > 0
                && config.texture_ref != overlay::font_file
                && config.texture_ref != overlay::game_icon
                && config.texture_ref != overlay::backbuffer
                && config.texture_ref < overlay::raw_image) {
                if (!m_resources_loaded) {
                    m_resources.load_files();
                    m_resources_loaded = true;
                    m_icon_images.clear();
                }
                const size_t idx = static_cast<size_t>(config.texture_ref - 1);
                if (idx < m_resources.texture_raw_data.size()) {
                    if (m_icon_images.size() <= idx)
                        m_icon_images.resize(idx + 1);
                    id<MTLTexture> __strong &icon = m_icon_images[idx];
                    const auto *info = m_resources.texture_raw_data[idx].get();
                    if (info && (!icon || info->dirty))
                        upload_image(cmd_buffer, info, icon);
                }
            }
        }

        m_prepared_views.push_back({ view, std::move(compiled) });
    }

    manager.unlock_shared();
}

void OverlayRenderer::render(id<MTLRenderCommandEncoder> encoder, float viewport_x, float viewport_y, float viewport_w, float viewport_h) {
    if (!m_state || m_prepared_views.empty() || !m_pipeline || !encoder)
        return;

    [encoder setRenderPipelineState:m_pipeline];
    [encoder setCullMode:MTLCullModeNone];
    [encoder setTriangleFillMode:MTLTriangleFillModeFill];

    [encoder setViewport:MTLViewport{ viewport_x, viewport_y, viewport_w, viewport_h, 0.0, 1.0 }];

    const ScreenRenderer &screen = m_state->device->screen_renderer;
    MTLScissorRect scissor{
        static_cast<NSUInteger>(std::max(viewport_x, 0.0f)),
        static_cast<NSUInteger>(std::max(viewport_y, 0.0f)),
        static_cast<NSUInteger>(std::max(viewport_w, 0.0f)),
        static_cast<NSUInteger>(std::max(viewport_h, 0.0f)),
    };
    scissor.x = std::min<NSUInteger>(scissor.x, screen.width);
    scissor.y = std::min<NSUInteger>(scissor.y, screen.height);
    scissor.width = std::min<NSUInteger>(scissor.width, screen.width - scissor.x);
    scissor.height = std::min<NSUInteger>(scissor.height, screen.height - scissor.y);
    [encoder setScissorRect:scissor];

    for (auto &prepared : m_prepared_views) {
        for (const auto &draw_cmd : prepared.compiled.draw_commands) {
            draw_command(encoder, draw_cmd, viewport_w, viewport_h, viewport_x, viewport_y);
        }

        const auto timestamp_us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
                                      .count();
        prepared.view->update(static_cast<uint64_t>(timestamp_us));
    }

    m_prepared_views.clear();
}

} // namespace renderer::metal
