#include <gxm/types.h>
#include <renderer/commands.h>
#include <renderer/state.h>
#include <renderer/types.h>

#include "driver_functions.h"
#include <renderer/gl/functions.h>
#include <renderer/gl/types.h>

#include <config/state.h>
#include <renderer/functions.h>
#include <util/log.h>

#define DEBUG_FRAMEBUFFER 1

#if DEBUG_FRAMEBUFFER
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#endif

namespace renderer {
COMMAND(handle_set_context) {
    const RenderTarget *rt = helper.pop<const RenderTarget *>();
    const SceGxmColorSurface *color_surface = helper.pop<SceGxmColorSurface *>();
    const SceGxmDepthStencilSurface *depth_stencil_surface = helper.pop<SceGxmDepthStencilSurface *>();

    if (rt) {
        render_context->current_render_target = rt;
    }

    if (color_surface && !color_surface->disabled) {
        state->color_surface = *color_surface;
        delete color_surface;
    } else {
        // Disable writing to this surface.
        // Data is still in render target though.
        state->color_surface.data = 0;
    }

    // Maybe we should disable writing to depth stencil too if it's null
    if (depth_stencil_surface) {
        state->depth_stencil_surface = *depth_stencil_surface;
        delete depth_stencil_surface;
    }

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        gl::set_context(*reinterpret_cast<gl::GLContext *>(render_context), *state, mem, reinterpret_cast<const gl::GLRenderTarget *>(rt), features);
        break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
}

COMMAND(handle_sync_surface_data) {
    const size_t width = state->color_surface.width;
    const size_t height = state->color_surface.height;
    const size_t stride_in_pixels = state->color_surface.strideInPixels;
    const Address data = state->color_surface.data.address();
    uint32_t *const pixels = Ptr<uint32_t>(data).get(mem);

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        gl::get_surface_data(*reinterpret_cast<gl::GLContext *>(render_context), width, height,
            stride_in_pixels, pixels, state->color_surface.colorFormat);

        break;
    }

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }

#if DEBUG_FRAMEBUFFER
    if (data != 0 && config.color_surface_debug) {
        const std::string filename = fmt::format("color_surface_0x{:X}.png", data);

        // Assuming output is RGBA
        const int result = stbi_write_png(filename.c_str(), width, height, 4, pixels, stride_in_pixels * 4);

        if (!result) {
            LOG_TRACE("Fail to save color surface 0x{:X}", data);
        }
    }
#endif
}

COMMAND(handle_draw) {
    SceGxmPrimitiveType type = helper.pop<SceGxmPrimitiveType>();
    SceGxmIndexFormat format = helper.pop<SceGxmIndexFormat>();
    const void *indicies = helper.pop<const void *>();
    const std::uint32_t count = helper.pop<const std::uint32_t>();

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        gl::draw(static_cast<gl::GLState &>(renderer), *reinterpret_cast<gl::GLContext *>(render_context),
            *state, features, type, format, indicies, count, mem, base_path, title_id, config);

        break;
    }

    default: {
        REPORT_MISSING(renderer.current_backend);
        break;
    }
    }
}
} // namespace renderer
