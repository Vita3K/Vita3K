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

#include <config/state.h>
#include <emuenv/state.h>
#include <gui-imgui/functions.h>
#include <renderer/frame_host.h>
#include <renderer/functions.h>
#include <renderer/state.h>
#include <util/log.h>

#include <SDL3/SDL_video.h>

#ifdef __ANDROID__
#include <app/functions.h>
#include <audio/state.h>
#include <ctrl/functions.h>
#include <ime/keyboard.h>
#include <sdl-frontend/src/frame_host.h>

#include <SDL3/SDL_init.h>
#endif

#ifndef __ANDROID__
#include <config/version.h>
#include <display/state.h>
#endif

namespace gui {

WindowState::WindowState(EmuEnvState &emuenv)
    : emuenv(emuenv) {
}

WindowState::~WindowState() {
    if (emuenv.renderer) {
        if (renderer_initialized) {
            renderer::stop_render_thread(*emuenv.renderer);
            emuenv.renderer->cleanup();
        }
        emuenv.renderer.reset();
    }
    frame_host.reset();
#ifdef __ANDROID__
    if (gl_context)
        SDL_GL_DestroyContext(gl_context);
    ime::set_sdl_window(nullptr);
#endif
    window.reset();
#ifdef __ANDROID__
    if (emuenv.audio.adapter) {
        emuenv.audio.adapter.reset();
        emuenv.audio.audio_backend.clear();
    }
    SDL_Quit();
#endif
}

#ifndef __ANDROID__
namespace {

class SdlFrameHost final : public renderer::FrameHost {
public:
    explicit SdlFrameHost(SDL_Window *window)
        : window(window) {
    }

    ~SdlFrameHost() override {
        destroy_render_context();
    }

    bool create_gl_context(bool vsync) {
        constexpr int minor_versions[] = { 6, 5, 4 };
        for (const int minor : minor_versions) {
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
            SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
            SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
            gl_context = SDL_GL_CreateContext(window);
            if (gl_context) {
                SDL_GL_SetSwapInterval(vsync ? 1 : 0);
                return true;
            }
        }

        LOG_ERROR("Failed to create an OpenGL context: {}", SDL_GetError());
        return false;
    }

    renderer::DisplayHandle handle() const override {
        const SDL_PropertiesID properties = SDL_GetWindowProperties(window);
#ifdef _WIN32
        return renderer::Win32DisplayHandle{ SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr) };
#elif defined(__APPLE__)
        return renderer::MacOSDisplayHandle{ SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr) };
#else
        if (void *display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr)) {
            return renderer::WaylandDisplayHandle{
                display,
                SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr)
            };
        }
        if (void *display = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr)) {
            return renderer::X11DisplayHandle{
                display,
                static_cast<std::uintptr_t>(SDL_GetNumberProperty(properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)),
                nullptr
            };
        }
        return {};
#endif
    }

    int drawable_width() const override {
        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        return width;
    }

    int drawable_height() const override {
        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        return height;
    }

    std::vector<std::string> font_dirs() const override {
        return {};
    }

    void *get_proc_address(const char *name) const override {
        return reinterpret_cast<void *>(SDL_GL_GetProcAddress(name));
    }

    bool make_current() override {
        return gl_context && SDL_GL_MakeCurrent(window, gl_context);
    }

    void done_current() override {
        if (gl_context)
            SDL_GL_MakeCurrent(window, nullptr);
    }

    void prepare_for_render_thread() override {
        done_current();
    }

    void swap_buffers() override {
        if (gl_context)
            SDL_GL_SwapWindow(window);
    }

    bool set_vsync(bool enabled) override {
        return !gl_context || SDL_GL_SetSwapInterval(enabled ? 1 : 0);
    }

    void destroy_render_context() override {
        if (gl_context) {
            SDL_GL_DestroyContext(gl_context);
            gl_context = nullptr;
        }
    }

private:
    SDL_Window *window = nullptr;
    SDL_GLContext gl_context = nullptr;
};

} // namespace

