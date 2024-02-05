// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <gxm/types.h>
#include <renderer/commands.h>
#include <renderer/driver_functions.h>
#include <renderer/state.h>
#include <renderer/types.h>

#include <renderer/gl/functions.h>
#include <renderer/gl/state.h>
#include <renderer/gl/types.h>

#include <renderer/vulkan/functions.h>
#include <renderer/vulkan/state.h>
#include <renderer/vulkan/types.h>

#include <util/align.h>
#include <util/log.h>
#include <util/tracy.h>

#include <config/state.h>

namespace renderer {
COMMAND_SET_STATE(region_clip) {
    TRACY_FUNC_COMMANDS_SET_STATE(region_clip);
    render_context->record.region_clip_mode = helper.pop<SceGxmRegionClipMode>();

    // see COMMAND_SET_STATE(viewport) for an explanation
    uint32_t factor = 1;
    const bool has_msaa = (render_context->current_render_target && render_context->current_render_target->multisample_mode);
    const bool has_downscale = render_context->record.color_surface.downscale;
    if (has_msaa && !has_downscale)
        factor = 2;
    else if (!has_msaa && has_downscale)
        factor = 1;

    const uint32_t xMin = helper.pop<uint32_t>() * factor;
    const uint32_t xMax = helper.pop<uint32_t>() * factor;
    const uint32_t yMin = helper.pop<uint32_t>() * factor;
    const uint32_t yMax = helper.pop<uint32_t>() * factor;

    render_context->record.region_clip_min.x = static_cast<SceInt>(align_down(xMin, SCE_GXM_TILE_SIZEX));
    render_context->record.region_clip_min.y = static_cast<SceInt>(align_down(yMin, SCE_GXM_TILE_SIZEY));
    // borders are inclusive
    render_context->record.region_clip_max.x = static_cast<SceInt>(align(xMax, SCE_GXM_TILE_SIZEX)) - 1;
    render_context->record.region_clip_max.y = static_cast<SceInt>(align(yMax, SCE_GXM_TILE_SIZEY)) - 1;

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        gl::sync_clipping(static_cast<gl::GLState &>(renderer), *static_cast<gl::GLContext *>(render_context));
        break;

    case Backend::Vulkan:
        vulkan::sync_clipping(*static_cast<vulkan::VKContext *>(render_context));
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(program) {
    TRACY_FUNC_COMMANDS_SET_STATE(program);
    const Ptr<void> program = helper.pop<Ptr<void>>();
    const bool is_fragment = helper.pop<bool>();

    if (is_fragment) {
        render_context->record.fragment_program = program.cast<SceGxmFragmentProgram>();
        const SceGxmFragmentProgram *gxm_program = render_context->record.fragment_program.get(mem);
        render_context->record.fragment_program_hash = gxm_program->renderer_data->hash;
        render_context->record.is_maskupdate = gxm_program->is_maskupdate;

        switch (renderer.current_backend) {
        case Backend::OpenGL:
            gl::sync_blending(render_context->record, mem);
            break;

        case Backend::Vulkan:
            break;

        default:
            REPORT_MISSING(renderer.current_backend);
            break;
        }
    } else {
        render_context->record.vertex_program = program.cast<SceGxmVertexProgram>();
        const SceGxmVertexProgram *gxm_program = render_context->record.vertex_program.get(mem);
        render_context->record.vertex_program_hash = gxm_program->renderer_data->hash;
    }

    if (renderer.current_backend == Backend::Vulkan) {
        vulkan::refresh_pipeline(*reinterpret_cast<vulkan::VKContext *>(render_context));
    }
}

COMMAND_SET_STATE(uniform_buffer) {
    TRACY_FUNC_COMMANDS_SET_STATE(uniform_buffer);
    const Ptr<uint8_t> data = helper.pop<const Ptr<uint8_t>>();
    const bool is_vertex = helper.pop<bool>();
    const int block_num = helper.pop<int>();
    const std::uint32_t size = helper.pop<std::uint32_t>();

    renderer::ShaderProgram *program = is_vertex ? reinterpret_cast<ShaderProgram *>(render_context->record.vertex_program.get(mem)->renderer_data.get())
                                                 : reinterpret_cast<ShaderProgram *>(render_context->record.fragment_program.get(mem)->renderer_data.get());

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        gl::set_uniform_buffer(*reinterpret_cast<gl::GLContext *>(render_context), program, is_vertex, block_num, size, data.get(mem));
        break;

    case Backend::Vulkan:
        vulkan::set_uniform_buffer(*reinterpret_cast<vulkan::VKContext *>(render_context), mem, program, is_vertex, block_num, size, data);
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        return;
    }

    const auto offset = program->uniform_buffer_data_offsets.at(block_num);
    if (offset != static_cast<uint32_t>(-1) && config.log_active_shaders) {
        const int base_binding_ubo_relative = is_vertex ? 0 : (SCE_GXM_REAL_MAX_UNIFORM_BUFFER + 1);

        std::vector<uint8_t> my_data(data.get(mem), data.get(mem) + size);
        render_context->ubo_data[base_binding_ubo_relative + block_num] = std::move(my_data);
    }
}

COMMAND_SET_STATE(viewport) {
    TRACY_FUNC_COMMANDS_SET_STATE(viewport);
    const bool flat = helper.pop<bool>();
    render_context->record.viewport_flat = flat;

    const float previous_flip_y = render_context->record.viewport_flip[1];
    if (!flat) {
        // if we use msaa without downscaling the texture or the opposite, the surface size will differ from the expected
        // one by a factor of 2, one way or an other
        float factor = 1.0f;
        const bool has_msaa = (render_context->current_render_target && render_context->current_render_target->multisample_mode);
        const bool has_downscale = render_context->record.color_surface.downscale;
        if (has_msaa && !has_downscale)
            factor = 2.0f;
        else if (!has_msaa && has_downscale)
            factor = 1.0f;

        const float xOffset = helper.pop<float>() * factor;
        const float yOffset = helper.pop<float>() * factor;
        const float zOffset = helper.pop<float>();
        const float xScale = helper.pop<float>() * factor;
        const float yScale = helper.pop<float>() * factor;
        const float zScale = helper.pop<float>();

        const float ymin = yOffset + yScale;
        const float ymax = yOffset - yScale - 1;

        render_context->record.viewport_flip[0] = 1.0f;
        render_context->record.viewport_flip[1] = (ymin < ymax) ? -1.0f : 1.0f;
        render_context->record.viewport_flip[2] = 1.0f;
        render_context->record.viewport_flip[3] = 1.0f;
        render_context->record.z_offset = zOffset;
        render_context->record.z_scale = zScale;

        switch (renderer.current_backend) {
        case Backend::OpenGL:
            gl::sync_viewport_real(static_cast<gl::GLState &>(renderer), *reinterpret_cast<gl::GLContext *>(render_context), xOffset, yOffset, zOffset, xScale, yScale, zScale);
            break;

        case Backend::Vulkan:
            vulkan::sync_viewport_real(*reinterpret_cast<vulkan::VKContext *>(render_context), xOffset, yOffset, zOffset, xScale, yScale, zScale);
            break;

        default:
            REPORT_MISSING(renderer.current_backend);
            break;
        }
    } else {
        render_context->record.viewport_flip[0] = 1.0f;
        render_context->record.viewport_flip[1] = -1.0f;
        render_context->record.viewport_flip[2] = 1.0f;
        render_context->record.viewport_flip[3] = 1.0f;
        render_context->record.z_offset = 0.0f;
        render_context->record.z_scale = 1.0f;

        switch (renderer.current_backend) {
        case Backend::OpenGL:
            gl::sync_viewport_flat(static_cast<gl::GLState &>(renderer), *reinterpret_cast<gl::GLContext *>(render_context));
            break;

        case Backend::Vulkan:
            vulkan::sync_viewport_flat(*reinterpret_cast<vulkan::VKContext *>(render_context));
            break;

        default:
            REPORT_MISSING(renderer.current_backend);
            break;
        }
    }

    if (previous_flip_y != render_context->record.viewport_flip[1]) {
        switch (renderer.current_backend) {
        case Backend::OpenGL:
            // We need to sync again state that uses the flip
            gl::sync_cull(render_context->record);
            gl::sync_clipping(static_cast<gl::GLState &>(renderer), *reinterpret_cast<gl::GLContext *>(render_context));
            break;

        case Backend::Vulkan:
            // We need to sync again state that uses the flip
            vulkan::sync_clipping(*reinterpret_cast<vulkan::VKContext *>(render_context));
            break;

        default:
            REPORT_MISSING(renderer.current_backend);
            break;
        }
    }
}

COMMAND_SET_STATE(depth_bias) {
    TRACY_FUNC_COMMANDS_SET_STATE(depth_bias);
    const bool is_front = helper.pop<bool>();
    const int factor = helper.pop<int>();
    const int unit = helper.pop<int>();

    render_context->record.depth_bias_unit = unit;
    render_context->record.depth_bias_slope = factor;

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        if (is_front)
            gl::sync_depth_bias(factor, unit, is_front);
        break;

    case Backend::Vulkan:
        if (is_front)
            vulkan::sync_depth_bias(*reinterpret_cast<vulkan::VKContext *>(render_context));
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(depth_func) {
    TRACY_FUNC_COMMANDS_SET_STATE(depth_func);
    const bool is_front = helper.pop<bool>();
    const SceGxmDepthFunc depth_func = helper.pop<SceGxmDepthFunc>();

    if (is_front) {
        render_context->record.front_depth_func = depth_func;
    } else {
        render_context->record.back_depth_func = depth_func;
    }

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        gl::sync_depth_func(depth_func, is_front);
        break;

    case Backend::Vulkan:
        vulkan::refresh_pipeline(*reinterpret_cast<vulkan::VKContext *>(render_context));
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(depth_write_enable) {
    TRACY_FUNC_COMMANDS_SET_STATE(depth_write_enable);
    const bool is_front = helper.pop<bool>();
    const SceGxmDepthWriteMode mode = helper.pop<SceGxmDepthWriteMode>();

    if (is_front)
        render_context->record.front_depth_write_mode = mode;
    else
        render_context->record.back_depth_write_mode = mode;

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        gl::sync_depth_write_enable(mode, is_front);
        break;

    case Backend::Vulkan:
        vulkan::refresh_pipeline(*reinterpret_cast<vulkan::VKContext *>(render_context));
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(polygon_mode) {
    TRACY_FUNC_COMMANDS_SET_STATE(polygon_mode);
    const bool is_front = helper.pop<bool>();
    const SceGxmPolygonMode mode = helper.pop<SceGxmPolygonMode>();
    if (is_front)
        render_context->record.front_polygon_mode = mode;
    else
        render_context->record.back_polygon_mode = mode;

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        gl::sync_polygon_mode(mode, is_front);
        break;

    case Backend::Vulkan:
        vulkan::refresh_pipeline(*reinterpret_cast<vulkan::VKContext *>(render_context));
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(point_line_width) {
    TRACY_FUNC_COMMANDS_SET_STATE(point_line_width);
    const bool is_front = helper.pop<bool>();
    const std::uint32_t width = helper.pop<std::uint32_t>();
    if (is_front)
        render_context->record.line_width = width;

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        gl::sync_point_line_width(static_cast<gl::GLState &>(renderer), width, is_front);
        break;

    case Backend::Vulkan:
        vulkan::sync_point_line_width(*reinterpret_cast<vulkan::VKContext *>(render_context), is_front);
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(stencil_func) {
    TRACY_FUNC_COMMANDS_SET_STATE(stencil_func);
    // Is this the pain that driver guys have to suffer?
    const bool is_front = helper.pop<bool>();
    GxmStencilStateOp &stencil_state_op = is_front ? render_context->record.front_stencil_state_op : render_context->record.back_stencil_state_op;
    GxmStencilStateValues &stencil_state_vals = is_front ? render_context->record.front_stencil_state_values : render_context->record.back_stencil_state_values;

    stencil_state_op.func = helper.pop<SceGxmStencilFunc>();
    stencil_state_op.stencil_fail = helper.pop<SceGxmStencilOp>();
    stencil_state_op.depth_fail = helper.pop<SceGxmStencilOp>();
    stencil_state_op.depth_pass = helper.pop<SceGxmStencilOp>();
    stencil_state_vals.compare_mask = helper.pop<std::uint8_t>();
    stencil_state_vals.write_mask = helper.pop<std::uint8_t>();

    if (render_context->record.is_maskupdate) {
        if (stencil_state_op.func == SCE_GXM_STENCIL_FUNC_NEVER) {
            render_context->record.writing_mask = 0.0f;
        } else if (stencil_state_op.func == SCE_GXM_STENCIL_FUNC_ALWAYS) {
            render_context->record.writing_mask = 1.0f;
        } else {
            assert(false);
        }
        return;
    }

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        gl::sync_stencil_func(stencil_state_op, stencil_state_vals, mem, !is_front);
        break;

    case Backend::Vulkan:
        vulkan::refresh_pipeline(dynamic_cast<vulkan::VKContext &>(*render_context));
        vulkan::sync_stencil_func(dynamic_cast<vulkan::VKContext &>(*render_context), !is_front);
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(stencil_ref) {
    TRACY_FUNC_COMMANDS_SET_STATE(stencil_ref);
    const bool is_front = helper.pop<bool>();
    const uint8_t sref = helper.pop<const unsigned char>();

    GxmStencilStateOp &stencil_state_op = is_front ? render_context->record.front_stencil_state_op : render_context->record.back_stencil_state_op;
    GxmStencilStateValues &stencil_state_vals = is_front ? render_context->record.front_stencil_state_values : render_context->record.back_stencil_state_values;

    stencil_state_vals.ref = sref;

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        gl::sync_stencil_func(stencil_state_op, stencil_state_vals, mem, !is_front);
        break;

    case Backend::Vulkan:
        vulkan::sync_stencil_func(dynamic_cast<vulkan::VKContext &>(*render_context), !is_front);
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(texture) {
    TRACY_FUNC_COMMANDS_SET_STATE(texture);
    const std::uint32_t texture_index = helper.pop<std::uint32_t>();
    SceGxmTexture texture = helper.pop<SceGxmTexture>();

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        gl::sync_texture(dynamic_cast<gl::GLState &>(renderer), *reinterpret_cast<gl::GLContext *>(render_context), mem, texture_index, texture,
            config);
        break;

    case Backend::Vulkan:
        vulkan::sync_texture(*reinterpret_cast<vulkan::VKContext *>(render_context), mem, texture_index, texture,
            config);
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(two_sided) {
    TRACY_FUNC_COMMANDS_SET_STATE(two_sided);
    const SceGxmTwoSidedMode two_sided = helper.pop<SceGxmTwoSidedMode>();
    render_context->record.two_sided = two_sided;

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        // TODO: something should be done here
        break;

    case Backend::Vulkan:
        vulkan::refresh_pipeline(*reinterpret_cast<vulkan::VKContext *>(render_context));
        vulkan::sync_stencil_func(dynamic_cast<vulkan::VKContext &>(*render_context), false);
        // this second call is useless if two_sided is disabled
        vulkan::sync_stencil_func(dynamic_cast<vulkan::VKContext &>(*render_context), true);
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(cull_mode) {
    TRACY_FUNC_COMMANDS_SET_STATE(cull_mode);
    render_context->record.cull_mode = helper.pop<SceGxmCullMode>();

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        gl::sync_cull(render_context->record);
        break;

    case Backend::Vulkan:
        vulkan::refresh_pipeline(*reinterpret_cast<vulkan::VKContext *>(render_context));
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(vertex_stream) {
    TRACY_FUNC_COMMANDS_SET_STATE(vertex_stream);
    const Ptr<const uint8_t> stream_data = helper.pop<Ptr<const uint8_t>>();
    const std::size_t stream_index = helper.pop<std::size_t>();
    const std::size_t stream_data_length = helper.pop<std::size_t>();

    renderer::GXMStreamInfo &info = render_context->record.vertex_streams[stream_index];
    info.data = stream_data;
    info.size = stream_data_length;
}

COMMAND_SET_STATE(fragment_program_enable) {
    TRACY_FUNC_COMMANDS_SET_STATE(fragment_program_enable);
    const bool is_front = helper.pop<bool>();
    const SceGxmFragmentProgramMode mode = helper.pop<SceGxmFragmentProgramMode>();

    if (is_front)
        render_context->record.front_side_fragment_program_mode = mode;
    else
        render_context->record.back_side_fragment_program_mode = mode;

    if (renderer.current_backend == Backend::Vulkan) {
        vulkan::refresh_pipeline(*reinterpret_cast<vulkan::VKContext *>(render_context));
    }
}

COMMAND_SET_STATE(visibility_buffer) {
    TRACY_FUNC_COMMANDS_SET_STATE(visibility_buffer);
    const Ptr<uint32_t> buffer = helper.pop<Ptr<uint32_t>>();
    const uint32_t stride = helper.pop<uint32_t>();

    if (renderer.current_backend == Backend::Vulkan) {
        vulkan::sync_visibility_buffer(*reinterpret_cast<vulkan::VKContext *>(render_context), buffer, stride);
    }
}

COMMAND_SET_STATE(visibility_index) {
    TRACY_FUNC_COMMANDS_SET_STATE(visibility_index);
    const uint32_t index = helper.pop<uint32_t>();
    const bool enable = helper.pop<bool>();
    const bool is_increment = helper.pop<bool>();

    if (renderer.current_backend == Backend::Vulkan) {
        vulkan::sync_visibility_index(*reinterpret_cast<vulkan::VKContext *>(render_context), enable, index, is_increment);
    }
}

COMMAND(handle_set_state) {
    // TRACY_FUNC_COMMANDS(handle_set_state); All set state commands have tracy so kinda redundant
    renderer::GXMState gxm_state_to_set = helper.pop<renderer::GXMState>();
    using StateChangeHandlerFunc = decltype(cmd_set_state_region_clip);

    static const std::map<renderer::GXMState, StateChangeHandlerFunc *> handlers = {
        { GXMState::RegionClip, cmd_set_state_region_clip },
        { GXMState::Program, cmd_set_state_program },
        { GXMState::Viewport, cmd_set_state_viewport },
        { GXMState::DepthBias, cmd_set_state_depth_bias },
        { GXMState::DepthFunc, cmd_set_state_depth_func },
        { GXMState::DepthWriteEnable, cmd_set_state_depth_write_enable },
        { GXMState::PolygonMode, cmd_set_state_polygon_mode },
        { GXMState::PointLineWidth, cmd_set_state_point_line_width },
        { GXMState::StencilFunc, cmd_set_state_stencil_func },
        { GXMState::Texture, cmd_set_state_texture },
        { GXMState::StencilRef, cmd_set_state_stencil_ref },
        { GXMState::TwoSided, cmd_set_state_two_sided },
        { GXMState::CullMode, cmd_set_state_cull_mode },
        { GXMState::VertexStream, cmd_set_state_vertex_stream },
        { GXMState::UniformBuffer, cmd_set_state_uniform_buffer },
        { GXMState::FragmentProgramEnable, cmd_set_state_fragment_program_enable },
        { GXMState::VisibilityBuffer, cmd_set_state_visibility_buffer },
        { GXMState::VisibilityIndex, cmd_set_state_visibility_index }
    };

    auto result = handlers.find(gxm_state_to_set);

    if (result != handlers.end()) {
        // LOG_TRACE("State set: {}", (int)gxm_state_to_set);
        result->second(renderer, mem, config, helper, render_context);
    } else {
        LOG_ERROR("Unknown state set command {}", static_cast<uint16_t>(gxm_state_to_set));
    }
}
} // namespace renderer
