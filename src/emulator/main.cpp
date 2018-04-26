// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include "vpk.h"

#include <host/functions.h>
#include <host/state.h>
#include <host/version.h>
#include <kernel/thread_functions.h>
#include <kernel/thread_state.h>
#include <util/find.h>
#include <util/log.h>
#include <util/resource.h>
#include <util/string_convert.h>

#include <SDL.h>
#include <glutil/gl.h>

#include <algorithm> // find_if_not
#include <cassert>
#include <iostream>
#include <sstream>

// clang-format off
#include <imgui.h>
#include <gui/imgui_impl_sdl_gl2.h>
// clang-format on
#include <gui/functions.h>

typedef std::unique_ptr<const void, void (*)(const void *)> SDLPtr;
typedef std::unique_ptr<SDL_Surface, void (*)(SDL_Surface *)> SurfacePtr;

enum ExitCode {
    Success = 0,
    IncorrectArgs,
    SDLInitFailed,
    HostInitFailed,
    ModuleLoadFailed,
    InitThreadFailed,
    RunThreadFailed
};

static bool is_macos_process_arg(const char *arg) {
    return strncmp(arg, "-psn_", 5) == 0;
}

static void error(const std::string &message, SDL_Window *window = nullptr) {
    if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message.c_str(), window) < 0) {
        LOG_ERROR("SDL Error: {}", message);
    }
}

static void term_sdl(const void *succeeded) {
    assert(succeeded != nullptr);

    SDL_Quit();
}

static constexpr auto DEFAULT_RES_WIDTH = 960;
static constexpr auto DEFAULT_RES_HEIGHT = 544;
static constexpr auto WINDOW_BORDER_WIDTH = 16;
static constexpr auto WINDOW_BORDER_HEIGHT = 34;

int main(int argc, char *argv[]) {
    init_logging();

    LOG_INFO("{}", window_title);

    ProgramArgsWide argv_wide = process_args(argc, argv);

    const SDLPtr sdl(reinterpret_cast<const void *>(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_VIDEO) >= 0), term_sdl);
    if (!sdl) {
        error("SDL initialisation failed.");
        return SDLInitFailed;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    const char *const *const path_arg = std::find_if_not(&argv[1], &argv[argc], is_macos_process_arg);
    std::wstring path;
    if (path_arg != &argv[argc]) {
        path = utf_to_wide(*path_arg);
    } else {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_DROPFILE) {
                path = utf_to_wide(ev.drop.file);
                SDL_free(ev.drop.file);
                break;
            }
        }
    }

    if (path.empty()) {
        std::string message = "Usage: ";
        message += argv[0];
        message += " <path to VPK file>";
        error(message);
        return IncorrectArgs;
    }

    HostState host;
    if (!init(host, DEFAULT_RES_WIDTH, WINDOW_BORDER_WIDTH, DEFAULT_RES_HEIGHT, WINDOW_BORDER_HEIGHT)) {
        error("Host initialisation failed.", host.window.get());
        return HostInitFailed;
    }

    ImGui::CreateContext();
    ImGui_ImplSdlGL2_Init(host.window.get());
    ImGui::StyleColorsDark();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    Ptr<const void> entry_point;
    if (!load_vpk(entry_point, host.game_title, host.title_id, host.kernel, host.io, host.mem, host.sfo_handle, path)) {
        std::string message = "Failed to load \"";
        message += wide_to_utf(path);
        message += "\"";
        message += "\nSee console output for details.";
        error(message.c_str(), host.window.get());
        return ModuleLoadFailed;
    }

    const CallImport call_import = [&host](uint32_t nid, SceUID main_thread_id) {
        ::call_import(host, nid, main_thread_id);
    };
        
    const SceUID main_thread_id = create_thread(entry_point, host.kernel, host.mem, host.title_id.c_str(), SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_STACK_SIZE_USER_MAIN, call_import, false);
    if (main_thread_id < 0) {
        error("Failed to init main thread.", host.window.get());
        return InitThreadFailed;
    }

    const ThreadStatePtr main_thread = find(main_thread_id, host.kernel.threads);
    Ptr<void> argp = Ptr<void>();
    if (!strncmp(host.kernel.loaded_modules.begin()->second->module_name, "SceLibc", 7)) {
        const SceUID libc_thread_id = create_thread(host.kernel.loaded_modules.begin()->second->module_start, host.kernel, host.mem, "libc", SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_STACK_SIZE_USER_DEFAULT, call_import, false);
        const ThreadStatePtr libc_thread = find(libc_thread_id, host.kernel.threads);
        run_on_current(*libc_thread, host.kernel.loaded_modules.begin()->second->module_start, 0, argp);
    }

    if (start_thread(host.kernel, main_thread_id, 0, Ptr<void>()) < 0) {
        error("Failed to run main thread.", host.window.get());
        return RunThreadFailed;
    }

    GLuint TextureID = 0;
    host.t1 = SDL_GetTicks();
    while (handle_events(host)) {
        if (!TextureID) {
            glGenTextures(1, &TextureID);
            glClearColor(1.0, 0.0, 0.5, 1.0);
            glClearDepth(1.0f);
            glViewport(0, 0, DEFAULT_RES_WIDTH, DEFAULT_RES_HEIGHT);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0, DEFAULT_RES_WIDTH, DEFAULT_RES_HEIGHT, 0, 1, -1);
            glMatrixMode(GL_MODELVIEW);
            glEnable(GL_TEXTURE_2D);
            glLoadIdentity();
        }

        ImGui_ImplSdlGL2_NewFrame(host.window.get());

        // Clear back buffer

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glLoadIdentity();

        {
            if (host.display.width > 0) {
                glBindTexture(GL_TEXTURE_2D, TextureID);
                void *const pixels = host.display.base.cast<void>().get(host.mem);

                glPixelStorei(GL_UNPACK_ROW_LENGTH, host.display.pitch);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, host.display.width, host.display.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                ImGui::SetNextWindowPos(ImVec2(0, 19), ImGuiSetCond_Always);
                ImGui::SetNextWindowSize(ImVec2(DEFAULT_RES_WIDTH + WINDOW_BORDER_WIDTH, DEFAULT_RES_HEIGHT + WINDOW_BORDER_HEIGHT), ImGuiSetCond_Always);
                ImGui::Begin("", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
                host.gui.renderer_focused = ImGui::IsWindowFocused();
                ImGui::Image(reinterpret_cast<void *>(TextureID), ImVec2(DEFAULT_RES_WIDTH, DEFAULT_RES_HEIGHT));
                ImGui::End();
            }
        }

        DrawUI(host);
        DrawCommonDialog(host);

        glViewport(0, 0, static_cast<int>(ImGui::GetIO().DisplaySize.x), static_cast<int>(ImGui::GetIO().DisplaySize.y));
        ImGui::Render();
        ImGui_ImplSdlGL2_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(host.window.get());

        {
            host.display.condvar.notify_all();
        }

        const uint32_t t2 = SDL_GetTicks();
        const uint32_t ms = t2 - host.t1;
        if (ms >= 1000 && host.frame_count > 0) {
            const uint32_t fps = (host.frame_count * 1000) / ms;
            const uint32_t ms_per_frame = ms / host.frame_count;
            std::ostringstream title;
            title << window_title << " | " << host.game_title << " (" << host.title_id << ") | " << ms_per_frame << " ms/frame (" << fps << " frames/sec)";
            SDL_SetWindowTitle(host.window.get(), title.str().c_str());
            host.t1 = t2;
            host.frame_count = 0;
        }
    }
    return Success;
}
