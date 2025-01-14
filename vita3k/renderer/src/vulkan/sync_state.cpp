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

#include <renderer/vulkan/functions.h>

#include <renderer/vulkan/gxm_to_vulkan.h>

#include <gxm/functions.h>
#include <renderer/functions.h>

#include <util/log.h>

namespace renderer::vulkan {

void sync_clipping(VKContext &context) {
    if (!context.render_target)
        return;

    const float res_multiplier = context.state.res_multiplier;

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
            { static_cast<int32_t>(scissor_x * res_multiplier), static_cast<int32_t>(scissor_y * res_multiplier) },
            { static_cast<uint32_t>(scissor_w * res_multiplier), static_cast<uint32_t>(scissor_h * res_multiplier) }
        };
        break;
    case SCE_GXM_REGION_CLIP_INSIDE:
        // TODO: Implement SCE_GXM_REGION_CLIP_INSIDE
        LOG_WARN("STUB SCE_GXM_REGION_CLIP_INSIDE");
        context.scissor = vk::Rect2D{ { 0, 0 }, { context.render_target->width, context.render_target->height } };
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

void sync_depth_bias(VKContext &context) {
    if (!context.is_recording)
        return;

    context.render_cmd.setDepthBias(static_cast<float>(context.record.depth_bias_unit), 0.0, static_cast<float>(context.record.depth_bias_slope));
}

void sync_depth_data(VKContext &context) {
    if (context.record.depth_stencil_surface.force_load)
        return;

    vk::ClearDepthStencilValue clear_value{
        .depth = context.record.depth_stencil_surface.background_depth
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
    if (context.record.depth_stencil_surface.force_load)
        return;

    vk::ClearDepthStencilValue clear_value{
        .stencil = context.record.depth_stencil_surface.stencil
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

    if (is_front && context.state.physical_device_features.wideLines)
        context.render_cmd.setLineWidth(context.record.line_width * context.state.res_multiplier);
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

    const float res_multiplier = context.state.res_multiplier;

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

void sync_visibility_buffer(VKContext &context, Ptr<uint32_t> buffer, uint32_t stride) {
    if (!buffer) {
        context.current_visibility_buffer = nullptr;
        return;
    }

    auto ite = context.visibility_buffers.find(buffer.address());
    if (ite == context.visibility_buffers.end()) {
        // create a new query pool
        vk::QueryPoolCreateInfo pool_info{
            .queryType = vk::QueryType::eOcclusion,
            .queryCount = static_cast<uint32_t>(stride / sizeof(uint32_t))
        };
        vk::QueryPool query_pool = context.state.device.createQueryPool(pool_info);

        context.visibility_buffers[buffer.address()] = { buffer.address(), nullptr, 0, static_cast<uint32_t>(stride / sizeof(uint32_t)), query_pool };
        ite = context.visibility_buffers.find(buffer.address());

        std::tie(ite->second.gpu_buffer, ite->second.buffer_offset) = context.state.get_matching_mapping(buffer.cast<void>());
        // the + 1 is to make computing the ranges easier in context.cpp
        ite->second.queries_used.resize(ite->second.size + 1, false);
    }

    context.current_visibility_buffer = &ite->second;
}

void sync_visibility_index(VKContext &context, bool enable, uint32_t index, bool is_increment) {
    if (context.current_visibility_buffer == nullptr) {
        context.current_query_idx = enable ? index : -1;
        context.is_query_op_increment = is_increment;
        return;
    }

    if (index >= context.current_visibility_buffer->size) {
        LOG_WARN_ONCE("Using visibility index {} which is too big for the buffer", index);
        index = 0;
    }

    if (!enable) {
        if (context.is_in_query) {
            context.render_cmd.endQuery(context.current_visibility_buffer->query_pool, context.current_query_idx);
            context.is_in_query = false;
        }

        context.current_query_idx = -1;
        return;
    }

    // do not end the query if it's the same index
    if (context.is_in_query && context.current_query_idx != index) {
        context.render_cmd.endQuery(context.current_visibility_buffer->query_pool, context.current_query_idx);
        context.is_in_query = false;
    }
    context.current_query_idx = index;
    context.is_query_op_increment = is_increment;
}

void refresh_pipeline(VKContext &context) {
    context.refresh_pipeline = true;
}

} // namespace renderer::vulkan
