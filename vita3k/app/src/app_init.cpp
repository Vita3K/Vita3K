// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <app/functions.h>

#include <audio/state.h>
#include <config/functions.h>
#include <config/state.h>
#include <config/version.h>
#include <display/state.h>
#include <emuenv/state.h>
#include <gui/imgui_impl_sdl.h>
#include <io/functions.h>
#include <kernel/state.h>
#include <ngs/state.h>
#include <renderer/state.h>

#include <nids/functions.h>
#include <renderer/functions.h>
#include <rtc/rtc.h>
#include <util/fs.h>
#include <util/lock_and_find.h>
#include <util/log.h>
#include <util/string_utils.h>

#if USE_DISCORD
#include <app/discord.h>
#endif

#include <gdbstub/functions.h>

#include <renderer/vulkan/functions.h>
#include <util/string_utils.h>

#include <SDL_video.h>
#include <SDL_vulkan.h>

namespace app {
void update_viewport(EmuEnvState &state) {
    int w = 0;
    int h = 0;

    switch (state.renderer->current_backend) {
    case renderer::Backend::OpenGL:
        SDL_GL_GetDrawableSize(state.window.get(), &w, &h);
        break;

    case renderer::Backend::Vulkan:
        SDL_Vulkan_GetDrawableSize(state.window.get(), &w, &h);
        break;

    default:
        LOG_ERROR("Unimplemented backend render: {}.", static_cast<int>(state.renderer->current_backend));
        break;
    }

    state.drawable_size.x = w;
    state.drawable_size.y = h;

    if (h > 0) {
        const float window_aspect = static_cast<float>(w) / h;
        const float vita_aspect = static_cast<float>(DEFAULT_RES_WIDTH) / DEFAULT_RES_HEIGHT;
        if (window_aspect > vita_aspect) {
            // Window is wide. Pin top and bottom.
            state.viewport_size.x = h * vita_aspect;
            state.viewport_size.y = static_cast<SceFloat>(h);
            state.viewport_pos.x = (w - state.viewport_size.x) / 2;
            state.viewport_pos.y = 0;
        } else {
            // Window is tall. Pin left and right.
            state.viewport_size.x = static_cast<SceFloat>(w);
            state.viewport_size.y = w / vita_aspect;
            state.viewport_pos.x = 0;
            state.viewport_pos.y = (h - state.viewport_size.y) / 2;
        }
    } else {
        state.viewport_pos.x = 0;
        state.viewport_pos.y = 0;
        state.viewport_size.x = 0;
        state.viewport_size.y = 0;
    }
}

bool init(EmuEnvState &state, Config &cfg, const Root &root_paths) {
    const ResumeAudioThread resume_thread = [&state](SceUID thread_id) {
        const auto thread = lock_and_find(thread_id, state.kernel.threads, state.kernel.mutex);
        const std::lock_guard<std::mutex> lock(thread->mutex);
        if (thread->status == ThreadStatus::wait) {
            thread->update_status(ThreadStatus::run);
        }
    };

    state.cfg = std::move(cfg);

    state.base_path = root_paths.get_base_path_string();
    state.default_path = root_paths.get_pref_path_string();

    // If configuration does not provide a preference path, use SDL's default
    if (state.cfg.pref_path == root_paths.get_pref_path() || state.cfg.pref_path.empty())
        state.pref_path = string_utils::utf_to_wide(root_paths.get_pref_path_string());
    else {
        if (state.cfg.pref_path.back() != '/')
            state.cfg.pref_path += '/';
        state.pref_path = string_utils::utf_to_wide(state.cfg.pref_path);
    }

    state.backend_renderer = renderer::Backend::Vulkan;

    if (string_utils::toupper(state.cfg.backend_renderer) == "OPENGL") {
#ifndef __APPLE__
        state.backend_renderer = renderer::Backend::OpenGL;
#else
        state.cfg.backend_renderer = "Vulkan";
        config::serialize_config(state.cfg, state.cfg.config_path);
#endif
    }

    int window_type = 0;
    switch (state.backend_renderer) {
    case renderer::Backend::OpenGL:
        window_type = SDL_WINDOW_OPENGL;
        break;

    case renderer::Backend::Vulkan:
        window_type = SDL_WINDOW_VULKAN;
        break;

    default:
        LOG_ERROR("Unimplemented backend render: {}.", state.cfg.backend_renderer);
        break;
    }

    if (state.cfg.fullscreen) {
        state.display.fullscreen = true;
        window_type |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
#if defined(WIN32) || defined(__LINUX__)
    const auto isSteamDeck = []() {
#ifdef __LINUX__
        std::ifstream file("/etc/os-release");
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                if (line.find("VARIANT_ID=steamdeck") != std::string::npos)
                    return true;
            }
        }
#endif
        return false;
    };

    if (!isSteamDeck()) {
        float ddpi, hdpi, vdpi;
        SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi);
        window_type |= SDL_WINDOW_ALLOW_HIGHDPI;
        state.dpi_scale = ddpi / 96;
    }
#endif
    state.res_width_dpi_scale = static_cast<uint32_t>(DEFAULT_RES_WIDTH * state.dpi_scale);
    state.res_height_dpi_scale = static_cast<uint32_t>(DEFAULT_RES_HEIGHT * state.dpi_scale);
    state.window = WindowPtr(SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, state.res_width_dpi_scale, state.res_height_dpi_scale, window_type | SDL_WINDOW_RESIZABLE), SDL_DestroyWindow);

    if (!state.window) {
        LOG_ERROR("SDL failed to create window!");
        return false;
    }

    // initialize the renderer first because we need to know if we need a page table
    if (!state.cfg.console) {
        if (renderer::init(state.window.get(), state.renderer, state.backend_renderer, state.cfg, state.base_path.data())) {
            update_viewport(state);
        } else {
            switch (state.backend_renderer) {
            case renderer::Backend::OpenGL:
                error_dialog("Could not create OpenGL context!\nDoes your GPU at least support OpenGL 4.4?", nullptr);
                break;

            case renderer::Backend::Vulkan:
                error_dialog("Could not create Vulkan context!");
                break;

            default:
                error_dialog(fmt::format("Unknown backend render: {}.", state.cfg.backend_renderer));
                break;
            }
            return false;
        }
    }

    if (!init(state.mem, state.renderer->need_page_table)) {
        LOG_ERROR("Failed to initialize memory for emulator state!");
        return false;
    }

    if (state.mem.use_page_table && state.kernel.cpu_backend == CPUBackend::Unicorn)
        LOG_CRITICAL("Unicorn backend is not supported with a page table");

    if (!state.audio.init(resume_thread, state.cfg.audio_backend)) {
        LOG_WARN("Failed to init audio! Audio will not work.");
    }

    if (!init(state.io, state.base_path, state.pref_path, state.cfg.console)) {
        LOG_ERROR("Failed to initialize file system for the emulator!");
        return false;
    }

    if (!ngs::init(state.ngs, state.mem)) {
        LOG_ERROR("Failed to initialize ngs.");
        return false;
    }

#if USE_DISCORD
    if (discordrpc::init() && state.cfg.discord_rich_presence) {
        discordrpc::update_presence();
    }
#endif

    return true;
}

void destroy(EmuEnvState &emuenv, ImGui_State *imgui) {
    ImGui_ImplSdl_Shutdown(imgui);

#ifdef USE_DISCORD
    discordrpc::shutdown();
#endif

    if (emuenv.cfg.gdbstub)
        server_close(emuenv);

    // There may be changes that made in the GUI, so we should save, again
    if (emuenv.cfg.overwrite_config)
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
}

} // namespace app
