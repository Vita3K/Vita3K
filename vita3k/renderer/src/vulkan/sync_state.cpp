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

#include <renderer/vulkan/functions.h>

#include <renderer/vulkan/gxm_to_vulkan.h>

#include <gxm/functions.h>
#include <renderer/functions.h>

#include <util/log.h>

namespace renderer::vulkan {

void sync_clipping(VKContext &context) {
    if (!context.render_target)
        return;

    const int res_multiplier = context.state.res_multiplier;

    const int display_h = context.render_target->height / res_multiplier;
    const int scissor_x = context.record.region_clip_min.x;
    const int scissor_y = context.record.region_clip_min.y;

    const unsigned int scissor_w = std::max(context.record.region_clip_max.x - context.record.region_clip_min.x + 1, 0);
    const unsigned int scissor_h = std::max(context.record.region_clip_max.y - context.record.region_clip_min.y + 1, 0);

    switch (context.record.region_clip_mode) {
    case SCE_GXM_REGION_CLIP_NONE:
        // make the scissor the size of the framebuffer
        context.scissor = vk::Rect2D{ { 0, 0 }, { context.render_target->width, context.render_target->height } };
        break;
    case SCE_GXM_REGION_CLIP_ALL:
        context.scissor = vk::Rect2D{};
        break;
    case SCE_GXM_REGION_CLIP_OUTSIDE:
        context.scissor = vk::Rect2D{
            { scissor_x * res_multiplier, scissor_y * res_multiplier },
            { scissor_w * res_multiplier, scissor_h * res_multiplier }
        };
        break;
    case SCE_GXM_REGION_CLIP_INSIDE:
        // TODO: Implement SCE_GXM_REGION_CLIP_INSIDE
        LOG_WARN("Unimplemented region clip mode used: SCE_GXM_REGION_CLIP_INSIDE");
        context.scissor = vk::Rect2D{};
        break;
    }

    // Vulkan does not allow the offset to be negative
    if (context.scissor.offset.x < 0) {
        context.scissor.extent.width = std::max(context.scissor.extent.width - context.scissor.offset.x, 0U);
        context.scissor.offset.x = 0;
    }

    if (context.scissor.offset.y < 0) {
        context.scissor.extent.height = std::max(context.scissor.extent.height - context.scissor.offset.y, 0U);
        context.scissor.offset.y = 0;
    }

    if (!context.is_recording)
        return;

    context.render_cmd.setScissor(0, context.scissor);
}

void sync_stencil_func(VKContext &context, const bool is_back) {
    if (!context.is_recording)
        return;

    vk::StencilFaceFlags face;
    GxmStencilStateValues *state;
    if (context.record.two_sided == SCE_GXM_TWO_SIDED_DISABLED) {
        // the back state is not used when two sided is disabled
        if (is_back)
            return;

        face = vk::StencilFaceFlagBits::eFrontAndBack;
        state = &context.record.front_stencil_state_values;
    } else {
        face = is_back ? vk::StencilFaceFlagBits::eBack : vk::StencilFaceFlagBits::eFront;
        state = is_back ? &context.record.back_stencil_state_values : &context.record.front_stencil_state_values;
    }

    context.render_cmd.setStencilCompareMask(face, state->compare_mask);
    context.render_cmd.setStencilReference(face, state->ref);
    context.render_cmd.setStencilWriteMask(face, state->write_mask);
}

void sync_mask(VKContext &context, const MemState &mem) {
    if (!context.state.features.use_mask_bit)
        return;

    auto control = context.record.depth_stencil_surface.control.content;
    // mask is not upscaled
    auto width = context.render_target->width / context.state.res_multiplier;
    auto height = context.render_target->height / context.state.res_multiplier;
    float initial_val = (control & SceGxmDepthStencilControl::mask_bit) ? 1.0f : 0.0f;

    std::array<float, 4> clear_bytes = { initial_val, initial_val, initial_val, initial_val };
    vk::ClearColorValue clear_color{ clear_bytes };
    context.render_target->mask.transition_to_discard(context.render_cmd, vkutil::ImageLayout::TransferDst);
    context.render_cmd.clearColorImage(context.render_target->mask.image, vk::ImageLayout::eTransferDstOptimal, clear_color, vkutil::color_subresource_range);
    context.render_target->mask.transition_to(context.render_cmd, vkutil::ImageLayout::StorageImage);
}

void sync_depth_bias(VKContext &context) {
    if (!context.is_recording)
        return;

    context.render_cmd.setDepthBias(static_cast<float>(context.record.depth_bias_unit), 0.0, static_cast<float>(context.record.depth_bias_slope));
}

void sync_depth_data(VKContext &context) {
    // If force load is enabled to load saved depth and depth data memory exists (the second condition is just for safe, may sometimes contradict its usefulness, hopefully won't)
    if ((context.record.depth_stencil_surface.zlsControl & SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED) || (!context.record.depth_stencil_surface.depthData))
        return;

    vk::ClearDepthStencilValue clear_value{
        .depth = context.record.depth_stencil_surface.backgroundDepth
    };
    vk::ClearAttachment clear_attachment{
        .aspectMask = vk::ImageAspectFlagBits::eDepth,
        .clearValue = { .depthStencil = clear_value }
    };
    vk::ClearRect clear_rect{
        .rect = vk::Rect2D{
            .offset = { 0, 0 },
            .extent = { context.render_target->width, context.render_target->height } },
        .baseArrayLayer = 0,
        .layerCount = 1
    };
    context.render_cmd.clearAttachments(clear_attachment, clear_rect);
}

void sync_stencil_data(VKContext &context, const MemState &mem) {
    if (context.record.depth_stencil_surface.zlsControl & SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED)
        return;

    vk::ClearDepthStencilValue clear_value{
        .stencil = context.record.depth_stencil_surface.control.content & SceGxmDepthStencilControl::stencil_bits
    };
    vk::ClearAttachment clear_attachment{
        .aspectMask = vk::ImageAspectFlagBits::eStencil,
        .clearValue = { .depthStencil = clear_value }
    };
    vk::ClearRect clear_rect{
        .rect = vk::Rect2D{
            .offset = { 0, 0 },
            .extent = { context.render_target->width, context.render_target->height } },
        .baseArrayLayer = 0,
        .layerCount = 1
    };
    context.render_cmd.clearAttachments(clear_attachment, clear_rect);
}

void sync_point_line_width(VKContext &context, const bool is_front) {
    if (!context.is_recording)
        return;

    if (is_front)
        context.render_cmd.setLineWidth(static_cast<float>(context.record.line_width));
}

void sync_viewport_flat(VKContext &context) {
    context.viewport = vk::Viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(context.render_target->width),
        .height = static_cast<float>(context.render_target->height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    if (!context.is_recording)
        return;
    context.render_cmd.setViewport(0, context.viewport);
}

void sync_viewport_real(VKContext &context, const float xOffset, const float yOffset, const float zOffset,
    const float xScale, const float yScale, const float zScale) {
    if (xScale < 0)
        LOG_ERROR("Game is using a viewport with negative width!");

    const float w = std::abs(2 * xScale);
    const float h = 2 * yScale;
    const float x = xOffset - std::abs(xScale);
    const float y = yOffset - yScale;

    const int res_multiplier = context.state.res_multiplier;

    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkViewport.html
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#vertexpostproc-viewport
    // on gxm: x_f = xOffset + xScale * (x / w)
    // on vulkan: x_f = (x + width/2) + width/2 * (x / w)
    // the depth viewport is applied directly in the shader
    context.viewport = vk::Viewport{
        .x = x * res_multiplier,
        .y = y * res_multiplier,
        .width = w * res_multiplier,
        .height = h * res_multiplier,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    if (!context.is_recording)
        return;
    context.render_cmd.setViewport(0, context.viewport);
}

void refresh_pipeline(VKContext &context) {
    context.refresh_pipeline = true;
}

} // namespace renderer::vulkan