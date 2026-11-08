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

#include <renderer/metal/functions.h>

#include "gxm_to_metal.h"
#include "mtl_types.h"
#include "shader_translate.h"

#include <gxm/functions.h>
#include <renderer/functions.h>
#include <util/align.h>
#include <util/log.h>

namespace renderer::metal {

MetalContext::MetalContext(MetalState &state, MemState &mem)
    : state(state)
    , mem(mem)
    , impl(std::make_unique<MetalContextImpl>(*this, state, mem)) {
}

MetalContext::~MetalContext() = default;

MetalRenderTarget::MetalRenderTarget() = default;
MetalRenderTarget::~MetalRenderTarget() = default;

MetalContextImpl::MetalContextImpl(MetalContext &owner, MetalState &state, MemState &mem)
    : owner(owner)
    , state(state)
    , mem(mem) {
    memset(&prev_vert_ublock, 0, sizeof(shader::RenderVertUniformBlock));
    memset(&prev_frag_ublock, 0, sizeof(shader::RenderFragUniformBlock));

    MetalDevice &dev = *state.device;

    // for the index buffer, we only have 16 or 32bit types
    index_stream_ring_buffer.alignment = sizeof(uint32_t);
    // for the vertex buffer, nothing should need more alignment than a vec4
    vertex_stream_ring_buffer.alignment = 4 * sizeof(float);
    // 256 is enough for buffer offsets on every GPU
    vertex_uniform_stream_ring_buffer.alignment = 256;
    fragment_uniform_stream_ring_buffer.alignment = 256;
    vertex_info_uniform_buffer.alignment = 256;
    fragment_info_uniform_buffer.alignment = 256;

    vertex_stream_ring_buffer.create(dev.device, MiB(64), @"vertex streams");
    index_stream_ring_buffer.create(dev.device, MiB(64), @"index streams");
    vertex_uniform_stream_ring_buffer.create(dev.device, MiB(64), @"vertex uniforms");
    fragment_uniform_stream_ring_buffer.create(dev.device, MiB(64), @"fragment uniforms");
    vertex_info_uniform_buffer.create(dev.device, MiB(16), @"vertex render info");
    fragment_info_uniform_buffer.create(dev.device, MiB(32), @"fragment render info");

    for (auto &buffer : vertex_stream_buffers)
        buffer = vertex_stream_ring_buffer.buffer;

    // default values for the viewport and scissors
    viewport = MTLViewport{
        .originX = 0.0,
        .originY = 0.0,
        .width = 960.0 * state.res_multiplier,
        .height = 544.0 * state.res_multiplier,
        .znear = 0.0,
        .zfar = 1.0
    };
    scissor = MTLScissorRect{ 0, 0, static_cast<NSUInteger>(960 * state.res_multiplier), static_cast<NSUInteger>(544 * state.res_multiplier) };
}

MetalContextImpl::~MetalContextImpl() {
    // an encoder must be ended before being released
    if (encoder) {
        [encoder endEncoding];
        encoder = nil;
    }
    end_prerender_blit();
    render_cmd = nil;
    prerender_cmd = nil;
}

bool create(MetalState &state, std::unique_ptr<Context> &context, MemState &mem) {
    @autoreleasepool {
        context = std::make_unique<MetalContext>(state, mem);
    }
    return true;
}

bool create(MetalState &state, std::unique_ptr<RenderTarget> &rt, const SceGxmRenderTargetParams &params, const FeatureState &features) {
    @autoreleasepool {
        MetalDevice &dev = *state.device;
        auto target = std::make_unique<MetalRenderTarget>();
        target->impl = std::make_unique<MetalRenderTargetImpl>();
        target->width = static_cast<uint16_t>(params.width * state.res_multiplier);
        target->height = static_cast<uint16_t>(params.height * state.res_multiplier);

        uint32_t ds_width = target->width;
        uint32_t ds_height = target->height;
        if (params.multisampleMode == SCE_GXM_MULTISAMPLE_4X) {
            // the depth buffer may need to be 4x bigger if we use a texture without downscale
            ds_width *= 2;
            ds_height *= 2;
        }

        target->impl->color = create_texture(dev.device, MTLPixelFormatRGBA8Unorm, target->width, target->height,
            MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead, 1, false, 1, MTLStorageModePrivate, "render target color");
        target->impl->depthstencil = create_texture(dev.device, depth_stencil_format, ds_width, ds_height,
            MTLTextureUsageRenderTarget, 1, false, 1, MTLStorageModePrivate, "render target depth-stencil");
        if (!target->impl->color || !target->impl->depthstencil)
            return false;

        if (features.use_mask_bit) {
            target->impl->mask = create_texture(dev.device, MTLPixelFormatRGBA8Unorm, ds_width, ds_height,
                MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite, 1, false, 1, MTLStorageModePrivate, "render target mask");
        }

        rt = std::move(target);
    }
    return true;
}

void destroy(MetalState &state, std::unique_ptr<RenderTarget> &rt) {
    // Metal keeps the textures alive while they are referenced by a command buffer, nothing to do
}

void set_context(MetalContext &context, MemState &mem, MetalRenderTarget *rt, const FeatureState &features) {
    @autoreleasepool {
        MetalContextImpl &ctx = *context.impl;
        MetalState &state = context.state;
        MetalDevice &dev = *state.device;

        ctx.render_target = rt;
        ctx.scene_timestamp++;
        ctx.gxm_scene_id++;
        dev.texture_cache.current_scene_timestamp = ctx.scene_timestamp;

        SceGxmColorSurface *color_surface_fin = &context.record.color_surface;
        // set these values for the pipeline cache
        context.record.color_base_format = gxm::get_base_format(color_surface_fin->colorFormat);
        context.record.is_gamma_corrected = static_cast<bool>(color_surface_fin->gamma);
        MTLPixelFormat format = color::translate_format(context.record.color_base_format, dev.support_packed16);

        if (color_surface_fin->gamma && format == MTLPixelFormatRGBA8Unorm)
            format = MTLPixelFormatRGBA8Unorm_sRGB;

        if (color_surface_fin->data.address() == 0) {
            color_surface_fin = nullptr;

            // set back default values
            format = MTLPixelFormatRGBA8Unorm;
            context.record.color_surface.downscale = static_cast<bool>(rt->multisample_mode);
            context.record.is_gamma_corrected = false;
            context.record.is_maskupdate = false;
            context.record.color_base_format = SCE_GXM_COLOR_BASE_FORMAT_U8U8U8U8;
        }
        ctx.current_color_format = format;

        if (rt->multisample_mode && !context.record.color_surface.downscale) {
            // using MSAA without downscaling, emulate this as best as we can by multiplying the width and height of the render target by 2
            rt->width *= 2;
            rt->height *= 2;
        }

        SceGxmDepthStencilSurface *ds_surface_fin = &context.record.depth_stencil_surface;
        // if the depth-stencil buffer is not backed by memory or we don't read nor write it to memory, use the transient attachment instead
        if ((!ds_surface_fin->depth_data && !ds_surface_fin->stencil_data)
            || (!ds_surface_fin->force_load && !ds_surface_fin->force_store)) {
            ds_surface_fin = nullptr;
        }

        dev.surface_cache.set_render_target(rt);

        ctx.start_recording(true);

        if (features.use_mask_bit && rt->impl->mask) {
            const float initial_mask = context.record.depth_stencil_surface.mask ? 1.0f : 0.0f;
            rt->impl->mask_may_reject = initial_mask == 0.0f;

            // clear the mask with an empty render pass
            MTLRenderPassDescriptor *desc = [MTLRenderPassDescriptor renderPassDescriptor];
            desc.colorAttachments[0].texture = rt->impl->mask;
            desc.colorAttachments[0].loadAction = MTLLoadActionClear;
            desc.colorAttachments[0].storeAction = MTLStoreActionStore;
            desc.colorAttachments[0].clearColor = MTLClearColorMake(initial_mask, initial_mask, initial_mask, initial_mask);
            id<MTLRenderCommandEncoder> clear_encoder = [ctx.render_cmd renderCommandEncoderWithDescriptor:desc];
            clear_encoder.label = @"mask clear";
            [clear_encoder endEncoding];
        }

        bool force_load = context.record.depth_stencil_surface.force_load;
        if (ds_surface_fin == nullptr)
            // no memory backing the depth stencil
            force_load = false;

        // might get modified by the surface cache
        dev.pipeline_cache.can_use_deferred_compilation = true;

        SurfaceRetrieveResult color_result;
        SurfaceRetrieveResult ds_result;
        if (color_surface_fin) {
            color_result = dev.surface_cache.retrieve_color_surface_for_framebuffer(mem, color_surface_fin);
        } else {
            color_result.view = rt->impl->color;
            color_result.base = rt->impl->color;
        }

        if (ds_surface_fin) {
            ds_result = dev.surface_cache.retrieve_depth_stencil_for_framebuffer(ds_surface_fin, rt->width, rt->height);
        } else {
            ds_result.view = rt->impl->depthstencil;
            ds_result.base = rt->impl->depthstencil;
        }

        ctx.current_color_view = color_result.view;
        ctx.current_color_base = color_result.base;
        ctx.current_ds_texture = ds_result.view;
        ctx.color_is_transient = color_surface_fin == nullptr;
        ctx.color_clear_pending = color_result.needs_clear;

        ctx.initial_depth_stencil_clear = !force_load;
        ctx.depth_stencil_clear_pending = !force_load || ds_result.needs_clear;
        if (force_load) {
            // a freshly created surface is cleared to the default values
            ctx.ds_clear_depth = 1.0f;
            ctx.ds_clear_stencil = 0;
        } else {
            ctx.ds_clear_depth = context.record.depth_stencil_surface.background_depth;
            ctx.ds_clear_stencil = context.record.depth_stencil_surface.stencil;
        }
        ctx.first_pass_of_scene = true;

        // make sure we are not keeping any texture from the previous pass
        // (textures can be still bound even though they are not used)
        for (int i = 0; i < SCE_GXM_MAX_TEXTURE_UNITS; i++) {
            ctx.vertex_textures[i] = {};
            ctx.fragment_textures[i] = {};
        }
        ctx.vertex_textures_dirty = true;
        ctx.fragment_textures_dirty = true;

        ctx.is_first_scene_draw = true;
        ctx.initialized_macroblocks = 0;
        ctx.last_macroblock_x = ~0;
        ctx.last_macroblock_y = ~0;
        ctx.ignore_macroblock = false;
    }
}

id<MTLBlitCommandEncoder> MetalContextImpl::blit() {
    if (!prerender_blit) {
        prerender_blit = [prerender_cmd blitCommandEncoder];
        prerender_blit.label = @"prerender";
    }
    return prerender_blit;
}

void MetalContextImpl::end_prerender_blit() {
    if (prerender_blit) {
        [prerender_blit endEncoding];
        prerender_blit = nil;
    }
}

void MetalContextImpl::flush_prerender() {
    end_prerender_blit();
    [prerender_cmd commit];
    state.device->frame().rendered_cmds.push_back(prerender_cmd);

    prerender_cmd = [state.device->queue commandBuffer];
    prerender_cmd.label = @"prerender";
}

void MetalContextImpl::start_recording(bool first_in_scene) {
    if (is_recording) {
        LOG_ERROR("Attempt to start recording while already recording");
        return;
    }

    if (render_target == nullptr) {
        LOG_ERROR("Recording started without a render target");
        return;
    }

    MetalDevice &dev = *state.device;
    render_target->impl->last_used_frame = dev.frame_timestamp;

    prerender_cmd = [dev.queue commandBuffer];
    prerender_cmd.label = @"prerender";
    render_cmd = [dev.queue commandBuffer];
    render_cmd.label = @"render";
    prerender_blit = nil;
    encoder = nil;

    // the result buffer must be set when the render pass starts, allocate one if the game uses visibility buffers
    if (!visibility_buffers.empty() && !visibility_result_buffer)
        visibility_result_buffer = create_shared_buffer(dev.device, max_visibility_queries * sizeof(uint64_t), "visibility results");

    is_recording = true;
    current_pipeline = nil;
    current_depth_stencil = nil;
    raster_state_dirty = true;
    vertex_textures_dirty = true;
    fragment_textures_dirty = true;
}

void MetalContextImpl::apply_dynamic_state() {
    if (!encoder)
        return;

    [encoder setViewport:viewport];

    // the scissor must be inside the render target
    MTLScissorRect rect = scissor;
    rect.x = std::min<NSUInteger>(rect.x, framebuffer_width);
    rect.y = std::min<NSUInteger>(rect.y, framebuffer_height);
    rect.width = std::min<NSUInteger>(rect.width, framebuffer_width - rect.x);
    rect.height = std::min<NSUInteger>(rect.height, framebuffer_height - rect.y);
    [encoder setScissorRect:rect];

    [encoder setFrontFacingWinding:MTLWindingCounterClockwise];
    if (!in_maskupdate_pass)
        [encoder setDepthBias:static_cast<float>(owner.record.depth_bias_unit) slopeScale:static_cast<float>(owner.record.depth_bias_slope) clamp:0.0f];

    const GxmRecordState &record = owner.record;
    if (record.two_sided == SCE_GXM_TWO_SIDED_ENABLED)
        [encoder setStencilFrontReferenceValue:record.front_stencil_state_values.ref backReferenceValue:record.back_stencil_state_values.ref];
    else
        [encoder setStencilReferenceValue:record.front_stencil_state_values.ref];

    raster_state_dirty = true;
}

void MetalContextImpl::start_render_pass(bool maskupdate) {
    if (in_renderpass) {
        LOG_ERROR("Starting render pass while already in render pass");
        return;
    }

    if (!is_recording)
        start_recording();

    MetalDevice &dev = *state.device;

    bool macroblock_clear = false;
    if (!maskupdate && state.features.use_mask_bit && render_target->has_macroblock_sync && !ignore_macroblock) {
        // Only the first pass may discard the attachment; later new blocks need a local clear.
        const uint16_t macroblock_bit = 1U << (last_macroblock_x + 4 * last_macroblock_y);
        is_first_scene_draw = (initialized_macroblocks & macroblock_bit) == 0;
        const bool first_macroblock = initialized_macroblocks == 0;
        if (is_first_scene_draw && !first_macroblock)
            macroblock_clear = initial_depth_stencil_clear;
        initialized_macroblocks |= macroblock_bit;
    }

    MTLRenderPassDescriptor *desc = [MTLRenderPassDescriptor renderPassDescriptor];
    if (maskupdate) {
        desc.colorAttachments[0].texture = render_target->impl->mask;
        desc.colorAttachments[0].loadAction = MTLLoadActionLoad;
        desc.colorAttachments[0].storeAction = MTLStoreActionStore;
        framebuffer_width = static_cast<uint32_t>(render_target->impl->mask.width);
        framebuffer_height = static_cast<uint32_t>(render_target->impl->mask.height);
    } else {
        desc.colorAttachments[0].texture = current_color_view;
        if (color_clear_pending) {
            desc.colorAttachments[0].loadAction = MTLLoadActionClear;
            desc.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
            color_clear_pending = false;
        } else if (color_is_transient && first_pass_of_scene) {
            desc.colorAttachments[0].loadAction = MTLLoadActionDontCare;
        } else {
            desc.colorAttachments[0].loadAction = MTLLoadActionLoad;
        }
        desc.colorAttachments[0].storeAction = MTLStoreActionStore;

        desc.depthAttachment.texture = current_ds_texture;
        desc.stencilAttachment.texture = current_ds_texture;
        if (depth_stencil_clear_pending) {
            desc.depthAttachment.loadAction = MTLLoadActionClear;
            desc.depthAttachment.clearDepth = ds_clear_depth;
            desc.stencilAttachment.loadAction = MTLLoadActionClear;
            desc.stencilAttachment.clearStencil = ds_clear_stencil;
        } else {
            desc.depthAttachment.loadAction = MTLLoadActionLoad;
            desc.stencilAttachment.loadAction = MTLLoadActionLoad;
        }
        desc.depthAttachment.storeAction = MTLStoreActionStore;
        desc.stencilAttachment.storeAction = MTLStoreActionStore;

        if (visibility_result_buffer)
            desc.visibilityResultBuffer = visibility_result_buffer;

        framebuffer_width = static_cast<uint32_t>(std::min(current_color_view.width, current_ds_texture.width));
        framebuffer_height = static_cast<uint32_t>(std::min(current_color_view.height, current_ds_texture.height));
    }
    desc.renderTargetWidth = framebuffer_width;
    desc.renderTargetHeight = framebuffer_height;

    encoder = [render_cmd renderCommandEncoderWithDescriptor:desc];
    encoder.label = maskupdate ? @"mask update" : @"scene";

    in_renderpass = true;
    in_maskupdate_pass = maskupdate;
    first_pass_of_scene = false;
    depth_stencil_clear_pending = false;

    refresh_pipeline = true;
    current_pipeline = nil;
    current_depth_stencil = nil;
    vertex_textures_dirty = true;
    fragment_textures_dirty = true;
    bound_vertex_texture_count = 0;
    bound_fragment_texture_count = 0;
    for (auto &buffer : bound_vertex_buffers)
        buffer = nil;

    apply_dynamic_state();

    if (macroblock_clear) {
        const MTLScissorRect region{
            static_cast<NSUInteger>(last_macroblock_x * render_target->macroblock_width),
            static_cast<NSUInteger>(last_macroblock_y * render_target->macroblock_height),
            render_target->macroblock_width,
            render_target->macroblock_height
        };
        clear_depth_stencil_region(region, true, true);
    }

    if (!maskupdate) {
        // the attachments read by the fragment shader
        if (state.features.use_mask_bit && render_target->impl->mask)
            [encoder setFragmentTexture:render_target->impl->mask atIndex:msl::MASK_TEXTURE_INDEX];
        if (!dev.support_framebuffer_fetch)
            [encoder setFragmentTexture:current_color_view atIndex:msl::COLOR_ATTACHMENT_TEXTURE_INDEX];
        [encoder setFragmentTexture:current_color_base atIndex:msl::COLOR_ATTACHMENT_RAW_TEXTURE_INDEX];
    }
}

void MetalContextImpl::stop_render_pass() {
    if (!in_renderpass) {
        LOG_ERROR("Stopping render pass while not in render pass");
        return;
    }

    // do this before ending the render pass
    end_visibility_query();

    [encoder endEncoding];
    encoder = nil;

    in_renderpass = false;
    in_maskupdate_pass = false;
}

void MetalContextImpl::clear_depth_stencil_region(const MTLScissorRect &region, bool clear_depth, bool clear_stencil) {
    if (!encoder || in_maskupdate_pass)
        return;

    MetalDevice &dev = *state.device;
    id<MTLRenderPipelineState> pipeline = dev.pipeline_cache.get_clear_pipeline(current_color_format, true);
    id<MTLDepthStencilState> ds_state = dev.pipeline_cache.get_clear_depth_stencil_state(clear_depth, clear_stencil);
    if (!pipeline || !ds_state)
        return;

    MTLScissorRect rect = region;
    rect.x = std::min<NSUInteger>(rect.x, framebuffer_width);
    rect.y = std::min<NSUInteger>(rect.y, framebuffer_height);
    rect.width = std::min<NSUInteger>(rect.width, framebuffer_width - rect.x);
    rect.height = std::min<NSUInteger>(rect.height, framebuffer_height - rect.y);
    if (rect.width == 0 || rect.height == 0)
        return;

    [encoder setRenderPipelineState:pipeline];
    [encoder setDepthStencilState:ds_state];
    [encoder setScissorRect:rect];
    [encoder setViewport:MTLViewport{ 0.0, 0.0, static_cast<double>(framebuffer_width), static_cast<double>(framebuffer_height), 0.0, 1.0 }];
    [encoder setCullMode:MTLCullModeNone];
    [encoder setTriangleFillMode:MTLTriangleFillModeFill];
    [encoder setDepthBias:0.0f slopeScale:0.0f clamp:0.0f];
    [encoder setStencilReferenceValue:ds_clear_stencil];
    const float depth = ds_clear_depth;
    [encoder setVertexBytes:&depth length:sizeof(depth) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

    // the state must be set again
    current_pipeline = nil;
    current_depth_stencil = nil;
    for (auto &buffer : bound_vertex_buffers)
        buffer = nil;
    apply_dynamic_state();
}

void MetalContextImpl::begin_visibility_query() {
    if (!encoder || !visibility_result_buffer || !current_visibility_buffer)
        return;

    const uint32_t slot = static_cast<uint32_t>(visibility_queries.size());
    if (slot >= max_visibility_queries) {
        LOG_WARN_ONCE("Too many visibility queries in a single scene");
        return;
    }

    visibility_queries.push_back({ current_visibility_buffer, static_cast<uint32_t>(current_query_idx), is_query_op_increment, slot });
    [encoder setVisibilityResultMode:(is_query_op_increment ? MTLVisibilityResultModeCounting : MTLVisibilityResultModeBoolean)
                              offset:slot * sizeof(uint64_t)];
    is_in_query = true;
}

void MetalContextImpl::end_visibility_query() {
    if (!is_in_query)
        return;
    if (encoder)
        [encoder setVisibilityResultMode:MTLVisibilityResultModeDisabled offset:0];
    is_in_query = false;
}

namespace {
struct VisibilityGroup {
    Address address;
    uint32_t index;
    bool use_previous;
    std::vector<std::pair<uint32_t, bool>> queries;
};
} // namespace

void MetalContextImpl::resolve_visibility_queries() {
    if (visibility_queries.empty() || !visibility_result_buffer) {
        visibility_queries.clear();
        return;
    }

    std::map<std::pair<Address, uint32_t>, VisibilityGroup> groups;
    for (const auto &query : visibility_queries) {
        auto &group = groups[{ query.buffer->address, query.index }];
        if (group.queries.empty()) {
            group.address = query.buffer->address;
            group.index = query.index;
            const auto previous_scene = query.buffer->result_scenes.find(query.index);
            group.use_previous = previous_scene != query.buffer->result_scenes.end() && previous_scene->second == gxm_scene_id;
            query.buffer->result_scenes[query.index] = gxm_scene_id;
        }
        group.queries.emplace_back(query.slot, query.increment);
    }

    auto groups_list = std::make_shared<std::vector<VisibilityGroup>>();
    for (auto &[key, group] : groups)
        groups_list->push_back(std::move(group));

    id<MTLBuffer> results = visibility_result_buffer;
    MemState *mem_ptr = &mem;
    [render_cmd addCompletedHandler:^(id<MTLCommandBuffer>) {
        const uint64_t *counts = static_cast<const uint64_t *>(results.contents);
        for (const VisibilityGroup &group : *groups_list) {
            uint32_t *destination = Ptr<uint32_t>(group.address + group.index * sizeof(uint32_t)).get(*mem_ptr);
            if (!destination)
                continue;
            uint32_t result = group.use_previous ? *destination : 0;
            for (const auto &[slot, increment] : group.queries) {
                const uint64_t samples = counts[slot];
                if (increment)
                    result += static_cast<uint32_t>(samples);
                else if (samples != 0)
                    result = 1;
            }
            *destination = result;
        }
    }];

    visibility_queries.clear();
    // use a new buffer for the next scene, this one is still being read
    visibility_result_buffer = nil;
}

void MetalContextImpl::stop_recording(bool submit) {
    if (!is_recording) {
        LOG_ERROR("Stopping recording while not recording");
        return;
    }

    if (in_renderpass)
        stop_render_pass();

    resolve_visibility_queries();

    end_prerender_blit();

    // the prerender cmd must be submitted before the render cmd
    [prerender_cmd commit];
    [render_cmd commit];

    FrameObject &frame = state.device->frame();
    frame.rendered_cmds.push_back(render_cmd);

    render_cmd = nil;
    prerender_cmd = nil;
    is_recording = false;
    current_pipeline = nil;
    current_depth_stencil = nil;

    if (!submit)
        return;

    if (render_target->multisample_mode && !owner.record.color_surface.downscale) {
        // revert changes made in set_context
        render_target->width /= 2;
        render_target->height /= 2;
    }
}

void MetalContextImpl::check_for_macroblock_change(bool is_draw) {
    if (!render_target->has_macroblock_sync)
        return;

    if (!ignore_macroblock && (scissor.width > render_target->macroblock_width || scissor.height > render_target->macroblock_height)) {
        // flower does not specify a scissor adapted to the current macroblock
        // so fallback to the slow path (one scene per draw, can't really do better)
        ignore_macroblock = true;
    }

    // use the scissor to know in which macroblock we are
    const uint16_t curr_macroblock_x = static_cast<uint16_t>(scissor.x / render_target->macroblock_width);
    const uint16_t curr_macroblock_y = static_cast<uint16_t>(scissor.y / render_target->macroblock_height);

    if ((ignore_macroblock && is_draw) || curr_macroblock_x != last_macroblock_x || curr_macroblock_y != last_macroblock_y) {
        // we changed the current macroblock, restart the render pass
        last_macroblock_x = curr_macroblock_x;
        last_macroblock_y = curr_macroblock_y;

        if (in_renderpass) {
            if (state.features.use_texture_viewport) {
                // no need to copy the texture after this render pass
                stop_render_pass();
            } else {
                stop_recording(false);
                start_recording();
                scene_timestamp++;
            }
        }
    }
}

bool is_recording(const MetalContext &context) {
    return context.impl->is_recording;
}

void stop_recording(MetalContext &context) {
    @autoreleasepool {
        context.impl->stop_recording(true);
    }
}

void new_frame(MetalContext &context) {
    @autoreleasepool {
        MetalDevice &dev = *context.state.device;

        dev.frame_timestamp++;
        dev.current_frame_idx = dev.frame_timestamp % MAX_FRAMES_RENDERING;

        FrameObject &frame = dev.frame();

        // wait for the command buffers submitted MAX_FRAMES_RENDERING frames ago
        for (id<MTLCommandBuffer> cmd : frame.rendered_cmds)
            [cmd waitUntilCompleted];
        frame.rendered_cmds.clear();

        frame.frame_timestamp = dev.frame_timestamp;

        context.impl->vertex_textures_dirty = true;
        context.impl->fragment_textures_dirty = true;
    }
}

} // namespace renderer::metal
