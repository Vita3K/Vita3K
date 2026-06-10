// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <renderer/functions.h>
#include <renderer/state.h>
#include <renderer/types.h>

#include <dialog/state.h>
#include <overlay/common_dialog.h>
#include <overlay/display_manager.h>
#include <overlay/font.h>
#include <overlay/pause_overlay.h>
#include <overlay/perf_overlay.h>
#include <overlay/shader_compile_notice.h>
#include <renderer/gl/state.h>
#include <renderer/gl/types.h>
#include <renderer/vulkan/functions.h>

#include <gxm/functions.h>
#include <util/log.h>

namespace renderer {

void State::update_overlays() {
    if (!overlay_manager)
        return;

    if (show_compile_shaders) {
        const auto now = std::chrono::steady_clock::now();
        const uint32_t newly_compiled = shaders_count_compiled;
        if (newly_compiled > 0) {
            m_shaders_compiled_count += newly_compiled;
            shaders_count_compiled = 0;
            m_shaders_compiled_time = now;

            auto notice = overlay_manager->get<overlay::shader_compile_notice>();
            if (!notice)
                notice = overlay_manager->create<overlay::shader_compile_notice>();
            notice->update_count(m_shaders_compiled_count, current_backend == Backend::Vulkan);
        } else if (m_shaders_compiled_count > 0) {
            auto notice = overlay_manager->get<overlay::shader_compile_notice>();
            if (notice && notice->should_hide()) {
                overlay_manager->remove<overlay::shader_compile_notice>();
                m_shaders_compiled_count = 0;
            }
        }
    }

    {
        const bool is_paused = paused.load(std::memory_order_relaxed);
        overlay_manager->set_paused(is_paused);
        if (is_paused) {
            if (!overlay_manager->get<overlay::pause_overlay>())
                overlay_manager->create<overlay::pause_overlay>();
        } else {
            if (overlay_manager->get<overlay::pause_overlay>())
                overlay_manager->remove<overlay::pause_overlay>();
        }
    }

    if (perf_overlay.enabled && perf_overlay.fps > 0) {
        auto perf = overlay_manager->get<overlay::perf_overlay>();
        if (!perf)
            perf = overlay_manager->create<overlay::perf_overlay>();

        perf->set_position(static_cast<overlay::screen_quadrant>(perf_overlay.position));
        perf->set_detail_level(static_cast<overlay::perf_detail_level>(perf_overlay.detail));
        perf->set_fps_data(perf_overlay.fps, perf_overlay.avg_fps, perf_overlay.min_fps,
            perf_overlay.max_fps, perf_overlay.ms_per_frame,
            perf_overlay.fps_values.data(), perf_overlay.fps_values_count,
            perf_overlay.current_fps_offset);
    } else {
        auto perf = overlay_manager->get<overlay::perf_overlay>();
        if (perf)
            overlay_manager->remove<overlay::perf_overlay>();
    }

    if (common_dialog) {
        auto dlg = overlay_manager->get<overlay::common_dialog_overlay>();
        if (common_dialog->type != NO_DIALOG && common_dialog->status == SCE_COMMON_DIALOG_STATUS_RUNNING) {
            bool just_created = false;
            if (!dlg) {
                dlg = overlay_manager->create<overlay::common_dialog_overlay>();
                just_created = true;
            }
            if (dlg->poll_dialog(*common_dialog, sys_date_format, sys_button)
                && common_dialog->type != TROPHY_SETUP_DIALOG) {
                if (just_created || dlg->input_loop_exited()) {
                    dlg->reset_input_loop();
                    overlay_manager->attach_thread_input("common_dialog", dlg);
                }
            }
        } else {
            if (dlg)
                overlay_manager->remove<overlay::common_dialog_overlay>();
        }
    }
}

void State::init_overlay_font_dirs() {
    overlay::fontmgr::set_system_lang(sys_lang);

    if (frame) {
        overlay::fontmgr::set_system_font_dirs(frame->font_dirs());
    }

    if (!vita_fs_path.empty()) {
        auto fw_dir = fs_utils::path_to_utf8(vita_fs_path / "sa0" / "data" / "font" / "pvf");
        if (!fw_dir.empty()) {
            if (fw_dir.back() != '/' && fw_dir.back() != '\\')
                fw_dir += '/';
            overlay::fontmgr::set_firmware_font_dir(fw_dir);
        }
    }

    {
        auto icons_dir = fs_utils::path_to_utf8(static_assets / "icons");
        if (!icons_dir.empty()) {
            if (icons_dir.back() != '/' && icons_dir.back() != '\\')
                icons_dir += '/';
            overlay::resource_config::set_icons_dir(icons_dir);
        }
    }

    LOG_INFO("Overlay font firmware dir: {}", overlay::fontmgr::get_firmware_font_dir().empty() ? "(none)" : overlay::fontmgr::get_firmware_font_dir());
    LOG_INFO("Overlay font system dirs: {} entries", overlay::fontmgr::get_system_font_dirs().size());
    for (const auto &d : overlay::fontmgr::get_system_font_dirs())
        LOG_DEBUG("  system font dir: {}", d);
}

void set_depth_bias(State &state, Context *ctx, bool is_front, int factor, int units) {
    renderer::add_state_set_command(ctx, renderer::GXMState::DepthBias, is_front, factor, units);
}

void set_depth_func(State &state, Context *ctx, bool is_front, SceGxmDepthFunc depth_func) {
    renderer::add_state_set_command(ctx, renderer::GXMState::DepthFunc, is_front, depth_func);
}

void set_depth_write_enable_mode(State &state, Context *ctx, bool is_front, SceGxmDepthWriteMode enable) {
    renderer::add_state_set_command(ctx, renderer::GXMState::DepthWriteEnable, is_front, enable);
}

void set_point_line_width(State &state, Context *ctx, bool is_front, unsigned int width) {
    renderer::add_state_set_command(ctx, renderer::GXMState::PointLineWidth, is_front, width);
}

void set_polygon_mode(State &state, Context *ctx, bool is_front, SceGxmPolygonMode mode) {
    renderer::add_state_set_command(ctx, renderer::GXMState::PolygonMode, is_front, mode);
}

void set_stencil_func(State &state, Context *ctx, bool is_front, SceGxmStencilFunc func, SceGxmStencilOp stencilFail, SceGxmStencilOp depthFail, SceGxmStencilOp depthPass, unsigned char compareMask, unsigned char writeMask) {
    renderer::add_state_set_command(ctx, renderer::GXMState::StencilFunc, is_front, func, stencilFail, depthFail, depthPass, compareMask, writeMask);
}

void set_stencil_ref(State &state, Context *ctx, bool is_front, unsigned char sref) {
    renderer::add_state_set_command(ctx, renderer::GXMState::StencilRef, is_front, sref);
}

void set_program(State &state, Context *ctx, Ptr<const void> program, const bool is_fragment) {
    renderer::add_state_set_command(ctx, renderer::GXMState::Program, program, is_fragment);
}

void set_cull_mode(State &state, Context *ctx, SceGxmCullMode cull) {
    renderer::add_state_set_command(ctx, renderer::GXMState::CullMode, cull);
}

void set_texture(State &state, Context *ctx, const std::uint32_t tex_index, const SceGxmTexture tex) {
    renderer::add_state_set_command(ctx, renderer::GXMState::Texture, tex_index, tex);
}

void set_viewport_real(State &state, Context *ctx, float xOffset, float yOffset, float zOffset, float xScale, float yScale, float zScale) {
    renderer::add_state_set_command(ctx, renderer::GXMState::Viewport, false, xOffset, yOffset,
        zOffset, xScale, yScale, zScale);
}

void set_viewport_flat(State &state, Context *ctx) {
    renderer::add_state_set_command(ctx, renderer::GXMState::Viewport, true);
}

void set_region_clip(State &state, Context *ctx, SceGxmRegionClipMode mode, unsigned int xMin, unsigned int xMax, unsigned int yMin, unsigned int yMax) {
    renderer::add_state_set_command(ctx, renderer::GXMState::RegionClip, mode, xMin, xMax, yMin, yMax);
}

void set_two_sided_enable(State &state, Context *ctx, SceGxmTwoSidedMode mode) {
    renderer::add_state_set_command(ctx, renderer::GXMState::TwoSided, mode);
}

void set_side_fragment_program_enable(State &state, Context *ctx, const bool is_front, SceGxmFragmentProgramMode mode) {
    renderer::add_state_set_command(ctx, renderer::GXMState::FragmentProgramEnable, is_front, mode);
}

void set_context(State &state, Context *ctx, RenderTarget *target, SceGxmColorSurface *color_surface, SceGxmDepthStencilSurface *depth_stencil_surface) {
    renderer::add_command(ctx, renderer::CommandOpcode::SetContext, nullptr, target, color_surface, depth_stencil_surface);
}

void set_vertex_stream(State &state, Context *ctx, const std::size_t index, const std::size_t data_len, const Ptr<const void> stream) {
    renderer::add_state_set_command(ctx, renderer::GXMState::VertexStream, stream, index, data_len);
}

void draw(State &state, Context *ctx, SceGxmPrimitiveType prim_type, SceGxmIndexFormat index_type, Ptr<const void> index_data, const std::uint32_t index_count, const std::uint32_t instance_count) {
    renderer::add_command(ctx, renderer::CommandOpcode::Draw, nullptr, prim_type, index_type, index_data, index_count, instance_count);
}

void transfer_copy(State &state, uint32_t colorKeyValue, uint32_t colorKeyMask, SceGxmTransferColorKeyMode colorKeyMode, const SceGxmTransferImage *images, SceGxmTransferType srcType, SceGxmTransferType destType) {
    renderer::send_single_command(state, nullptr, renderer::CommandOpcode::TransferCopy, false, colorKeyValue, colorKeyMask, colorKeyMode, images, srcType, destType);
}

void transfer_downscale(State &state, const SceGxmTransferImage *src, const SceGxmTransferImage *dest) {
    renderer::send_single_command(state, nullptr, renderer::CommandOpcode::TransferDownscale, false, src, dest);
}

void transfer_fill(State &state, uint32_t fillColor, const SceGxmTransferImage *dest) {
    renderer::send_single_command(state, nullptr, renderer::CommandOpcode::TransferFill, false, fillColor, dest);
}

void sync_surface_data(State &state, Context *ctx, const SceGxmNotification vertex_notification, const SceGxmNotification fragment_notification) {
    renderer::add_command(ctx, renderer::CommandOpcode::SyncSurfaceData, nullptr, vertex_notification, fragment_notification);
}

bool create_context(State &state, std::unique_ptr<Context> &context) {
    return renderer::send_single_command(state, nullptr, renderer::CommandOpcode::CreateContext, true, &context) > CommandErrorCodeNone;
}

void destroy_context(State &state, std::unique_ptr<Context> &context) {
    renderer::send_single_command(state, nullptr, renderer::CommandOpcode::DestroyContext, true, &context);
}

void destroy_context_during_shutdown(State &state, std::unique_ptr<Context> &context) {
    assert(!state.render_thread);

    if (state.current_backend == Backend::OpenGL) {
        state.set_current();
    }

    if (state.context == context.get()) {
        state.context = nullptr;
    }

    context.reset();
}

bool create_render_target(State &state, std::unique_ptr<RenderTarget> &rt, const SceGxmRenderTargetParams *params) {
    return renderer::send_single_command(state, nullptr, renderer::CommandOpcode::CreateRenderTarget, true, &rt, params) > CommandErrorCodeNone;
}

void destroy_render_target(State &state, std::unique_ptr<RenderTarget> &rt) {
    renderer::send_single_command(state, nullptr, renderer::CommandOpcode::DestroyRenderTarget, true, &rt);
}

void destroy_render_target_during_shutdown(State &state, std::unique_ptr<RenderTarget> &rt) {
    assert(!state.render_thread);
    if (!rt)
        return;

    switch (state.current_backend) {
    case Backend::OpenGL:
        state.set_current();
        break;

    case Backend::Vulkan:
        vulkan::destroy(dynamic_cast<vulkan::VKState &>(state), rt);
        break;
    }

    rt.reset();
}

void set_uniform_buffer(State &state, Context *ctx, const bool is_vertex_uniform, const int block_number, const std::uint16_t block_size, const Ptr<const void> buffer) {
    // Calculate the number of bytes
    std::uint32_t bytes_to_copy_and_pad = ((block_size + 15) / 16) * 16;

    renderer::add_state_set_command(ctx, renderer::GXMState::UniformBuffer, buffer, is_vertex_uniform, block_number, bytes_to_copy_and_pad);
}

void set_visibility_buffer(State &state, Context *ctx, Ptr<uint32_t> visibility_address, uint32_t visibility_stride) {
    renderer::add_state_set_command(ctx, renderer::GXMState::VisibilityBuffer, visibility_address, visibility_stride);
}

void set_visibility_index(State &state, Context *ctx, bool enable, uint32_t index, bool is_increment) {
    renderer::add_state_set_command(ctx, renderer::GXMState::VisibilityIndex, index, enable, is_increment);
}

} // namespace renderer
