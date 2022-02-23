// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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
#include <renderer/state.h>
#include <renderer/texture_cache_state.h>
#include <renderer/types.h>

#include <renderer/gl/functions.h>
#ifdef USE_VULKAN
#include <renderer/vulkan/functions.h>
#endif
#include <renderer/texture_cache_state.h>

#include "driver_functions.h"

#include <gxm/types.h>
#include <renderer/functions.h>
#include <util/log.h>
#include <util/string_utils.h>

namespace renderer {
COMMAND(handle_create_context) {
    std::unique_ptr<Context> *ctx = helper.pop<std::unique_ptr<Context> *>();
    bool result = false;

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        result = gl::create(*ctx);
        break;
    }

    default: {
        REPORT_MISSING(renderer.current_backend);
        break;
    }
    }

    complete_command(renderer, helper, result);
}

COMMAND(handle_create_render_target) {
    std::unique_ptr<RenderTarget> *render_target = helper.pop<std::unique_ptr<RenderTarget> *>();
    SceGxmRenderTargetParams *params = helper.pop<SceGxmRenderTargetParams *>();

    bool result = false;

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        result = gl::create(*render_target, *params, features);
        break;
    }

    default: {
        REPORT_MISSING(renderer.current_backend);
        break;
    }
    }

    complete_command(renderer, helper, result);
}

COMMAND(handle_destroy_render_target) {
    std::unique_ptr<RenderTarget> *render_target = helper.pop<std::unique_ptr<RenderTarget> *>();
    render_target->reset();

    complete_command(renderer, helper, 0);
}

COMMAND(handle_prepare_overall_buffer_storage) {
}

// Client
bool create(std::unique_ptr<FragmentProgram> &fp, State &state, const SceGxmProgram &program, const SceGxmBlendInfo *blend, GXPPtrMap &gxp_ptr_map, const char *base_path, const char *title_id) {
    switch (state.current_backend) {
    case Backend::OpenGL: {
        return gl::create(fp, static_cast<gl::GLState &>(state), program, blend, gxp_ptr_map, base_path, title_id);
    }

    default: {
        REPORT_MISSING(state.current_backend);
        break;
    }
    }

    return false;
}

bool create(std::unique_ptr<VertexProgram> &vp, State &state, const SceGxmProgram &program, GXPPtrMap &gxp_ptr_map, const char *base_path, const char *title_id) {
    switch (state.current_backend) {
    case Backend::OpenGL: {
        return gl::create(vp, static_cast<gl::GLState &>(state), program, gxp_ptr_map, base_path, title_id);
    }

    default: {
        REPORT_MISSING(state.current_backend);
        break;
    }
    }

    return false;
}

bool init(SDL_Window *window, std::unique_ptr<State> &state, Backend backend, const Config &config, const char *base_path) {
    switch (backend) {
    case Backend::OpenGL:
        state = std::make_unique<gl::GLState>();
        if (!gl::create(window, state, base_path, config.hashless_taexture_cache))
            return false;
        break;
#ifdef USE_VULKAN
    case Backend::Vulkan:
        state = std::make_unique<vulkan::VulkanState>();
        if (!vulkan::create(window, state))
            return false;
        break;
#endif
    default:
        LOG_ERROR("Cannot create a renderer with unsupported backend {}.", static_cast<int>(backend));
        return false;
    }

    state->current_backend = backend;

    // Can change this
    state->command_buffer_queue.maxPendingCount_ = 30;

    return true;
}
} // namespace renderer
