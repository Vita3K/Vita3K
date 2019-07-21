#include <gxm/types.h>
#include <renderer/state.h>
#include <renderer/types.h>
#include <renderer/commands.h>

#include "gl/functions.h"
#include "gl/types.h"
#include "driver_functions.h"

#include <renderer/functions.h>
#include <util/log.h>
#include <config/config.h>

namespace renderer {
    COMMAND(handle_set_context) {
        const RenderTarget *rt = helper.pop<const RenderTarget*>();
        const emu::SceGxmColorSurface *color_surface = helper.pop<emu::SceGxmColorSurface*>();
        const emu::SceGxmDepthStencilSurface *depth_stencil_surface = helper.pop<emu::SceGxmDepthStencilSurface*>();

        if (rt) {
            render_context->current_render_target = rt;
        }

        if (color_surface) {
            state->color_surface = *color_surface;
            delete color_surface;
        }
        
        if (depth_stencil_surface) {
            state->depth_stencil_surface = *depth_stencil_surface;
            delete depth_stencil_surface;
        }
        
        switch (renderer.current_backend) {
        case Backend::OpenGL: {
            gl::set_context(*reinterpret_cast<gl::GLContext*>(render_context), reinterpret_cast<const gl::GLRenderTarget*>(rt));
            break;
        }

        default:
            REPORT_MISSING(renderer.current_backend);
            break;
        }
    }

    COMMAND(handle_sync_surface_data) {
        const size_t width = state->color_surface.pbeEmitWords[0];
        const size_t height = state->color_surface.pbeEmitWords[1];
        const size_t stride_in_pixels = state->color_surface.pbeEmitWords[2];
        const Address data = state->color_surface.pbeEmitWords[3];
        uint32_t *const pixels = Ptr<uint32_t>(data).get(mem);

        switch (renderer.current_backend) {
        case Backend::OpenGL: {
            gl::get_surface_data(*reinterpret_cast<gl::GLContext*>(render_context), width, height, 
                stride_in_pixels, pixels);
            
            break;
        }

        default:
            REPORT_MISSING(renderer.current_backend);
            break;
        }
    }

    COMMAND(handle_draw) {
        SceGxmPrimitiveType type = helper.pop<SceGxmPrimitiveType>();
        SceGxmIndexFormat format = helper.pop<SceGxmIndexFormat>();
        const void *indicies = helper.pop<const void*>();
        const std::uint32_t count = helper.pop<const std::uint32_t>();
        
        switch (renderer.current_backend) {
        case Backend::OpenGL: {
            gl::draw(static_cast<gl::GLState&>(renderer),*reinterpret_cast<gl::GLContext*>(render_context),
                *state, type, format, indicies, count, mem, base_path, title_id, config.log_active_shaders, config.log_uniforms);

            break;
        }

        default: {
            REPORT_MISSING(renderer.current_backend);
            break;
        }
        }
    }
}