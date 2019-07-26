#include <renderer/functions.h>
#include <renderer/state.h>
#include <renderer/types.h>

namespace renderer {
/**
     * NOTE: If your backend doesn't use command queue, you can directly call it by adding a switch case and call that
     * function.
     * 
     * The switch is reserved for backend like Vulkan, when building a command buffer directly is possible.
     */

void set_depth_bias(State &state, Context *ctx, GxmContextState *gxm_context, bool is_front, int factor, int units) {
    switch (state.current_backend) {
    default:
        renderer::add_state_set_command(ctx, renderer::GXMState::DepthBias, is_front, factor, units);
        break;
    }
}

void set_depth_func(State &state, Context *ctx, GxmContextState *gxm_context, bool is_front, SceGxmDepthFunc depth_func) {
    switch (state.current_backend) {
    default:
        renderer::add_state_set_command(ctx, renderer::GXMState::DepthFunc, is_front, depth_func);
        break;
    }
}

void set_depth_write_enable_mode(State &state, Context *ctx, GxmContextState *gxm_context, bool is_front, SceGxmDepthWriteMode enable) {
    switch (state.current_backend) {
    default:
        renderer::add_state_set_command(ctx, renderer::GXMState::DepthWriteEnable, is_front, enable);
        break;
    }
}

void set_point_line_width(State &state, Context *ctx, GxmContextState *gxm_context, bool is_front, unsigned int width) {
    switch (state.current_backend) {
    default:
        renderer::add_state_set_command(ctx, renderer::GXMState::PointLineWidth, is_front, width);
        break;
    }
}

void set_polygon_mode(State &state, Context *ctx, GxmContextState *gxm_context, bool is_front, SceGxmPolygonMode mode) {
    switch (state.current_backend) {
    default:
        renderer::add_state_set_command(ctx, renderer::GXMState::PolygonMode, is_front, mode);
    }
}

void set_stencil_func(State &state, Context *ctx, GxmContextState *gxm_context, bool is_front, SceGxmStencilFunc func, SceGxmStencilOp stencilFail, SceGxmStencilOp depthFail, SceGxmStencilOp depthPass, unsigned char compareMask, unsigned char writeMask) {
    switch (state.current_backend) {
    default:
        renderer::add_state_set_command(ctx, renderer::GXMState::StencilFunc, is_front, func, stencilFail, depthFail, depthPass, compareMask, writeMask);
        break;
    }
}

void set_stencil_ref(State &state, Context *ctx, GxmContextState *gxm_context, bool is_front, unsigned char sref) {
    switch (state.current_backend) {
    default:
        renderer::add_state_set_command(ctx, renderer::GXMState::StencilRef, is_front, sref);
        break;
    }
}

void set_program(State &state, Context *ctx, GxmContextState *gxm_context, Ptr<const void> program, const bool is_fragment) {
    switch (state.current_backend) {
    default:
        renderer::add_state_set_command(ctx, renderer::GXMState::Program, program, is_fragment);
        break;
    }
}

void set_cull_mode(State &state, Context *ctx, GxmContextState *gxm_context, SceGxmCullMode cull) {
    switch (state.current_backend) {
    default:
        renderer::add_state_set_command(ctx, renderer::GXMState::CullMode, cull);
        break;
    }
}

void set_fragment_texture(State &state, Context *ctx, GxmContextState *gxm_context, const std::uint32_t tex_index, const emu::SceGxmTexture tex) {
    switch (state.current_backend) {
    default:
        renderer::add_state_set_command(ctx, renderer::GXMState::FragmentTexture, tex_index, tex);
        break;
    }
}

void set_viewport(State &state, Context *ctx, GxmContextState *gxm_context, float xOffset, float yOffset, float zOffset, float xScale, float yScale, float zScale) {
    switch (state.current_backend) {
    default:
        renderer::add_state_set_command(ctx, renderer::GXMState::Viewport, SCE_GXM_VIEWPORT_ENABLED, true, xOffset, yOffset,
            zOffset, xScale, yScale, zScale);

        break;
    }
}

void set_viewport_enable(State &state, Context *ctx, GxmContextState *gxm_context, SceGxmViewportMode enable) {
    switch (state.current_backend) {
    default:
        renderer::add_state_set_command(ctx, renderer::GXMState::Viewport, enable, false);
        break;
    }
}

void set_region_clip(State &state, Context *ctx, GxmContextState *gxm_context, SceGxmRegionClipMode mode, unsigned int xMin, unsigned int xMax, unsigned int yMin, unsigned int yMax) {
    switch (state.current_backend) {
    default:
        renderer::add_state_set_command(ctx, renderer::GXMState::RegionClip, mode, xMin, xMax, yMin, yMax);
        break;
    }
}

void set_two_sided_enable(State &state, Context *ctx, GxmContextState *gxm_context, SceGxmTwoSidedMode mode) {
    switch (state.current_backend) {
    default:
        renderer::add_state_set_command(ctx, renderer::GXMState::TwoSided, mode);
        break;
    }
}

void set_context(State &state, Context *ctx, GxmContextState *gxm_context, RenderTarget *target, emu::SceGxmColorSurface *color_surface, emu::SceGxmDepthStencilSurface *depth_stencil_surface) {
    switch (state.current_backend) {
    default:
        renderer::add_command(ctx, renderer::CommandOpcode::SetContext, nullptr, target, color_surface, depth_stencil_surface);
        break;
    }
}

void set_vertex_stream(State &state, Context *ctx, GxmContextState *gxm_context, const std::size_t index, const std::size_t data_len, const void *data) {
    switch (state.current_backend) {
    default:
        renderer::add_state_set_command(ctx, renderer::GXMState::VertexStream, index, data_len, data);
        break;
    }
}

void draw(State &state, Context *ctx, GxmContextState *gxm_context, SceGxmPrimitiveType prim_type, SceGxmIndexFormat index_type, const void *index_data, const std::uint32_t index_count) {
    switch (state.current_backend) {
    default:
        renderer::add_command(ctx, renderer::CommandOpcode::Draw, nullptr, prim_type, index_type, index_data, index_count);
        break;
    }
}

void sync_surface_data(State &state, Context *ctx, GxmContextState *gxm_context) {
    switch (state.current_backend) {
    default:
        renderer::add_command(ctx, renderer::CommandOpcode::SyncSurfaceData, nullptr);
        break;
    }
}

bool create_context(State &state, std::unique_ptr<Context> &context) {
    switch (state.current_backend) {
    default:
        return renderer::send_single_command(state, nullptr, nullptr, renderer::CommandOpcode::CreateContext, &context);
    }
}

bool create_render_target(State &state, std::unique_ptr<RenderTarget> &rt, const SceGxmRenderTargetParams *params) {
    switch (state.current_backend) {
    default:
        return renderer::send_single_command(state, nullptr, nullptr, renderer::CommandOpcode::CreateRenderTarget, &rt, params);
    }
}

void destroy_render_target(State &state, std::unique_ptr<RenderTarget> &rt) {
    switch (state.current_backend) {
    default:
        renderer::send_single_command(state, nullptr, nullptr, renderer::CommandOpcode::DestroyRenderTarget, &rt);
        break;
    }
}

void set_uniform(State &state, Context *ctx, const bool is_vertex_uniform, const SceGxmProgramParameter *parameter, const void *data) {
    switch (state.current_backend) {
    default:
        renderer::add_state_set_command(ctx, renderer::GXMState::Uniform, is_vertex_uniform, parameter, data);
        break;
    }
}
} // namespace renderer