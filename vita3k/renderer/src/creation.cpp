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
#include <renderer/driver_functions.h>
#include <renderer/state.h>
#include <renderer/texture_cache_state.h>
#include <renderer/types.h>

#include <renderer/gl/functions.h>
#include <renderer/texture_cache_state.h>
#include <renderer/vulkan/functions.h>
#include <renderer/vulkan/state.h>

#include <gxm/functions.h>
#include <gxm/types.h>
#include <renderer/functions.h>
#include <util/log.h>
#include <util/string_utils.h>

namespace renderer {

static void layout_ssbo_offset_from_uniform_buffer_sizes(UniformBufferSizes &sizes, UniformBufferSizes &offsets, std::size_t &total_hold) {
    std::uint32_t last_offset = 0;

    for (std::size_t i = 0; i < sizes.size(); i++) {
        if (sizes[i] != 0) {
            // Round to vec4 unit
            offsets[i] = last_offset;
            last_offset += ((sizes[i] + 3) / 4 * 4);
        } else {
            offsets[i] = static_cast<std::uint32_t>(-1);
        }
    }

    total_hold = static_cast<std::size_t>(last_offset);
}

COMMAND(handle_create_context) {
    std::unique_ptr<Context> *ctx = helper.pop<std::unique_ptr<Context> *>();
    bool result = false;

    switch (renderer.current_backend) {
    case Backend::OpenGL: {
        result = gl::create(*ctx);
        break;
    }

    case Backend::Vulkan: {
        result = vulkan::create(dynamic_cast<vulkan::VKState &>(renderer), *ctx);
        break;
    }

    default: {
        REPORT_MISSING(renderer.current_backend);
        break;
    }
    }

    renderer.context = ctx->get();

    // fill with default values
    renderer.context->shader_hints = {
        .attributes = nullptr,
        .color_format = SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR
    };
    std::fill_n(renderer.context->shader_hints.vertex_textures, SCE_GXM_MAX_TEXTURE_UNITS, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);
    std::fill_n(renderer.context->shader_hints.fragment_textures, SCE_GXM_MAX_TEXTURE_UNITS, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);

    complete_command(renderer, helper, result);
}

COMMAND(handle_destroy_context) {
    std::unique_ptr<Context> *ctx = helper.pop<std::unique_ptr<Context> *>();
    ctx->reset();

    complete_command(renderer, helper, 0);
}

COMMAND(handle_create_render_target) {
    std::unique_ptr<RenderTarget> *render_target = helper.pop<std::unique_ptr<RenderTarget> *>();
    SceGxmRenderTargetParams *params = helper.pop<SceGxmRenderTargetParams *>();

    bool result = false;

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        result = gl::create(dynamic_cast<gl::GLState &>(renderer), *render_target, *params, features);
        break;

    case Backend::Vulkan:
        result = vulkan::create(dynamic_cast<vulkan::VKState &>(renderer), *render_target, *params, features);
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }
    complete_command(renderer, helper, result);
}

COMMAND(handle_destroy_render_target) {
    std::unique_ptr<RenderTarget> *render_target = helper.pop<std::unique_ptr<RenderTarget> *>();

    switch (renderer.current_backend) {
    case Backend::OpenGL:
        // nothing to do
        break;

    case Backend::Vulkan:
        vulkan::destroy(dynamic_cast<vulkan::VKState &>(renderer), *render_target);
        break;

    default:
        REPORT_MISSING(renderer.current_backend);
        break;
    }

    render_target->reset();

    complete_command(renderer, helper, 0);
}

COMMAND(handle_prepare_overall_buffer_storage) {
}

// Client
bool create(std::unique_ptr<FragmentProgram> &fp, State &state, const SceGxmProgram &program, const SceGxmBlendInfo *blend, GXPPtrMap &gxp_ptr_map, const char *base_path, const char *title_id) {
    switch (state.current_backend) {
    case Backend::OpenGL:
        gl::create(fp, dynamic_cast<gl::GLState &>(state), program, blend);
        break;

    case Backend::Vulkan:
        vulkan::create(fp, dynamic_cast<vulkan::VKState &>(state), program, blend);
        break;

    default:
        REPORT_MISSING(state.current_backend);
        return false;
    }

    // Try to hash this shader
    fp->hash = sha256(&program, program.size);
    gxp_ptr_map.emplace(fp->hash, &program);

    shader::usse::get_uniform_buffer_sizes(program, fp->uniform_buffer_sizes);
    layout_ssbo_offset_from_uniform_buffer_sizes(fp->uniform_buffer_sizes, fp->uniform_buffer_data_offsets, fp->max_total_uniform_buffer_storage);
    fp->texture_count = gxp::get_texture_count(program);

    return true;
}

bool create(std::unique_ptr<VertexProgram> &vp, State &state, const SceGxmProgram &program, GXPPtrMap &gxp_ptr_map, const char *base_path, const char *title_id) {
    switch (state.current_backend) {
    case Backend::OpenGL:
        gl::create(vp, dynamic_cast<gl::GLState &>(state), program);
        break;

    case Backend::Vulkan:
        vulkan::create(vp, dynamic_cast<vulkan::VKState &>(state), program);
        break;

    default:
        REPORT_MISSING(state.current_backend);
        return false;
    }

    // Hash this shader
    vp->hash = sha256(&program, program.size);
    gxp_ptr_map.emplace(vp->hash, &program);

    shader::usse::get_uniform_buffer_sizes(program, vp->uniform_buffer_sizes);
    shader::usse::get_attribute_informations(program, vp->attribute_infos);
    layout_ssbo_offset_from_uniform_buffer_sizes(vp->uniform_buffer_sizes, vp->uniform_buffer_data_offsets, vp->max_total_uniform_buffer_storage);
    vp->texture_count = gxp::get_texture_count(program);

    if (vp->attribute_infos.empty()) {
        vp->stripped_symbols_checked = false;
    } else {
        vp->stripped_symbols_checked = true;
    }

    return true;
}

void create(SceGxmSyncObject *sync, State &state) {
    // Set as if the last display was already done
    sync->last_display = 0;
    sync->timestamp_current = 0;
    sync->timestamp_ahead = 0;
    sync->extra = 0;

    if (state.current_backend == Backend::Vulkan)
        vulkan::create(sync);
}

void destroy(SceGxmSyncObject *sync, State &state) {
    if (state.current_backend == Backend::Vulkan)
        vulkan::destroy(sync);
}

bool init(SDL_Window *window, std::unique_ptr<State> &state, Backend backend, const Config &config, const char *base_path) {
    switch (backend) {
    case Backend::OpenGL:
        state = std::make_unique<gl::GLState>();
        if (!gl::create(window, state, base_path, config.hashless_texture_cache))
            return false;
        break;

    case Backend::Vulkan:
        state = std::make_unique<vulkan::VKState>(config.gpu_idx);
        if (!vulkan::create(window, state, base_path))
            return false;
        break;

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
