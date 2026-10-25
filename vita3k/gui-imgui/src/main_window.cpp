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

#include "private.h"

#include <app/functions.h>
#include <app/session_controller.h>
#include <audio/state.h>
#include <bgm_player/functions.h>
#include <config/functions.h>
#include <config/state.h>
#include <config/version.h>
#include <ctrl/functions.h>
#include <ctrl/state.h>
#include <dialog/state.h>
#include <display/state.h>
#include <emuenv/app_launch_request.h>
#include <gui-imgui/functions.h>
#include <gxm/state.h>
#include <ime/functions.h>
#include <interface.h>
#include <io/state.h>
#include <kernel/state.h>
#include <motion/event_handler.h>
#include <renderer/frame_host.h>
#include <renderer/functions.h>
#include <renderer/state.h>
#include <sdl/frame_host.h>
#include <sdl/sdl_event.h>
#include <sdl/sdl_init.h>
#include <sdl/sdl_title.h>
#include <sdl/sdl_window.h>
#include <touch/functions.h>
#include <touch/state.h>
#include <util/log.h>
#include <util/string_utils.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_sensor.h>
#include <SDL3/SDL_video.h>

#if USE_DISCORD
#include <app/discord.h>
#endif

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <utility>

#if defined(__ANDROID__)
#include <ime/keyboard.h>

#include <SDL3/SDL_system.h>
#include <cstdlib>
#include <jni.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <process.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
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
#ifdef __ANDROID__
    if (frame_host)
        frame_host->destroy_render_context();
#endif
    frame_host.reset();
#ifdef __ANDROID__
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

class SdlFrameHost final : public sdl::FrameHost {
public:
    SdlFrameHost(SDL_Window *window, SDL_GLContext *gl_context)
        : sdl::FrameHost(window, gl_context) {
    }

    ~SdlFrameHost() override {
        destroy_render_context();
    }

    bool create_gl_context(bool vsync) {
        if (!gl_context)
            return false;

        *gl_context = SDL_GL_CreateContext(window);
        if (!*gl_context) {
            LOG_ERROR("Failed to create an OpenGL context: {}", SDL_GetError());
            return false;
        }
        set_vsync(vsync);
        return true;
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

    const renderer::VulkanSurfaceProvider *vulkan_surface_provider() const override {
        return nullptr;
    }

    bool set_vsync(bool enabled) override {
        return !gl_context || !*gl_context || sdl::FrameHost::set_vsync(enabled);
    }

    std::vector<std::string> font_dirs() const override {
        return {};
    }
};

} // namespace
#endif

static void update_viewport(GuiState &gui, const EmuEnvState &state) {
    auto *window = gui.window_state->window.get();
    if (!sdl::update_viewport(window, state.cfg.current_config.stretch_the_display_area,
            state.cfg.current_config.fullscreen_hd_res_pixel_perfect, state.display.fullscreen.load()))
        return;
    const auto &viewport = sdl::get_viewport();
    ImGui::GetIO().FontGlobalScale = viewport.manual_dpi_scale;

    // Update nearest font level
    float scale = viewport.scale_y * viewport.system_dpi_scale * viewport.manual_dpi_scale;
    gui.current_font_level = 0;
    for (int i = 0; i <= gui.max_font_level; i++) {
        if (i == gui.max_font_level || scale <= FontScaleCandidates[i]) {
            gui.current_font_level = i;
            break;
        }
        if (FontScaleCandidates[i] / scale > scale / FontScaleCandidates[i + 1]) {
            gui.current_font_level = i;
            break;
        }
    }
}

static void switch_full_screen(GuiState &gui, EmuEnvState &emuenv) {
    const bool fullscreen = !emuenv.display.fullscreen.load();
    if (sdl::set_fullscreen(emuenv, gui.window_state->window.get(), fullscreen))
        update_viewport(gui, emuenv);
}

static void update_live_area_last_app_frame(EmuEnvState &emuenv, GuiState &gui) {
    uint32_t width = 0;
    uint32_t height = 0;
    auto frame = get_current_app_frame(emuenv, width, height);
    if (frame.empty()) {
        LOG_ERROR("Failed to dump current app frame for live area");
        return;
    }

    gui.live_area_last_app_frame = ImGui_Texture(gui.imgui_state.get(), frame.data(), width, height);
}

