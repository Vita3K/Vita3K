#include <gxm/types.h>
#include <renderer/commands.h>
#include <renderer/state.h>
#include <renderer/types.h>

#include "driver_functions.h"
#include "gl/functions.h"
#include "gl/types.h"
#include <util/align.h>
#include <util/log.h>

#include <config/config.h>

namespace renderer {
COMMAND_SET_STATE(region_clip) {
    state->region_clip_mode = helper.pop<SceGxmRegionClipMode>();
    const std::uint32_t xMin = helper.pop<std::uint32_t>();
    const std::uint32_t xMax = helper.pop<std::uint32_t>();
    const std::uint32_t yMin = helper.pop<std::uint32_t>();
    const std::uint32_t yMax = helper.pop<std::uint32_t>();

    state->region_clip_min.x = static_cast<SceInt>(align(xMin, SCE_GXM_TILE_SIZEX));
    state->region_clip_min.y = static_cast<SceInt>(align(yMin, SCE_GXM_TILE_SIZEY));
    state->region_clip_max.x = static_cast<SceInt>(align(xMax, SCE_GXM_TILE_SIZEX));
    state->region_clip_max.y = static_cast<SceInt>(align(yMax, SCE_GXM_TILE_SIZEY));

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        gl::sync_clipping(*state);
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
        state->fragment_program = program.cast<const SceGxmFragmentProgram>();

        switch (renderer.current_backend) {
        case Backend::OpenGL: {
            gl::sync_blending(*state, mem);
            break;
        }

        default:
            REPORT_MISSING(renderer.current_backend);
            break;
        }
    } else {
        state->vertex_program = program.cast<const SceGxmVertexProgram>();

        // Try to bind and layout vertex attributes
        switch (renderer.current_backend) {
        case Backend::OpenGL: {
            gl::sync_vertex_attributes(*reinterpret_cast<gl::GLContext *>(render_context), *state, mem);
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

COMMAND_SET_STATE(viewport) {
    state->viewport.enable = helper.pop<SceGxmViewportMode>();
    const bool set_viewport_param = helper.pop<bool>();

    if (set_viewport_param) {
        state->viewport.offset.x = helper.pop<float>();
        state->viewport.offset.y = helper.pop<float>();
        state->viewport.offset.z = helper.pop<float>();
        state->viewport.scale.x = helper.pop<float>();
        state->viewport.scale.y = helper.pop<float>();
        state->viewport.scale.z = helper.pop<float>();
    }

    // Sync
    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        gl::sync_viewport(*state);
        break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(depth_bias) {
    const bool is_front = helper.pop<bool>();

    if (is_front) {
        state->front_depth_bias_factor = helper.pop<int>();
        state->front_depth_bias_units = helper.pop<int>();
    } else {
        state->back_depth_bias_factor = helper.pop<int>();
        state->back_depth_bias_units = helper.pop<int>();
    }

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        if (is_front) {
            gl::sync_front_depth_bias(*state);
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

    if (is_front) {
        state->front_depth_func = helper.pop<SceGxmDepthFunc>();
    } else {
        state->back_depth_func = helper.pop<SceGxmDepthFunc>();
    }

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        if (is_front) {
            gl::sync_front_depth_func(*state);
        } else {
            //LOG_WARN("Unhandle set back depth func for OpenGL backend");
        }

        break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(depth_write_enable) {
    const bool is_front = helper.pop<bool>();

    if (is_front) {
        state->front_depth_write_enable = helper.pop<SceGxmDepthWriteMode>();
    } else {
        state->back_depth_write_enable = helper.pop<SceGxmDepthWriteMode>();
    }

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        if (is_front)
            gl::sync_front_depth_write_enable(*state);
        //else
        //LOG_WARN("Unhandle set back depth write enable for OpenGL backend");

        break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(polygon_mode) {
    const bool is_front = helper.pop<bool>();

    if (is_front) {
        state->front_polygon_mode = helper.pop<SceGxmPolygonMode>();
    } else {
        state->back_polygon_mode = helper.pop<SceGxmPolygonMode>();
    }

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        if (is_front)
            gl::sync_front_polygon_mode(*state);
        else
            // LOG_WARN("Unhandle set back polygon mode for OpenGL backend");

            break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(point_line_width) {
    const bool is_front = helper.pop<bool>();

    if (is_front)
        state->front_point_line_width = helper.pop<std::uint32_t>();
    else
        state->back_point_line_width = helper.pop<std::uint32_t>();

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        if (is_front)
            gl::sync_front_point_line_width(*state);
        else
            //LOG_WARN("Unhandle set back line width for OpenGL backend");

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

    GxmStencilState &stencil_state = is_front ? state->front_stencil : state->back_stencil;

    stencil_state.func = helper.pop<SceGxmStencilFunc>();
    stencil_state.stencil_fail = helper.pop<SceGxmStencilOp>();
    stencil_state.depth_fail = helper.pop<SceGxmStencilOp>();
    stencil_state.depth_pass = helper.pop<SceGxmStencilOp>();
    stencil_state.compare_mask = helper.pop<std::uint8_t>();
    stencil_state.write_mask = helper.pop<std::uint8_t>();

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        gl::sync_stencil_func(*state, !is_front);
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
        state->front_stencil.ref = sref;
    } else {
        state->back_stencil.ref = sref;
    }
}

COMMAND_SET_STATE(fragment_texture) {
    const std::uint32_t texture_index = helper.pop<std::uint32_t>();
    emu::SceGxmTexture texture = helper.pop<emu::SceGxmTexture>();

    state->fragment_textures[texture_index] = texture;
    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        gl::sync_texture(*reinterpret_cast<gl::GLContext *>(render_context), *state, mem, texture_index,
            config.texture_cache);

        break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(two_sided) {
    REPORT_STUBBED();

    const SceGxmTwoSidedMode two_sided = helper.pop<SceGxmTwoSidedMode>();
    state->two_sided = two_sided;
}

COMMAND_SET_STATE(cull_mode) {
    const SceGxmCullMode cull_mode = helper.pop<SceGxmCullMode>();
    state->cull_mode = cull_mode;

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        gl::sync_cull(*state);
        break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND_SET_STATE(vertex_stream) {
    const std::size_t stream_index = helper.pop<std::size_t>();
    const std::size_t stream_data_length = helper.pop<std::size_t>();
    const void *stream_data = helper.pop<const void *>();

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

    delete stream_data;
}

COMMAND(handle_set_state) {
    renderer::GXMState gxm_state_to_set = helper.pop<renderer::GXMState>();
    using StateChangeHandlerFunc = std::function<void(renderer::State &, MemState &, Config &, CommandHelper &,
        Context *, GxmContextState *)>;

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
        { renderer::GXMState::Uniform, cmd_set_state_uniform }
    };

    auto result = handlers.find(gxm_state_to_set);

    if (result != handlers.end()) {
        //LOG_TRACE("State set: {}", (int)gxm_state_to_set);
        result->second(renderer, mem, config, helper, render_context, state);
    }
}
} // namespace renderer