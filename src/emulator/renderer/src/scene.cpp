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

        if (rt) {
            render_context->current_render_target = rt;
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

    COMMAND(handle_get_surface_data) {
        const std::size_t width = helper.pop<std::size_t>();
        const std::size_t height = helper.pop<std::size_t>();
        const std::size_t stride_in_pixels = helper.pop<std::size_t>();
        std::uint32_t *data = helper.pop<std::uint32_t*>();

        switch (renderer.current_backend) {
        case Backend::OpenGL: {
            gl::get_surface_data(*reinterpret_cast<gl::GLContext*>(render_context), width, height, 
                stride_in_pixels, data);
            
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