static bool pump_sdl_events(EmuEnvState &emuenv, GuiState &gui, app::AppSessionController *session = nullptr) {
    std::lock_guard<std::mutex> render_lock(gui.render_mutex);
    const bool ime_dialog_active = emuenv.common_dialog.type == IME_DIALOG
        && emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING;
    if (ime_dialog_active && !SDL_TextInputActive(gui.window_state->window.get())) {
        if (gui.imgui_state)
            sdl::clear_keyboard_state(emuenv);
        SDL_StartTextInput(gui.window_state->window.get());
    } else if (!ime_dialog_active && gui.imgui_state
        && !gui.imgui_state->is_typing && !ImGui::GetIO().WantTextInput
        && SDL_TextInputActive(gui.window_state->window.get())) {
        SDL_StopTextInput(gui.window_state->window.get());
    }
    const auto allow_switch_state = !emuenv.io.title_id.empty() && !gui.vita_area.live_area_screen && !gui.vita_area.app_close && !gui.vita_area.online_storage && (!gui.vita_area.home_screen || emuenv.kernel.is_threads_paused()) && !gui.vita_area.user_management && !gui.configuration_menu.custom_settings_dialog && !gui.configuration_menu.settings_dialog && !gui.controls_menu.controls_dialog && gui::get_sys_apps_state(gui);

    const auto toggle_pause = [session]() {
        if (session && session->is_running())
            session->set_pause_reason(app::AppSessionPauseReason::User, !session->is_paused());
    };

    const auto ui_navigation = [&emuenv, &gui, session, allow_switch_state, &toggle_pause](const uint32_t sce_ctrl_btn) {
        switch (sce_ctrl_btn) {
        case SCE_CTRL_CROSS:
        case SCE_CTRL_CIRCLE:
            gui.is_key_locked = true;
            if (gui.vita_area.start_screen)
                gui::close_start_screen(gui, emuenv);
            break;
        case SCE_CTRL_PSBUTTON:
            gui.is_key_locked = true;
            if (allow_switch_state) {
                if (!emuenv.cfg.show_live_area_screen) {
                    toggle_pause();
                } else {
                    const auto live_area_app_index = gui::get_live_area_current_open_apps_list_index(gui, emuenv.io.app_path);
                    if (live_area_app_index == gui.live_area_current_open_apps_list.end())
                        gui::open_live_area(gui, emuenv, emuenv.io.app_path);
                    else {
                        if ((gui.live_area_app_current_open < 0) || (gui.live_area_current_open_apps_list[gui.live_area_app_current_open] != emuenv.io.app_path))
                            gui.live_area_app_current_open = static_cast<int32_t>(std::distance(live_area_app_index, gui.live_area_current_open_apps_list.end()) - 1);
                        gui.vita_area.information_bar = true;
                        gui.vita_area.live_area_screen = true;
                    }
                    gui.imgui_render = true;
#ifdef __ANDROID__
                    gui::set_controller_overlay_state(0);
#endif

                    if (session && session->is_running()) {
                        const bool was_paused = session->is_paused();
                        if (!was_paused)
                            update_live_area_last_app_frame(emuenv, gui);

                        session->set_pause_reason(app::AppSessionPauseReason::Menu, true);
                        if (!was_paused) {
                            app::update_app_time_used(emuenv, emuenv.io.app_path);
                            gui.gate_animation.start(GateAnimationState::ReturnApp);
                            bgm_player::switch_bgm_state(false);
                        }
                    }
                }
            } else if (!gui::get_sys_apps_state(gui))
                gui::close_system_app(gui, emuenv);
            break;
        default: break;
        }

        if (gui.vita_area.app_close) {
            switch (sce_ctrl_btn) {
            case SCE_CTRL_CIRCLE:
            case SCE_CTRL_CROSS:
                if (gui.vita_area.app_close_button == 0)
                    gui.vita_area.app_close_button = sce_ctrl_btn;
                break;
            default: break;
            }
        } else if (gui.vita_area.user_management)
            gui::browse_users_management(gui, emuenv, sce_ctrl_btn);
        else if (gui.vita_area.manual)
            gui::browse_pages_manual(gui, emuenv, sce_ctrl_btn);
        else if (gui.vita_area.home_screen)
            gui::browse_home_apps_list(gui, emuenv, sce_ctrl_btn, session);
        else if (gui.vita_area.live_area_screen)
            gui::browse_live_area_apps_list(gui, emuenv, sce_ctrl_btn, session);
    };

    emuenv.drop_inputs = gui.configuration_menu.settings_dialog || gui.configuration_menu.custom_settings_dialog || gui.controls_menu.controllers_dialog || gui.controls_menu.controls_dialog;
    std::set<uint32_t> last_buttons;

    const auto update_mouse_position = [&emuenv](const float x, const float y) {
        const auto &viewport = sdl::get_viewport();
        const float scale_x = viewport.window_width > 0.f ? viewport.drawable_width / viewport.window_width : 1.0f;
        const float scale_y = viewport.window_height > 0.f ? viewport.drawable_height / viewport.window_height : 1.0f;
        emuenv.touch.mouse_x = x * scale_x;
        emuenv.touch.mouse_y = y * scale_y;

        if (viewport.drawable_view_width > 0.f && viewport.drawable_view_height > 0.f) {
            auto &mouse = emuenv.ctrl.overlay_mouse;
            mouse.x.store((emuenv.touch.mouse_x - viewport.drawable_x) * DEFAULT_RES_WIDTH / viewport.drawable_view_width,
                std::memory_order_relaxed);
            mouse.y.store((emuenv.touch.mouse_y - viewport.drawable_y) * DEFAULT_RES_HEIGHT / viewport.drawable_view_height,
                std::memory_order_relaxed);
        }
    };

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        const bool route_text_to_ime = ime_dialog_active
            && (event.type == SDL_EVENT_TEXT_INPUT || event.type == SDL_EVENT_TEXT_EDITING);
        if (!route_text_to_ime)
            ImGui_ImplSdl_ProcessEvent(gui.imgui_state.get(), &event);
        const auto shared_event = sdl::process_event(emuenv, event);
        if (shared_event.firmware_updated) {
            if (emuenv.cfg.initial_setup)
                gui::init_theme(gui, emuenv, emuenv.app.user_list.users[emuenv.cfg.user_id].theme_id);
            else if (bgm_player::init_bgm(emuenv.vita_fs_path, true))
                bgm_player::switch_bgm_state(false);
        }
        if (shared_event.outcome == sdl::EventOutcome::QuitRequested) {
            gui.is_capturing_keys = false;
            return false;
        }
        switch (event.type) {
        case SDL_EVENT_KEY_DOWN: {
            if (gui.controller_binding_capture >= 0) {
                gui.controller_binding_capture = -1;
                continue;
            }
            if (ime_dialog_active) {
                switch (event.key.key) {
                case SDLK_BACKSPACE: {
                    std::lock_guard lock(emuenv.ime.mutex);
                    ime_backspace(emuenv.ime);
                    continue;
                }
                case SDLK_LEFT: {
                    std::lock_guard lock(emuenv.ime.mutex);
                    ime_cursor_left(emuenv.ime);
                    continue;
                }
                case SDLK_RIGHT: {
                    std::lock_guard lock(emuenv.ime.mutex);
                    ime_cursor_right(emuenv.ime);
                    continue;
                }
                case SDLK_RETURN:
                case SDLK_KP_ENTER: {
                    auto &dialog = emuenv.common_dialog;
                    std::lock_guard<std::recursive_mutex> dialog_lock(dialog.mutex);
                    std::lock_guard ime_lock(emuenv.ime.mutex);
                    const size_t copy_len = std::min(static_cast<size_t>(emuenv.ime.str.length()), static_cast<size_t>(dialog.ime.max_length));
                    std::memcpy(dialog.ime.result, emuenv.ime.str.c_str(), copy_len * sizeof(uint16_t));
                    dialog.ime.result[copy_len] = 0;
                    const std::string utf8 = string_utils::utf16_to_utf8(emuenv.ime.str);
                    snprintf(dialog.ime.text, sizeof(dialog.ime.text), "%s", utf8.c_str());
                    emuenv.ime.event_id = SCE_IME_EVENT_PRESS_ENTER;
                    dialog.ime.status = SCE_IME_DIALOG_BUTTON_ENTER;
                    dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
                    dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
                    continue;
                }
                case SDLK_ESCAPE: {
                    auto &dialog = emuenv.common_dialog;
                    std::lock_guard<std::recursive_mutex> dialog_lock(dialog.mutex);
                    if (dialog.ime.cancelable) {
                        std::lock_guard ime_lock(emuenv.ime.mutex);
                        emuenv.ime.event_id = SCE_IME_EVENT_PRESS_CLOSE;
                        dialog.ime.status = SCE_IME_DIALOG_BUTTON_CLOSE;
                        dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
                        dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
                    }
                    continue;
                }
                default:
                    continue;
                }
            }
            const auto physical_key = shared_event.physical_key;

            auto sce_ctrl_btn = shared_event.vita_button;
            if (gui.is_capturing_keys && physical_key != input::PhysicalKeyCode::Unbound) {
                gui.is_key_capture_dropped = false;
                if (physical_key == input::PhysicalKeyCode::Escape) {
                    LOG_ERROR("Key is reserved!");
                    gui.captured_key = gui.old_captured_key;
                    gui.is_key_capture_dropped = true;
                } else
                    gui.captured_key = static_cast<int>(physical_key);
                gui.is_capturing_keys = false;
                auto capture_completion = std::move(gui.key_capture_completion);
                gui.key_capture_completion = {};
                if (capture_completion)
                    capture_completion();
            }

            if (ImGui::GetIO().WantTextInput || gui.is_key_locked || (emuenv.drop_inputs && !gui.vita_area.live_area_screen) || gui.gate_animation.state != GateAnimationState::None)
                continue;
#ifdef __ANDROID__
            if (event.key.scancode == SDL_SCANCODE_AC_BACK)
                sce_ctrl_btn = SCE_CTRL_PSBUTTON;
#else
            if (allow_switch_state && (physical_key == emuenv.cfg.keyboard_gui_toggle_gui || physical_key == emuenv.cfg.keyboard_gui_toggle_gui_alt))
                gui.imgui_render = !gui.imgui_render;
            if ((physical_key == emuenv.cfg.keyboard_gui_toggle_touch || physical_key == emuenv.cfg.keyboard_gui_toggle_touch_alt) && !gui.is_key_capture_dropped)
                toggle_touchscreen(emuenv.touch);
            if (!event.key.repeat && !gui.is_key_capture_dropped
                && sdl::process_fullscreen_key(emuenv, gui.window_state->window.get(), physical_key))
                update_viewport(gui, emuenv);
            if ((physical_key == emuenv.cfg.keyboard_toggle_texture_replacement || physical_key == emuenv.cfg.keyboard_toggle_texture_replacement_alt) && !gui.is_key_capture_dropped)
                toggle_texture_replacement(emuenv);
            if ((physical_key == emuenv.cfg.keyboard_take_screenshot || physical_key == emuenv.cfg.keyboard_take_screenshot_alt) && !gui.is_key_capture_dropped)
                take_screenshot(emuenv);
            if ((physical_key == emuenv.cfg.keyboard_pinch_modifier || physical_key == emuenv.cfg.keyboard_pinch_modifier_alt || physical_key == emuenv.cfg.keyboard_alternate_pinch_in || physical_key == emuenv.cfg.keyboard_alternate_pinch_in_alt || physical_key == emuenv.cfg.keyboard_alternate_pinch_out || physical_key == emuenv.cfg.keyboard_alternate_pinch_out_alt) && !gui.is_key_capture_dropped)
                pinch_modifier(emuenv.touch, true);

            constexpr float pinch_amount = 0.5f;
            if ((physical_key == emuenv.cfg.keyboard_alternate_pinch_in || physical_key == emuenv.cfg.keyboard_alternate_pinch_in_alt) && !gui.is_key_capture_dropped)
                pinch_automove(emuenv.touch, -pinch_amount);
            if ((physical_key == emuenv.cfg.keyboard_alternate_pinch_out || physical_key == emuenv.cfg.keyboard_alternate_pinch_out_alt) && !gui.is_key_capture_dropped)
                pinch_automove(emuenv.touch, pinch_amount);
#endif
            if (sce_ctrl_btn != 0) {
                if (last_buttons.contains(sce_ctrl_btn))
                    continue;
                last_buttons.insert(sce_ctrl_btn);
                ui_navigation(sce_ctrl_btn);
            }
            break;
        }

        case SDL_EVENT_KEY_UP: {
            gui.is_key_locked = false;
            const auto physical_key = shared_event.physical_key;
            if (physical_key == emuenv.cfg.keyboard_pinch_modifier || physical_key == emuenv.cfg.keyboard_pinch_modifier_alt || physical_key == emuenv.cfg.keyboard_alternate_pinch_in || physical_key == emuenv.cfg.keyboard_alternate_pinch_in_alt || physical_key == emuenv.cfg.keyboard_alternate_pinch_out || physical_key == emuenv.cfg.keyboard_alternate_pinch_out_alt) {
                pinch_modifier(emuenv.touch, false);
                pinch_automove(emuenv.touch, 0.0f);
            }
            break;
        }

        case SDL_EVENT_TEXT_INPUT: {
            if (ime_dialog_active) {
                std::lock_guard lock(emuenv.ime.mutex);
                ime_commit_text(emuenv.ime, string_utils::utf8_to_utf16(event.text.text));
            }
            break;
        }

        case SDL_EVENT_TEXT_EDITING: {
            if (ime_dialog_active) {
                std::lock_guard lock(emuenv.ime.mutex);
                ime_set_preedit(emuenv.ime, string_utils::utf8_to_utf16(event.edit.text));
            }
            break;
        }

        case SDL_EVENT_MOUSE_WHEEL:
            pinch_move(emuenv.touch, event.wheel.y);
            gui.is_nav_button = false;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            update_mouse_position(event.motion.x, event.motion.y);
            gui.is_nav_button = false;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            update_mouse_position(event.button.x, event.button.y);
            if (event.button.button == SDL_BUTTON_LEFT) {
                emuenv.touch.mouse_button_left = true;
                emuenv.ctrl.overlay_mouse.pressed.store(true, std::memory_order_relaxed);
            }
            if (event.button.button == SDL_BUTTON_RIGHT)
                emuenv.touch.mouse_button_right = true;
            gui.is_nav_button = false;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            update_mouse_position(event.button.x, event.button.y);
            if (event.button.button == SDL_BUTTON_LEFT) {
                emuenv.touch.mouse_button_left = false;
                emuenv.ctrl.overlay_mouse.pressed.store(false, std::memory_order_relaxed);
            }
            if (event.button.button == SDL_BUTTON_RIGHT)
                emuenv.touch.mouse_button_right = false;
            gui.is_nav_button = false;
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            if (gui.controller_binding_capture >= 0) {
                const auto binding = static_cast<size_t>(gui.controller_binding_capture);
                if (binding < emuenv.cfg.controller_binds.size()) {
                    emuenv.cfg.controller_binds[binding] = event.gbutton.button;
                    config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
                }
                gui.controller_binding_capture = -1;
                continue;
            }
            if (!emuenv.kernel.is_threads_paused() && event.gbutton.button == SDL_GAMEPAD_BUTTON_TOUCHPAD)
                toggle_touchscreen(emuenv.touch);
            if (ImGui::GetIO().WantTextInput || gui.is_key_locked || (emuenv.drop_inputs && !gui.vita_area.live_area_screen) || gui.gate_animation.state != GateAnimationState::None)
                continue;
            if (shared_event.vita_button != 0) {
                if (last_buttons.contains(shared_event.vita_button))
                    continue;
                last_buttons.insert(shared_event.vita_button);
                ui_navigation(shared_event.vita_button);
            }
            break;

        case SDL_EVENT_GAMEPAD_BUTTON_UP:
            gui.is_key_locked = false;
            break;

        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            update_viewport(gui, emuenv);
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            pinch_modifier(emuenv.touch, false);
            pinch_automove(emuenv.touch, 0.0f);
            gui.is_key_locked = false;
            break;
        default: break;
        }
    }

    return true;
}

