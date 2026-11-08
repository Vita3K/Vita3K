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

#include <gxm/functions.h>
#include <util/log.h>

namespace renderer::metal {

static void apply_scissor(MetalContextImpl &ctx) {
    if (!ctx.encoder)
        return;

    MTLScissorRect rect = ctx.scissor;
    rect.x = std::min<NSUInteger>(rect.x, ctx.framebuffer_width);
    rect.y = std::min<NSUInteger>(rect.y, ctx.framebuffer_height);
    rect.width = std::min<NSUInteger>(rect.width, ctx.framebuffer_width - rect.x);
    rect.height = std::min<NSUInteger>(rect.height, ctx.framebuffer_height - rect.y);
    [ctx.encoder setScissorRect:rect];
}

void sync_clipping(MetalContext &context) {
    MetalContextImpl &ctx = *context.impl;
    if (!ctx.render_target)
        return;

    const float res_multiplier = context.state.res_multiplier;
    const GxmRecordState &record = context.record;

    int32_t scissor_x = record.region_clip_min.x;
    int32_t scissor_y = record.region_clip_min.y;

    const uint32_t scissor_w = std::max(record.region_clip_max.x - record.region_clip_min.x + 1, 0);
    const uint32_t scissor_h = std::max(record.region_clip_max.y - record.region_clip_min.y + 1, 0);

    switch (record.region_clip_mode) {
    case SCE_GXM_REGION_CLIP_NONE:
        // make the scissor the size of the framebuffer
        ctx.scissor = MTLScissorRect{ 0, 0, ctx.render_target->width, ctx.render_target->height };
        break;
    case SCE_GXM_REGION_CLIP_ALL:
        ctx.scissor = MTLScissorRect{ 0, 0, 0, 0 };
        break;
    case SCE_GXM_REGION_CLIP_OUTSIDE: {
        int32_t x = static_cast<int32_t>(scissor_x * res_multiplier);
        int32_t y = static_cast<int32_t>(scissor_y * res_multiplier);
        int32_t w = static_cast<int32_t>(scissor_w * res_multiplier);
        int32_t h = static_cast<int32_t>(scissor_h * res_multiplier);
        // the offset can't be negative
        if (x < 0) {
            w = std::max(w + x, 0);
            x = 0;
        }
        if (y < 0) {
            h = std::max(h + y, 0);
            y = 0;
        }
        ctx.scissor = MTLScissorRect{ static_cast<NSUInteger>(x), static_cast<NSUInteger>(y), static_cast<NSUInteger>(w), static_cast<NSUInteger>(h) };
        break;
    }
    case SCE_GXM_REGION_CLIP_INSIDE:
        // TODO: Implement SCE_GXM_REGION_CLIP_INSIDE
        LOG_WARN("STUB SCE_GXM_REGION_CLIP_INSIDE");
        ctx.scissor = MTLScissorRect{ 0, 0, ctx.render_target->width, ctx.render_target->height };
        break;
    }

    apply_scissor(ctx);
}

void sync_stencil_func(MetalContext &context, const bool is_back) {
    MetalContextImpl &ctx = *context.impl;
    if (!ctx.encoder || ctx.in_maskupdate_pass)
        return;

    const GxmRecordState &record = context.record;
    if (record.two_sided == SCE_GXM_TWO_SIDED_DISABLED) {
        // the back state is not used when two sided is disabled
        if (is_back)
            return;
        [ctx.encoder setStencilReferenceValue:record.front_stencil_state_values.ref];
    } else {
        [ctx.encoder setStencilFrontReferenceValue:record.front_stencil_state_values.ref backReferenceValue:record.back_stencil_state_values.ref];
    }
}

void sync_depth_bias(MetalContext &context) {
    MetalContextImpl &ctx = *context.impl;
    if (!ctx.encoder || ctx.in_maskupdate_pass)
        return;

    [ctx.encoder setDepthBias:static_cast<float>(context.record.depth_bias_unit) slopeScale:static_cast<float>(context.record.depth_bias_slope) clamp:0.0f];
}

void sync_point_line_width(MetalContext &context, const bool is_front) {
    // Metal does not support wide lines
}

void sync_viewport_flat(MetalContext &context) {
    MetalContextImpl &ctx = *context.impl;
    ctx.viewport = MTLViewport{
        .originX = 0.0,
        .originY = 0.0,
        .width = static_cast<double>(ctx.render_target->width),
        .height = static_cast<double>(ctx.render_target->height),
        .znear = 0.0,
        .zfar = 1.0
    };

    if (ctx.encoder)
        [ctx.encoder setViewport:ctx.viewport];
}

void sync_viewport_real(MetalContext &context, const float xOffset, const float yOffset, const float zOffset,
    const float xScale, const float yScale, const float zScale) {
    MetalContextImpl &ctx = *context.impl;
    if (xScale < 0)
        LOG_ERROR("Game is using a viewport with negative width!");

    const float w = std::abs(2 * xScale);
    const float h = 2 * yScale;
    const float x = xOffset - std::abs(xScale);
    const float y = yOffset - yScale;

    const float res_multiplier = context.state.res_multiplier;

    // same as the Vulkan renderer, the shader output is flipped to match the Metal clip space
    // so the viewport is identical (a negative height flips the image)
    ctx.viewport = MTLViewport{
        .originX = x * res_multiplier,
        .originY = y * res_multiplier,
        .width = w * res_multiplier,
        .height = h * res_multiplier,
        .znear = 0.0,
        .zfar = 1.0
    };

    if (ctx.encoder)
        [ctx.encoder setViewport:ctx.viewport];
}

void sync_visibility_buffer(MetalContext &context, Ptr<uint32_t> buffer, uint32_t stride) {
    MetalContextImpl &ctx = *context.impl;
    ctx.end_visibility_query();
    if (!buffer) {
        ctx.current_visibility_buffer = nullptr;
        return;
    }

    auto ite = ctx.visibility_buffers.find(buffer.address());
    if (ite == ctx.visibility_buffers.end()) {
        ite = ctx.visibility_buffers.emplace(buffer.address(), VisibilityBuffer{ buffer.address(), static_cast<uint32_t>(stride / sizeof(uint32_t)), {} }).first;
    }

    ctx.current_visibility_buffer = &ite->second;
}

void sync_visibility_index(MetalContext &context, bool enable, uint32_t index, bool is_increment) {
    MetalContextImpl &ctx = *context.impl;
    if (ctx.current_visibility_buffer == nullptr) {
        ctx.current_query_idx = enable ? index : -1;
        ctx.is_query_op_increment = is_increment;
        return;
    }

    if (index >= ctx.current_visibility_buffer->size) {
        LOG_WARN_ONCE("Using visibility index {} which is too big for the buffer", index);
        index = 0;
    }

    if (!enable) {
        ctx.end_visibility_query();
        ctx.current_query_idx = -1;
        return;
    }

    if (ctx.current_query_idx != static_cast<int>(index) || ctx.is_query_op_increment != is_increment)
        ctx.end_visibility_query();
    ctx.current_query_idx = index;
    ctx.is_query_op_increment = is_increment;
}

void refresh_pipeline(MetalContext &context) {
    context.impl->refresh_pipeline = true;
}

} // namespace renderer::metal