bool init_window(GuiState &gui, EmuEnvState &state) {
    gui.window_state = std::make_unique<WindowState>(state);
    auto &window_state = *gui.window_state;
    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    switch (state.backend_renderer) {
    case renderer::Backend::OpenGL:
        window_flags |= SDL_WINDOW_OPENGL;
        break;
    case renderer::Backend::Vulkan:
        window_flags |= SDL_WINDOW_VULKAN;
        break;
    default:
        LOG_ERROR("Unimplemented backend renderer: {}.", state.cfg.backend_renderer);
        return false;
    }

    if (state.cfg.fullscreen) {
        state.display.fullscreen = true;
        window_flags |= SDL_WINDOW_FULLSCREEN;
    }

    gui.manual_dpi_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    window_state.window = WindowPtr(SDL_CreateWindow(
                                        window_title,
                                        static_cast<int>(DEFAULT_RES_WIDTH * gui.manual_dpi_scale),
                                        static_cast<int>(DEFAULT_RES_HEIGHT * gui.manual_dpi_scale),
                                        window_flags),
        SDL_DestroyWindow);
    if (!window_state.window) {
        LOG_ERROR("SDL failed to create window: {}", SDL_GetError());
        return false;
    }

    gui.manual_dpi_scale = SDL_GetDisplayContentScale(SDL_GetDisplayForWindow(window_state.window.get()));
    auto frame_host = std::make_unique<SdlFrameHost>(window_state.window.get());
    if (state.backend_renderer == renderer::Backend::OpenGL
        && !frame_host->create_gl_context(state.cfg.current_config.v_sync)) {
        return false;
    }

    window_state.frame_host = std::move(frame_host);
    if (!renderer::init(*window_state.frame_host, state.renderer, state.backend_renderer, state.cfg, state.get_root_paths())) {
        LOG_ERROR("Failed to initialize the renderer for the ImGui frontend.");
        return false;
    }
    window_state.renderer_initialized = true;

    const int width = window_state.frame_host->drawable_width();
    const int height = window_state.frame_host->drawable_height();
    state.display.viewport_drawable_w = width;
    state.display.viewport_drawable_h = height;
    state.display.viewport_w = static_cast<float>(width);
    state.display.viewport_h = static_cast<float>(height);
    gui.drawable_viewport_size = { static_cast<float>(width), static_cast<float>(height) };
    gui.logical_viewport_size = gui.drawable_viewport_size;
    return true;
}

#else

bool init_window(GuiState &gui, EmuEnvState &emuenv) {
    gui.window_state = std::make_unique<WindowState>(emuenv);
    auto &window_state = *gui.window_state;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC | SDL_INIT_SENSOR | SDL_INIT_CAMERA)) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        return false;
    }

    SDL_WindowFlags window_flags = emuenv.backend_renderer == renderer::Backend::OpenGL ? SDL_WINDOW_OPENGL : SDL_WINDOW_VULKAN;
    if (emuenv.backend_renderer == renderer::Backend::OpenGL) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    }

    SDL_PropertiesID window_props = SDL_CreateProperties();
    if (!window_props) {
        LOG_ERROR("SDL_CreateProperties failed: {}", SDL_GetError());
        return false;
    }
    SDL_SetStringProperty(window_props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Vita3K");
    SDL_SetNumberProperty(window_props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 960);
    SDL_SetNumberProperty(window_props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 544);
    SDL_SetNumberProperty(window_props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, window_flags);
    if (emuenv.backend_renderer == renderer::Backend::OpenGL)
        SDL_SetBooleanProperty(window_props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
    auto *window = SDL_CreateWindowWithProperties(window_props);
    SDL_DestroyProperties(window_props);
    if (!window) {
        LOG_ERROR("SDL_CreateWindowWithProperties failed: {}", SDL_GetError());
        return false;
    }

    if (emuenv.backend_renderer == renderer::Backend::OpenGL) {
        window_state.gl_context = SDL_GL_CreateContext(window);
        if (!window_state.gl_context || !SDL_GL_MakeCurrent(window, window_state.gl_context)) {
            LOG_ERROR("Failed to create OpenGL ES context: {}", SDL_GetError());
            if (window_state.gl_context) {
                SDL_GL_DestroyContext(window_state.gl_context);
                window_state.gl_context = nullptr;
            }
            SDL_DestroyWindow(window);
            return false;
        }
        SDL_GL_SetSwapInterval(static_cast<int>(emuenv.cfg.current_config.v_sync));
    }

    window_state.window = WindowPtr(window, SDL_DestroyWindow);
    window_state.frame_host = std::make_unique<sdl_frontend::FrameHost>(window, &window_state.gl_context);
    ime::set_sdl_window(window);
    refresh_controllers(emuenv.ctrl, emuenv);
    if (!renderer::init(*window_state.frame_host, emuenv.renderer, emuenv.backend_renderer, emuenv.cfg, emuenv.get_root_paths())) {
        LOG_ERROR("Failed to initialize the ImGui frontend renderer.");
        return false;
    }
    window_state.renderer_initialized = true;
    app::apply_renderer_config(emuenv);
    return true;
}

#endif

} // namespace gui