static bool run_execv(char *argv[], const AppLaunchRequest &request,
    const std::string &self_path, const std::string &configured_app_args) {
    const auto &exec_path = request.self_path.empty() ? self_path : request.self_path;
    std::string exec_args = configured_app_args;
    if (!request.argv.empty()) {
        exec_args = request.argv.front();
        for (size_t i = 1; i < request.argv.size(); ++i)
            exec_args += ", " + request.argv[i];
    }

#ifdef __ANDROID__
    (void)argv;
    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
    jobject activity = reinterpret_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) {
        LOG_ERROR("Cannot restart ImGui: Android activity or JNI environment is unavailable");
        return false;
    }

    if (request.app_path.empty() || env->PushLocalFrame(8) < 0) {
        LOG_ERROR("Cannot prepare the Android ImGui restart request");
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        env->DeleteLocalRef(activity);
        return false;
    }

    const bool restart_requested = [&]() {
        jclass clazz = env->GetObjectClass(activity);
        if (!clazz)
            return false;
        jmethodID method = env->GetMethodID(clazz, "restartApp", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
        if (!method)
            return false;

        jstring app_path = env->NewStringUTF(request.app_path.c_str());
        if (!app_path)
            return false;
        jstring jexec_path = env->NewStringUTF(exec_path.c_str());
        if (!jexec_path)
            return false;
        jstring app_args = env->NewStringUTF(exec_args.c_str());
        if (!app_args)
            return false;
        env->CallVoidMethod(activity, method, app_path, jexec_path, app_args);
        return !env->ExceptionCheck();
    }();
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    env->PopLocalFrame(nullptr);
    env->DeleteLocalRef(activity);
    if (!restart_requested) {
        LOG_ERROR("Failed to request an Android ImGui cold restart; keeping the frontend open");
        return false;
    }
    LOG_INFO("Android ImGui cold restart requested for '{}'", request.app_path);
    // The restart activity terminates this process once it is running independently.
    return true;
#else
    if (!argv || !argv[0] || request.app_path.empty())
        return false;

    const char *args[10] = { argv[0], "-a", "true", "-r", request.app_path.c_str() };
    if (!exec_path.empty()) {
        args[5] = "--self";
        args[6] = exec_path.c_str();
        if (!exec_args.empty()) {
            args[7] = "--app-args";
            args[8] = exec_args.c_str();
        }
    } else if (!exec_args.empty()) {
        args[5] = "--app-args";
        args[6] = exec_args.c_str();
    }

#ifdef _WIN32
    FreeConsole();
    _execv(argv[0], args);
#elif defined(__unix__) || defined(__APPLE__)
    execv(argv[0], const_cast<char *const *>(args));
#endif
    LOG_ERROR("Failed to relaunch Vita3K: {}", std::strerror(errno));
    return false;
#endif
}

