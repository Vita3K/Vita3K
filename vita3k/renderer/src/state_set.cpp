#include <gxm/types.h>
#include <renderer/commands.h>
#include <renderer/state.h>
#include <renderer/types.h>

#include <renderer/gl/functions.h>
#include <renderer/gl/types.h>

#include "driver_functions.h"

#include <util/align.h>
#include <util/log.h>

#include <config/state.h>

namespace renderer {
COMMAND_SET_STATE(region_clip) {
    render_context->record.region_clip_mode = helper.pop<SceGxmRegionClipMode>();
    const std::uint32_t xMin = helper.pop<std::uint32_t>();
    const std::uint32_t xMax = helper.pop<std::uint32_t>();
    const std::uint32_t yMin = helper.pop<std::uint32_t>();
    const std::uint32_t yMax = helper.pop<std::uint32_t>();

    render_context->record.region_clip_min.x = static_cast<SceInt>(align(xMin, SCE_GXM_TILE_SIZEX) - SCE_GXM_TILE_SIZEX);
    render_context->record.region_clip_min.y = static_cast<SceInt>(align(yMin, SCE_GXM_TILE_SIZEY) - SCE_GXM_TILE_SIZEY);
    render_context->record.region_clip_max.x = static_cast<SceInt>(align(xMax, SCE_GXM_TILE_SIZEX));
    render_context->record.region_clip_max.y = static_cast<SceInt>(align(yMax, SCE_GXM_TILE_SIZEY));

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        gl::sync_clipping(*reinterpret_cast<gl::GLContext *>(render_context));
        break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(program) {
    const Ptr<const void> program = helper.pop<Ptr<const void>>();
    const bool is_fragment = helper.pop<bool>();

    if (is_fragment) {
        render_context->record.fragment_program = program.cast<const SceGxmFragmentProgram>();

        switch (renderer.current_backend) {
        case Backend::OpenGL: {
            gl::sync_blending(render_context->record, mem);
            break;
        }

        default:
            REPORT_MISSING(renderer.current_backend);
            break;
        }
    } else {
        render_context->record.vertex_program = program.cast<const SceGxmVertexProgram>();

        // Try to bind and layout vertex attributes
        switch (renderer.current_backend) {
        case Backend::OpenGL: {
            gl::sync_vertex_attributes(*reinterpret_cast<gl::GLContext *>(render_context), render_context->record, mem);
            break;
        }

        default:
            REPORT_MISSING(renderer.current_backend);
            break;
        }
    }
}

COMMAND_SET_STATE(uniform) {
    const bool is_vertex = helper.pop<bool>();
    const SceGxmProgramParameter *parameter = helper.pop<SceGxmProgramParameter *>();
    const void *data = helper.pop<const void *>();

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        gl::UniformSetRequest request{ parameter, data };
        gl::GLContext *gl_context = reinterpret_cast<gl::GLContext *>(render_context);

        if (is_vertex) {
            gl_context->vertex_set_requests.push_back(std::move(request));
        } else {
            gl_context->fragment_set_requests.push_back(std::move(request));
        }

        break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(uniform_buffer) {
    std::uint8_t *data = helper.pop<std::uint8_t *>();
    const bool is_vertex = helper.pop<bool>();
    const int block_num = helper.pop<int>();
    const std::uint32_t size = helper.pop<std::uint32_t>();

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        gl::set_uniform_buffer(*reinterpret_cast<gl::GLContext *>(render_context), is_vertex, block_num, size, data, config.log_active_shaders);

        break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }

    delete[] data;
}

COMMAND_SET_STATE(viewport) {
    const bool flat = helper.pop<bool>();

    if (!flat) {
        const float xOffset = helper.pop<float>();
        const float yOffset = helper.pop<float>();
        const float zOffset = helper.pop<float>();
        const float xScale = helper.pop<float>();
        const float yScale = helper.pop<float>();
        const float zScale = helper.pop<float>();

        switch (renderer.current_backend) {
        case Backend::OpenGL:
            gl::sync_viewport_real(*reinterpret_cast<gl::GLContext *>(render_context), xOffset, yOffset, zOffset, xScale, yScale, zScale);
            break;

        default:
            REPORT_MISSING(renderer.current_backend);
            break;
        }
    } else {
        switch (renderer.current_backend) {
        case Backend::OpenGL:
            gl::sync_viewport_flat(*reinterpret_cast<gl::GLContext *>(render_context));
            break;

        default:
            REPORT_MISSING(renderer.current_backend);
            break;
        }
    }
}

COMMAND_SET_STATE(depth_bias) {
    const bool is_front = helper.pop<bool>();
    const int factor = helper.pop<int>();
    const int unit = helper.pop<int>();

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        if (is_front) {
            gl::sync_depth_bias(factor, unit, is_front);
        } else {
            // LOG_INFO("AAAA");
        }

        break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(depth_func) {
    const bool is_front = helper.pop<bool>();
    const SceGxmDepthFunc depth_func = helper.pop<SceGxmDepthFunc>();

    if (is_front) {
        render_context->record.front_depth_func = depth_func;
    } else {
        render_context->record.back_depth_func = depth_func;
    }

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        gl::sync_depth_func(depth_func, is_front);
        break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(depth_write_enable) {
    const bool is_front = helper.pop<bool>();
    const SceGxmDepthWriteMode mode = helper.pop<SceGxmDepthWriteMode>();

    if (is_front)
        render_context->record.front_depth_write_mode = mode;
    else
        render_context->record.back_depth_write_mode = mode;

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        gl::sync_depth_write_enable(mode, is_front);
        break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(polygon_mode) {
    const bool is_front = helper.pop<bool>();
    const SceGxmPolygonMode mode = helper.pop<SceGxmPolygonMode>();

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        gl::sync_polygon_mode(mode, is_front);
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(point_line_width) {
    const bool is_front = helper.pop<bool>();
    const std::uint32_t width = helper.pop<std::uint32_t>();

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        gl::sync_point_line_width(width, is_front);
        break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(stencil_func) {
    // Is this the pain that driver guys have to suffer?
    const bool is_front = helper.pop<bool>();
    GxmStencilState stencil_state;

    stencil_state.func = helper.pop<SceGxmStencilFunc>();
    stencil_state.stencil_fail = helper.pop<SceGxmStencilOp>();
    stencil_state.depth_fail = helper.pop<SceGxmStencilOp>();
    stencil_state.depth_pass = helper.pop<SceGxmStencilOp>();
    stencil_state.compare_mask = helper.pop<std::uint8_t>();
    stencil_state.write_mask = helper.pop<std::uint8_t>();

    if (is_front) {
        render_context->record.front_stencil_state = stencil_state;
    } else {
        render_context->record.back_stencil_state = stencil_state;
    }

    if (render_context->record.fragment_program && render_context->record.fragment_program.get(mem)->is_maskupdate) {
        if (stencil_state.func == SCE_GXM_STENCIL_FUNC_NEVER) {
            render_context->record.writing_mask = false;
        } else if (stencil_state.func == SCE_GXM_STENCIL_FUNC_ALWAYS) {
            render_context->record.writing_mask = true;
        } else {
            assert(false);
        }
        return;
    }

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        gl::sync_stencil_func(stencil_state, mem, !is_front);
        break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(stencil_ref) {
    REPORT_STUBBED();

    const bool is_front = helper.pop<bool>();
    const unsigned char sref = helper.pop<const unsigned char>();

    if (is_front) {
        render_context->record.front_stencil_state.ref = sref;
    } else {
        render_context->record.back_stencil_state.ref = sref;
    }
}

COMMAND_SET_STATE(fragment_texture) {
    const std::uint32_t texture_index = helper.pop<std::uint32_t>();
    SceGxmTexture texture = helper.pop<SceGxmTexture>();

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        gl::sync_texture(*reinterpret_cast<gl::GLContext *>(render_context), mem, texture_index, texture,
            config, base_path, title_id);
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(two_sided) {
    const SceGxmTwoSidedMode two_sided = helper.pop<SceGxmTwoSidedMode>();
    render_context->record.two_sided = two_sided;
}

COMMAND_SET_STATE(cull_mode) {
    render_context->record.cull_mode = helper.pop<SceGxmCullMode>();

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        gl::sync_cull(render_context->record);
        break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(vertex_stream) {
    std::uint8_t *stream_data = helper.pop<std::uint8_t *>();
    const std::size_t stream_index = helper.pop<std::size_t>();
    const std::size_t stream_data_length = helper.pop<std::size_t>();

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        gl::upload_vertex_stream(*reinterpret_cast<gl::GLContext *>(render_context), stream_index, stream_data_length,
            stream_data);

        break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }

    delete[] stream_data;
}

COMMAND_SET_STATE(fragment_program_enable) {
    const bool is_front = helper.pop<bool>();
    const SceGxmFragmentProgramMode mode = helper.pop<SceGxmFragmentProgramMode>();

    if (is_front)
        render_context->record.front_side_fragment_program_mode = mode;
    else
        render_context->record.back_side_fragment_program_mode = mode;
}

COMMAND(handle_set_state) {
    renderer::GXMState gxm_state_to_set = helper.pop<renderer::GXMState>();
    using StateChangeHandlerFunc = std::function<void(renderer::State &, MemState &, Config &, CommandHelper &,
        Context *, const char *base_path, const char *title_id)>;

    static const std::map<renderer::GXMState, StateChangeHandlerFunc> handlers = {
        { renderer::GXMState::RegionClip, cmd_set_state_region_clip },
        { renderer::GXMState::Program, cmd_set_state_program },
        { renderer::GXMState::Viewport, cmd_set_state_viewport },
        { renderer::GXMState::DepthBias, cmd_set_state_depth_bias },
        { renderer::GXMState::DepthFunc, cmd_set_state_depth_func },
        { renderer::GXMState::DepthWriteEnable, cmd_set_state_depth_write_enable },
        { renderer::GXMState::PolygonMode, cmd_set_state_polygon_mode },
        { renderer::GXMState::PointLineWidth, cmd_set_state_point_line_width },
        { renderer::GXMState::StencilFunc, cmd_set_state_stencil_func },
        { renderer::GXMState::FragmentTexture, cmd_set_state_fragment_texture },
        { renderer::GXMState::StencilRef, cmd_set_state_stencil_ref },
        { renderer::GXMState::TwoSided, cmd_set_state_two_sided },
        { renderer::GXMState::CullMode, cmd_set_state_cull_mode },
        { renderer::GXMState::VertexStream, cmd_set_state_vertex_stream },
        { renderer::GXMState::Uniform, cmd_set_state_uniform },
        { renderer::GXMState::UniformBuffer, cmd_set_state_uniform_buffer },
        { renderer::GXMState::FragmentProgramEnable, cmd_set_state_fragment_program_enable }
    };

    auto result = handlers.find(gxm_state_to_set);

    if (result != handlers.end()) {
        //LOG_TRACE("State set: {}", (int)gxm_state_to_set);
        result->second(renderer, mem, config, helper, render_context, base_path, title_id);
    }
}
} // namespace renderer