bool init_window(GuiState &gui, EmuEnvState &state) {
    gui.window_state = std::make_unique<WindowState>(state);
    auto &window_state = *gui.window_state;

#ifdef __ANDROID__
    if (!sdl::initialize()) {
        LOG_ERROR("SDL_Init failed: {}", SDL_GetError());
        return false;
    }
#endif

    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#ifdef __ANDROID__
    window_flags |= SDL_WINDOW_FULLSCREEN;
    state.display.fullscreen = true;
    const char *title = "Vita3K";
#else
    const char *title = window_title;

    if (state.cfg.fullscreen) {
        state.display.fullscreen = true;
        window_flags |= SDL_WINDOW_FULLSCREEN;
    }
#endif

    auto *window = sdl::create_window(title, state.backend_renderer, window_flags);
    if (!window) {
        LOG_ERROR("SDL window creation failed: {}", SDL_GetError());
        return false;
    }

#ifdef __ANDROID__
    window_state.frame_host = std::make_unique<sdl::FrameHost>(window, &window_state.gl_context);
    if (state.backend_renderer == renderer::Backend::OpenGL) {
        window_state.gl_context = SDL_GL_CreateContext(window);
        if (!window_state.gl_context || !window_state.frame_host->make_current()) {
            LOG_ERROR("Failed to create OpenGL ES context: {}", SDL_GetError());
            window_state.frame_host->destroy_render_context();
            SDL_DestroyWindow(window);
            return false;
        }
        window_state.frame_host->set_vsync(state.cfg.current_config.v_sync);
    }

    window_state.window = WindowPtr(window, SDL_DestroyWindow);
    ime::set_sdl_window(window);
    refresh_controllers(state.ctrl, state);
#else
    window_state.window = WindowPtr(window, SDL_DestroyWindow);
    auto frame_host = std::make_unique<SdlFrameHost>(window_state.window.get(), &window_state.gl_context);
    if (state.backend_renderer == renderer::Backend::OpenGL
        && !frame_host->create_gl_context(state.cfg.current_config.v_sync)) {
        return false;
    }

    window_state.frame_host = std::move(frame_host);
#endif

    if (!renderer::init(*window_state.frame_host, state.renderer, state.backend_renderer, state.cfg, state.get_root_paths())) {
        LOG_ERROR("Failed to initialize the renderer for the ImGui frontend.");
        return false;
    }
    window_state.renderer_initialized = true;

    return true;
}

ExitCode run_frontend(EmuEnvState &emuenv, char *argv[]) {
    if (emuenv.cfg.console)
        return Success;

    const auto renderer_config = emuenv.cfg.current_config;

#ifdef __ANDROID__
    // The process outlives the frontend on Android. Declared before gui so that the ImGui context
    // is destroyed after it, on every exit path: a leftover context would keep its font atlas and
    // backend data pointing to the destroyed renderer.
    struct ImGuiContextGuard {
        ~ImGuiContextGuard() {
            if (ImGui::GetCurrentContext() != NULL)
                ImGui::DestroyContext();
        }
    } imgui_context_guard;
#endif
    GuiState gui;
    if (!init_window(gui, emuenv))
        return RendererInitFailed;

    // Qt initializes this when its main window is created. The ImGui frontend
    // must do the same before the graphics settings dialog is drawn.
    if (!emuenv.vulkan_device_info)
        emuenv.vulkan_device_info = std::make_unique<renderer::VulkanDeviceInfo>(renderer::enumerate_vulkan_devices());

    const auto run_app_path = emuenv.cfg.run_app_path;
    // Declared after gui so that the backend is shut down before the GUI textures are destroyed
    // and while the renderer is still alive.
    struct ImGuiBackendGuard {
        GuiState &gui;
        EmuEnvState &emuenv;
        ~ImGuiBackendGuard() {
            if (gui.imgui_state && !gui.imgui_state->shutdown && emuenv.renderer
                && gui.imgui_state->renderer == emuenv.renderer.get())
                ImGui_ImplSdl_Shutdown(gui.imgui_state.get());
        }
    } imgui_backend_guard{ gui, emuenv };
    pre_init(gui, emuenv);
#ifdef __ANDROID__
    update_viewport(gui, emuenv);
#endif
    bgm_player::init_bgm_player(emuenv.cfg.bgm_volume);

    auto present = std::chrono::system_clock::now();
    auto later = std::chrono::system_clock::now();
    constexpr double frame_time = 1000.0 / 60.0;

    const auto wait_for_frame_done = [&]() {
        present = std::chrono::system_clock::now();
        const std::chrono::duration<double, std::milli> work_time = present - later;
        if (work_time.count() < frame_time) {
            const std::chrono::duration<double, std::milli> delta_ms(frame_time - work_time.count());
            const auto delta_ms_duration = std::chrono::duration_cast<std::chrono::milliseconds>(delta_ms);
            std::this_thread::sleep_for(delta_ms_duration);
        }
        later = std::chrono::system_clock::now();
    };

    if (!emuenv.cfg.initial_setup) {
        emuenv.cfg.system_music.emplace(false);
        if (bgm_player::init_bgm(emuenv.vita_fs_path, true))
            bgm_player::switch_bgm_state(true);

        while (!emuenv.cfg.initial_setup) {
            wait_for_frame_done();
            if (!pump_sdl_events(emuenv, gui)) {
                bgm_player::destroy_bgm_player();
                return QuitRequested;
            }

            update_viewport(gui, emuenv);
            draw_begin(gui, emuenv);
            draw_initial_setup(gui, emuenv);
            draw_end(gui);
            emuenv.renderer->swap_window();
        }

        if (!gui.fw_font)
            load_fonts(gui, emuenv, true);
    }

    init(gui, emuenv);
    update_viewport(gui, emuenv);

    app::AppSessionController session(emuenv, true);
    bool renderer_cleaned = false;
    const auto stop_session = [&](const app::AppSessionStopReason reason) {
        const bool had_active_session = session.has_active_session();
        session.stop(reason);
#ifdef __ANDROID__
        if (had_active_session)
            set_controller_overlay_state(0);
#endif
        if (had_active_session && emuenv.renderer)
            renderer_cleaned = true;
        if (emuenv.renderer)
            emuenv.renderer->frontend_frame_callback = {};
    };
    const auto draw_frontend = [&](const bool renderer_owns_frame) {
        if (!renderer_owns_frame) {
            wait_for_frame_done();
            if (!emuenv.renderer->set_current())
                return;
        }

        std::lock_guard<std::mutex> render_lock(gui.render_mutex);
        update_viewport(gui, emuenv);
        draw_begin(gui, emuenv);
        if (gui.app_selector.icon_async_loader)
            gui.app_selector.icon_async_loader->commit(gui);
        draw_vita_area(gui, emuenv, &session);
        if (renderer_owns_frame) {
            if (emuenv.cfg.current_config.show_touchpad_cursor && !emuenv.kernel.is_threads_paused())
                draw_touchpad_cursor(gui, emuenv);
        }

        if (gui.imgui_render)
            draw_ui(gui, emuenv, &session);

        draw_end(gui);
        if (!renderer_owns_frame)
            emuenv.renderer->swap_window();
    };
    bool auto_boot_consumed = !run_app_path.has_value();
    const auto launch_request = [&]() -> std::optional<AppLaunchRequest> {
        if (auto request = emuenv.take_app_launch_request()) {
            if (run_app_path && request->app_path == *run_app_path)
                auto_boot_consumed = true;
            return request;
        }
        if (run_app_path && !auto_boot_consumed) {
            auto_boot_consumed = true;
            AppLaunchRequest request{ .app_path = *run_app_path };
            return request;
        }
        return std::nullopt;
    };
    const auto return_to_frontend_after_app = [&]() {
        if (gui.live_area_app_current_open >= 0
            && gui.live_area_app_current_open < static_cast<int32_t>(gui.live_area_current_open_apps_list.size())
            && gui::get_live_area_current_open_apps_list_index(gui, gui.live_area_current_open_apps_list[gui.live_area_app_current_open]) != gui.live_area_current_open_apps_list.end()) {
            gui.vita_area.home_screen = false;
            gui.vita_area.live_area_screen = true;
            gui.vita_area.information_bar = true;
            gui.gate_animation.start(GateAnimationState::ReturnApp);
        } else {
            gui.vita_area.live_area_screen = false;
            gui.vita_area.information_bar = emuenv.cfg.show_info_bar;
            gui.vita_area.home_screen = true;
        }
        bgm_player::switch_bgm_state(false);
    };
    const auto launch_app = [&](const AppLaunchRequest &request) {
        if (!session.begin_launch(request, request.reason != AppLaunchReason::LoadExec)) {
            LOG_ERROR("Could not begin launching application {}.", request.app_path);
            return_to_frontend_after_app();
            return false;
        }

        // Reset the app list sort state to ensure the app list is sorted after launching an app
        gui.app_selector.is_app_list_sorted = false;

        const auto &current_config = emuenv.cfg.current_config;
        const auto restart_settings = get_emulator_restart_required_settings(renderer_config, current_config);
        const bool renderer_config_changed = emuenv.renderer
            && (emuenv.renderer->current_backend != emuenv.backend_renderer
                || !restart_settings.empty());
        if (renderer_config_changed) {
            LOG_INFO("Restarting Vita3K for renderer configuration changes: backend '{}' -> '{}', GPU {} -> {}, validation {} -> {}",
                renderer_config.backend_renderer, current_config.backend_renderer,
                renderer_config.gpu_idx, current_config.gpu_idx,
                renderer_config.validation_layer, current_config.validation_layer);
#ifdef __ANDROID__
            LOG_INFO("Vulkan driver '{}' -> '{}'", renderer_config.custom_driver_name, current_config.custom_driver_name);
#endif
            const auto restart_self_path = request.self_path.empty() ? emuenv.self_path : request.self_path;
            const auto restart_app_args = emuenv.cfg.app_args;
            if (!renderer_cleaned)
                stop_session(app::AppSessionStopReason::FrontendShutdown);
            if (run_execv(argv, request, restart_self_path, restart_app_args))
                return true;
            return_to_frontend_after_app();
            return false;
        }
        if (!emuenv.cfg.show_gui)
            gui.imgui_render = false;
        init_ime_lang(emuenv.ime, static_cast<SceImeLanguage>(emuenv.cfg.current_ime_lang));
        sdl::set_loading_window_title(gui.window_state->window.get(), emuenv);
        if (!gui.window_state->frame_host
            || !session.initialize_renderer(*gui.window_state->frame_host)
            || !session.initialize_runtime()) {
            stop_session(app::AppSessionStopReason::LaunchFailure);
            return_to_frontend_after_app();
            return false;
        }
        renderer_cleaned = false;
        if (emuenv.cfg.boot_apps_full_screen && !emuenv.display.fullscreen.load())
            switch_full_screen(gui, emuenv);
        emuenv.renderer->frontend_frame_callback = [&]() { draw_frontend(true); };
        if (!session.load_and_run()) {
            stop_session(app::AppSessionStopReason::LaunchFailure);
            return_to_frontend_after_app();
            return false;
        }
        emuenv.np.trophy_state.add_trophy_unlock_callback([&gui](NpTrophyUnlockCallbackData &callback_data) {
            const std::lock_guard<std::mutex> guard(gui.trophy_unlock_display_requests_access_mutex);
            gui.trophy_unlock_display_requests.insert(gui.trophy_unlock_display_requests.begin(), callback_data);
        });
        bgm_player::switch_bgm_state(true);
#ifdef __ANDROID__
        if (session.is_running())
            set_controller_overlay_state(get_overlay_display_mask(emuenv.cfg));
#endif
        return true;
    };

    bool running = true;
#if USE_DISCORD
    bool discord_rich_presence_old = false;
    const auto sync_discord = [&]() {
        if (!discordrpc::update_init_status(emuenv.cfg.discord_rich_presence, &discord_rich_presence_old))
            return;
        // Discord has just been connected: publish the current state.
        if (session.has_active_session())
            discordrpc::update_presence(emuenv.io.title_id, emuenv.current_app_title);
        else
            discordrpc::update_presence();
    };
#endif
    while (running) {
        if (auto request = launch_request())
            launch_app(*request);

        if (!session.has_active_session()) {
            if (!emuenv.cfg.show_gui)
                gui.imgui_render = true;
            sdl::set_default_window_title(gui.window_state->window.get());
            if (!pump_sdl_events(emuenv, gui))
                break;
#if USE_DISCORD
            sync_discord();
#endif
            draw_frontend(false);
            continue;
        }

        app::LaunchRuntimeMetrics metrics;
        while (session.is_running()) {
            wait_for_frame_done();
            if (!pump_sdl_events(emuenv, gui, &session)) {
                stop_session(app::AppSessionStopReason::FrontendShutdown);
                running = false;
                break;
            }
#if USE_DISCORD
            sync_discord();
#endif
            if (!emuenv.kernel.is_threads_paused() && app::update_runtime_metrics(emuenv, metrics)) {
                sdl::update_runtime_window_title(gui.window_state->window.get(), emuenv);
            }
            const auto request = emuenv.take_app_launch_request();
            if (request) {
                stop_session(app::AppSessionStopReason::Relaunch);
                if ((request->reason == AppLaunchReason::User) && gui.vita_area.live_area_screen) {
                    gui.gate_animation.start(GateAnimationState::EnterApp);
                    gui.gate_animation.launch_request = *request;
                    break;
                }
                if (request->reason != AppLaunchReason::ProcessExit) {
                    gui.vita_area.home_screen = false;
                    gui.vita_area.live_area_screen = false;
                    gui.vita_area.information_bar = false;
                    launch_app(*request);
                } else if (!gui.vita_area.online_storage && !gui.vita_area.live_area_screen && !gui.vita_area.home_screen)
                    return_to_frontend_after_app();
                break;
            }
            if (session.stop_requested()) {
                stop_session(app::AppSessionStopReason::UserRequest);
                return_to_frontend_after_app();
                break;
            }

            if (emuenv.renderer->precompile_requested
                && !emuenv.renderer->precompile_complete.load(std::memory_order_acquire)) {
                continue;
            }

            if (emuenv.kernel.threads.empty()) {
                stop_session(app::AppSessionStopReason::UserRequest);
                if (!gui.vita_area.online_storage && !gui.vita_area.live_area_screen && !gui.vita_area.home_screen)
                    return_to_frontend_after_app();
                break;
            }
        }
    }

    if (session.has_active_session())
        stop_session(app::AppSessionStopReason::FrontendShutdown);
    if (emuenv.renderer)
        emuenv.renderer->frontend_frame_callback = {};
    bgm_player::destroy_bgm_player();
    return Success;
}

} // namespace gui