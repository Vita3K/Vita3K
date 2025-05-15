// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <gui/imgui_impl_sdl.h>
#include <gui/imgui_impl_sdl_gl3.h>
#include <gui/imgui_impl_sdl_vulkan.h>

#include <renderer/state.h>
#include <util/log.h>

#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <imgui_internal.h>

#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__) && !(defined(__APPLE__) && TARGET_OS_IOS) && !defined(__amigaos4__)
#define SDL_HAS_CAPTURE_AND_GLOBAL_MOUSE 1
#else
#define SDL_HAS_CAPTURE_AND_GLOBAL_MOUSE 0
#endif

static char *clipboard_text_data = nullptr;
static const char *ImGui_ImplSdl_GetClipboardText(ImGuiContext *) {
    if (clipboard_text_data)
        SDL_free(clipboard_text_data);
    clipboard_text_data = SDL_GetClipboardText();
    return clipboard_text_data;
}

static void ImGui_ImplSdl_SetClipboardText(ImGuiContext *, const char *text) {
    SDL_SetClipboardText(text);
}

static ImGuiKey ImGui_ImplSDL3_KeycodeToImGuiKey(SDL_Keycode keycode) {
    switch (keycode) {
    case SDLK_TAB: return ImGuiKey_Tab;
    case SDLK_LEFT: return ImGuiKey_LeftArrow;
    case SDLK_RIGHT: return ImGuiKey_RightArrow;
    case SDLK_UP: return ImGuiKey_UpArrow;
    case SDLK_DOWN: return ImGuiKey_DownArrow;
    case SDLK_PAGEUP: return ImGuiKey_PageUp;
    case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
    case SDLK_HOME: return ImGuiKey_Home;
    case SDLK_END: return ImGuiKey_End;
    case SDLK_INSERT: return ImGuiKey_Insert;
    case SDLK_DELETE: return ImGuiKey_Delete;
    case SDLK_BACKSPACE: return ImGuiKey_Backspace;
    case SDLK_SPACE: return ImGuiKey_Space;
    case SDLK_RETURN: return ImGuiKey_Enter;
    case SDLK_ESCAPE: return ImGuiKey_Escape;
    case SDLK_APOSTROPHE: return ImGuiKey_Apostrophe;
    case SDLK_COMMA: return ImGuiKey_Comma;
    case SDLK_MINUS: return ImGuiKey_Minus;
    case SDLK_PERIOD: return ImGuiKey_Period;
    case SDLK_SLASH: return ImGuiKey_Slash;
    case SDLK_SEMICOLON: return ImGuiKey_Semicolon;
    case SDLK_EQUALS: return ImGuiKey_Equal;
    case SDLK_LEFTBRACKET: return ImGuiKey_LeftBracket;
    case SDLK_BACKSLASH: return ImGuiKey_Backslash;
    case SDLK_RIGHTBRACKET: return ImGuiKey_RightBracket;
    case SDLK_GRAVE: return ImGuiKey_GraveAccent;
    case SDLK_CAPSLOCK: return ImGuiKey_CapsLock;
    case SDLK_SCROLLLOCK: return ImGuiKey_ScrollLock;
    case SDLK_NUMLOCKCLEAR: return ImGuiKey_NumLock;
    case SDLK_PRINTSCREEN: return ImGuiKey_PrintScreen;
    case SDLK_PAUSE: return ImGuiKey_Pause;
    case SDLK_KP_0: return ImGuiKey_Keypad0;
    case SDLK_KP_1: return ImGuiKey_Keypad1;
    case SDLK_KP_2: return ImGuiKey_Keypad2;
    case SDLK_KP_3: return ImGuiKey_Keypad3;
    case SDLK_KP_4: return ImGuiKey_Keypad4;
    case SDLK_KP_5: return ImGuiKey_Keypad5;
    case SDLK_KP_6: return ImGuiKey_Keypad6;
    case SDLK_KP_7: return ImGuiKey_Keypad7;
    case SDLK_KP_8: return ImGuiKey_Keypad8;
    case SDLK_KP_9: return ImGuiKey_Keypad9;
    case SDLK_KP_PERIOD: return ImGuiKey_KeypadDecimal;
    case SDLK_KP_DIVIDE: return ImGuiKey_KeypadDivide;
    case SDLK_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
    case SDLK_KP_MINUS: return ImGuiKey_KeypadSubtract;
    case SDLK_KP_PLUS: return ImGuiKey_KeypadAdd;
    case SDLK_KP_ENTER: return ImGuiKey_KeypadEnter;
    case SDLK_KP_EQUALS: return ImGuiKey_KeypadEqual;
    case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
    case SDLK_LSHIFT: return ImGuiKey_LeftShift;
    case SDLK_LALT: return ImGuiKey_LeftAlt;
    case SDLK_LGUI: return ImGuiKey_LeftSuper;
    case SDLK_RCTRL: return ImGuiKey_RightCtrl;
    case SDLK_RSHIFT: return ImGuiKey_RightShift;
    case SDLK_RALT: return ImGuiKey_RightAlt;
    case SDLK_RGUI: return ImGuiKey_RightSuper;
    case SDLK_APPLICATION: return ImGuiKey_Menu;
    case SDLK_0: return ImGuiKey_0;
    case SDLK_1: return ImGuiKey_1;
    case SDLK_2: return ImGuiKey_2;
    case SDLK_3: return ImGuiKey_3;
    case SDLK_4: return ImGuiKey_4;
    case SDLK_5: return ImGuiKey_5;
    case SDLK_6: return ImGuiKey_6;
    case SDLK_7: return ImGuiKey_7;
    case SDLK_8: return ImGuiKey_8;
    case SDLK_9: return ImGuiKey_9;
    case SDLK_A: return ImGuiKey_A;
    case SDLK_B: return ImGuiKey_B;
    case SDLK_C: return ImGuiKey_C;
    case SDLK_D: return ImGuiKey_D;
    case SDLK_E: return ImGuiKey_E;
    case SDLK_F: return ImGuiKey_F;
    case SDLK_G: return ImGuiKey_G;
    case SDLK_H: return ImGuiKey_H;
    case SDLK_I: return ImGuiKey_I;
    case SDLK_J: return ImGuiKey_J;
    case SDLK_K: return ImGuiKey_K;
    case SDLK_L: return ImGuiKey_L;
    case SDLK_M: return ImGuiKey_M;
    case SDLK_N: return ImGuiKey_N;
    case SDLK_O: return ImGuiKey_O;
    case SDLK_P: return ImGuiKey_P;
    case SDLK_Q: return ImGuiKey_Q;
    case SDLK_R: return ImGuiKey_R;
    case SDLK_S: return ImGuiKey_S;
    case SDLK_T: return ImGuiKey_T;
    case SDLK_U: return ImGuiKey_U;
    case SDLK_V: return ImGuiKey_V;
    case SDLK_W: return ImGuiKey_W;
    case SDLK_X: return ImGuiKey_X;
    case SDLK_Y: return ImGuiKey_Y;
    case SDLK_Z: return ImGuiKey_Z;
    case SDLK_F1: return ImGuiKey_F1;
    case SDLK_F2: return ImGuiKey_F2;
    case SDLK_F3: return ImGuiKey_F3;
    case SDLK_F4: return ImGuiKey_F4;
    case SDLK_F5: return ImGuiKey_F5;
    case SDLK_F6: return ImGuiKey_F6;
    case SDLK_F7: return ImGuiKey_F7;
    case SDLK_F8: return ImGuiKey_F8;
    case SDLK_F9: return ImGuiKey_F9;
    case SDLK_F10: return ImGuiKey_F10;
    case SDLK_F11: return ImGuiKey_F11;
    case SDLK_F12: return ImGuiKey_F12;
    default: return ImGuiKey_None;
    }
}

static void ImGui_ImplSDL3_UpdateKeyModifiers(SDL_Keymod sdl_key_mods) {
    ImGuiIO &io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl, (sdl_key_mods & SDL_KMOD_CTRL) != 0);
    io.AddKeyEvent(ImGuiMod_Shift, (sdl_key_mods & SDL_KMOD_SHIFT) != 0);
    io.AddKeyEvent(ImGuiMod_Alt, (sdl_key_mods & SDL_KMOD_ALT) != 0);
    io.AddKeyEvent(ImGuiMod_Super, (sdl_key_mods & SDL_KMOD_GUI) != 0);
}

// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
bool ImGui_ImplSdl_ProcessEvent(ImGui_State *state, SDL_Event *event) {
    ImGuiIO &io = ImGui::GetIO();
    switch (event->type) {
    case SDL_EVENT_MOUSE_MOTION: {
        io.AddMousePosEvent((float)event->motion.x, (float)event->motion.y);
        return true;
    }
    case SDL_EVENT_MOUSE_WHEEL: {
        float wheel_x = (event->wheel.x > 0) ? 1.0f : (event->wheel.x < 0) ? -1.0f
                                                                           : 0.0f;
        float wheel_y = (event->wheel.y > 0) ? 1.0f : (event->wheel.y < 0) ? -1.0f
                                                                           : 0.0f;
        io.AddMouseWheelEvent(wheel_x, wheel_y);
        return true;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        int mouse_button;
        switch (event->button.button) {
        case SDL_BUTTON_LEFT: mouse_button = 0; break;
        case SDL_BUTTON_RIGHT: mouse_button = 1; break;
        case SDL_BUTTON_MIDDLE: mouse_button = 2; break;
        case SDL_BUTTON_X1: mouse_button = 3; break;
        case SDL_BUTTON_X2: mouse_button = 4; break;
        default: return false;
        }
        io.AddMouseButtonEvent(mouse_button, (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN));
        state->mouse_buttons_down = (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) ? (state->mouse_buttons_down | (1 << mouse_button)) : (state->mouse_buttons_down & ~(1 << mouse_button));
        return true;
    }
    case SDL_EVENT_TEXT_INPUT: {
        io.AddInputCharactersUTF8(event->text.text);
        return true;
    }
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        ImGui_ImplSDL3_UpdateKeyModifiers((SDL_Keymod)event->key.mod);
        ImGuiKey key = ImGui_ImplSDL3_KeycodeToImGuiKey(event->key.key);
        io.AddKeyEvent(key, (event->type == SDL_EVENT_KEY_DOWN));
        io.SetKeyEventNativeData(key, event->key.key, event->key.scancode, event->key.scancode); // To support legacy indexing (<1.87 user code). Legacy backend uses SDLK_*** as indices to IsKeyXXX() functions.
        return true;
    }
        // - When capturing mouse, SDL will send a bunch of conflicting LEAVE/ENTER event on every mouse move, but the final ENTER tends to be right.
        // - However we won't get a correct LEAVE event for a captured window.
        // - In some cases, when detaching a window from main viewport SDL may send SDL_WINDOWEVENT_ENTER one frame too late,
        //   causing SDL_WINDOWEVENT_LEAVE on previous frame to interrupt drag operation by clear mouse position. This is why
        //   we delay process the SDL_WINDOWEVENT_LEAVE events by one frame. See issue #5012 for details.
    case SDL_EVENT_WINDOW_MOUSE_ENTER: {
        state->pending_mouse_leave_frame = 0;
        return true;
    }
    case SDL_EVENT_WINDOW_MOUSE_LEAVE: {
        state->pending_mouse_leave_frame = ImGui::GetFrameCount() + 1;
        return true;
    }
    case SDL_EVENT_WINDOW_FOCUS_GAINED: {
        io.AddFocusEvent(true);
        return true;
    }
    case SDL_EVENT_WINDOW_FOCUS_LOST: {
        io.AddFocusEvent(false);
        return true;
    }
    default: return false;
    }
}

IMGUI_API ImGui_State *ImGui_ImplSdl_Init(renderer::State *renderer, SDL_Window *window) {
    ImGui_State *state;

    switch (renderer->current_backend) {
    case renderer::Backend::OpenGL:
        state = reinterpret_cast<ImGui_State *>(ImGui_ImplSdlGL3_Init(renderer, window, nullptr));
        break;

    case renderer::Backend::Vulkan:
        state = reinterpret_cast<ImGui_State *>(ImGui_ImplSdlVulkan_Init(renderer, window));
        break;

    default:
        LOG_ERROR("Missing ImGui init for backend {}.", static_cast<int>(renderer->current_backend));
        return nullptr;
    }

    // Check and store if we are on a SDL backend that supports global mouse position
    // ("wayland" and "rpi" don't support it, but we chose to use a white-list instead of a black-list)
    bool mouse_can_use_global_state = false;
#if SDL_HAS_CAPTURE_AND_GLOBAL_MOUSE
    const char *sdl_backend = SDL_GetCurrentVideoDriver();
    const char *global_mouse_whitelist[] = { "windows", "cocoa", "x11", "DIVE", "VMAN" };
    for (int n = 0; n < IM_ARRAYSIZE(global_mouse_whitelist); n++)
        if (strncmp(sdl_backend, global_mouse_whitelist[n], strlen(global_mouse_whitelist[n])) == 0)
            mouse_can_use_global_state = true;
#endif

    // Setup back-end capabilities flags
    ImGuiIO &io = ImGui::GetIO();
    io.BackendPlatformName = "imgui_impl_sdl";
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors; // We can honor GetMouseCursor() values (optional)
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos; // We can honor io.WantSetMousePos requests (optional, rarely used)

    state->mouse_can_use_global_state = mouse_can_use_global_state;

    ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_SetClipboardTextFn = ImGui_ImplSdl_SetClipboardText;
    platform_io.Platform_GetClipboardTextFn = ImGui_ImplSdl_GetClipboardText;
    platform_io.Platform_ClipboardUserData = nullptr;
    platform_io.Platform_SetImeDataFn = nullptr;

    // Load mouse cursors
    state->mouse_cursors[ImGuiMouseCursor_Arrow] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    state->mouse_cursors[ImGuiMouseCursor_TextInput] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
    state->mouse_cursors[ImGuiMouseCursor_ResizeAll] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
    state->mouse_cursors[ImGuiMouseCursor_ResizeNS] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);
    state->mouse_cursors[ImGuiMouseCursor_ResizeEW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
    state->mouse_cursors[ImGuiMouseCursor_ResizeNESW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE);
    state->mouse_cursors[ImGuiMouseCursor_ResizeNWSE] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE);
    state->mouse_cursors[ImGuiMouseCursor_Hand] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    state->mouse_cursors[ImGuiMouseCursor_NotAllowed] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED);

    // Set platform dependent data in viewport
    auto viewport = ImGui::GetMainViewport();
    viewport->PlatformHandle = (void *)(intptr_t)SDL_GetWindowID(window);
    viewport->PlatformHandleRaw = nullptr;
#if defined(_WIN32) && !defined(__WINRT__)
    viewport->PlatformHandleRaw = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(__APPLE__) && defined(SDL_VIDEO_DRIVER_COCOA)
    viewport->PlatformHandleRaw = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#endif

    // Set SDL hint to receive mouse click events on window focus, otherwise SDL doesn't emit the event.
    // Without this, when clicking to gain focus, our widgets wouldn't activate even though they showed as hovered.
    // (This is unfortunately a global SDL setting, so enabling it might have a side-effect on your application.
    // It is unlikely to make a difference, but if your app absolutely needs to ignore the initial on-focus click:
    // you can ignore SDL_MOUSEBUTTONDOWN events coming right after a SDL_WINDOWEVENT_FOCUS_GAINED)
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

    return state;
}
IMGUI_API void ImGui_ImplSdl_Shutdown(ImGui_State *state) {
    switch (state->renderer->current_backend) {
    case renderer::Backend::OpenGL:
        return ImGui_ImplSdlGL3_Shutdown(dynamic_cast<ImGui_GLState &>(*state));

    case renderer::Backend::Vulkan:
        return ImGui_ImplSdlVulkan_Shutdown(dynamic_cast<ImGui_VulkanState &>(*state));

    default:
        LOG_ERROR("Missing ImGui init for backend {}.", static_cast<int>(state->renderer->current_backend));
    }

    if (clipboard_text_data)
        SDL_free(clipboard_text_data);
    for (ImGuiMouseCursor cursor_n = 0; cursor_n < ImGuiMouseCursor_COUNT; cursor_n++)
        SDL_DestroyCursor(state->mouse_cursors[cursor_n]);

    ImGuiIO &io = ImGui::GetIO();
    io.BackendPlatformName = nullptr;
}

static void ImGui_ImplSDL3_UpdateMouseData(ImGui_State *state) {
    ImGuiIO &io = ImGui::GetIO();

    // We forward mouse input when hovered or captured (via SDL_MOUSEMOTION) or when focused (below)
#if SDL_HAS_CAPTURE_AND_GLOBAL_MOUSE
    SDL_Window *focused_window = SDL_GetKeyboardFocus();
    const bool is_app_focused = (state->window == focused_window);
#else
    SDL_Window *focused_window = state->window;
    const bool is_app_focused = (SDL_GetWindowFlags(state->window) & SDL_WINDOW_INPUT_FOCUS) != 0; // SDL 2.0.3 and non-windowed systems: single-viewport only
#endif
    if (is_app_focused) {
        // (Optional) Set OS mouse position from Dear ImGui if requested (rarely used, only when io.ConfigNavMoveSetMousePos is enabled by user)
        if (io.WantSetMousePos)
            SDL_WarpMouseInWindow(state->window, io.MousePos.x, io.MousePos.y);

        // (Optional) Fallback to provide mouse position when focused (SDL_EVENT_MOUSE_MOTION already provides this when hovered or captured)
        const bool is_relative_mouse_mode = SDL_GetWindowRelativeMouseMode(state->window);
        if (state->mouse_can_use_global_state && state->mouse_buttons_down == 0 && !is_relative_mouse_mode) {
            // Single-viewport mode: mouse position in client window coordinates (io.MousePos is (0,0) when the mouse is on the upper-left corner of the app window)
            float mouse_x_global, mouse_y_global;
            int window_x, window_y;
            SDL_GetGlobalMouseState(&mouse_x_global, &mouse_y_global);
            SDL_GetWindowPosition(focused_window, &window_x, &window_y);
            io.AddMousePosEvent(mouse_x_global - window_x, mouse_y_global - window_y);
        }
    }
}

static void ImGui_ImplSDL3_UpdateMouseCursor(ImGui_State *state) {
    ImGuiIO &io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
        return;

    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    if (io.MouseDrawCursor || imgui_cursor == ImGuiMouseCursor_None) {
        // Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
        SDL_HideCursor();
    } else {
        // Show OS mouse cursor
        SDL_SetCursor(state->mouse_cursors[imgui_cursor] ? state->mouse_cursors[imgui_cursor] : state->mouse_cursors[ImGuiMouseCursor_Arrow]);
        SDL_ShowCursor();
    }
}

static void ImGui_ImplSDL3_UpdateGamepads(ImGui_State *state) {
    ImGuiIO &io = ImGui::GetIO();
    if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0)
        return;

    // Get gamepad
    io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    SDL_Gamepad *game_controller = SDL_OpenGamepad(0);
    if (!game_controller)
        return;
    io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

// Update gamepad inputs
#define MAP_BUTTON(KEY_NO, BUTTON_NO) \
    io.AddKeyEvent(KEY_NO, SDL_GetGamepadButton(game_controller, BUTTON_NO))

#define MAP_ANALOG(KEY_NO, AXIS_NO, V0, V1)                                                             \
    {                                                                                                   \
        float vn = (float)(SDL_GetGamepadAxis(game_controller, AXIS_NO) - (V0)) / (float)((V1) - (V0)); \
        vn = std::clamp(vn, 0.f, 1.f);                                                                  \
        io.AddKeyAnalogEvent(KEY_NO, vn > 0.1f, vn);                                                    \
    }
    const int thumb_dead_zone = 8000; // SDL_Gamepad.h suggests using this value.
    MAP_BUTTON(ImGuiKey_GamepadStart, SDL_GAMEPAD_BUTTON_START);
    MAP_BUTTON(ImGuiKey_GamepadBack, SDL_GAMEPAD_BUTTON_BACK);
    MAP_BUTTON(ImGuiKey_GamepadFaceDown, SDL_GAMEPAD_BUTTON_SOUTH); // Xbox A, PS Cross
    MAP_BUTTON(ImGuiKey_GamepadFaceRight, SDL_GAMEPAD_BUTTON_EAST); // Xbox B, PS Circle
    MAP_BUTTON(ImGuiKey_GamepadFaceLeft, SDL_GAMEPAD_BUTTON_WEST); // Xbox X, PS Square
    MAP_BUTTON(ImGuiKey_GamepadFaceUp, SDL_GAMEPAD_BUTTON_NORTH); // Xbox Y, PS Triangle
    MAP_BUTTON(ImGuiKey_GamepadDpadLeft, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    MAP_BUTTON(ImGuiKey_GamepadDpadRight, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    MAP_BUTTON(ImGuiKey_GamepadDpadUp, SDL_GAMEPAD_BUTTON_DPAD_UP);
    MAP_BUTTON(ImGuiKey_GamepadDpadDown, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    MAP_BUTTON(ImGuiKey_GamepadL1, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    MAP_BUTTON(ImGuiKey_GamepadR1, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    MAP_ANALOG(ImGuiKey_GamepadL2, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 0.0f, 32767);
    MAP_ANALOG(ImGuiKey_GamepadR2, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0.0f, 32767);
    MAP_BUTTON(ImGuiKey_GamepadL3, SDL_GAMEPAD_BUTTON_LEFT_STICK);
    MAP_BUTTON(ImGuiKey_GamepadR3, SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    MAP_ANALOG(ImGuiKey_GamepadLStickLeft, SDL_GAMEPAD_AXIS_LEFTX, -thumb_dead_zone, -32768);
    MAP_ANALOG(ImGuiKey_GamepadLStickRight, SDL_GAMEPAD_AXIS_LEFTX, +thumb_dead_zone, +32767);
    MAP_ANALOG(ImGuiKey_GamepadLStickUp, SDL_GAMEPAD_AXIS_LEFTY, -thumb_dead_zone, -32768);
    MAP_ANALOG(ImGuiKey_GamepadLStickDown, SDL_GAMEPAD_AXIS_LEFTY, +thumb_dead_zone, +32767);
    MAP_ANALOG(ImGuiKey_GamepadRStickLeft, SDL_GAMEPAD_AXIS_RIGHTX, -thumb_dead_zone, -32768);
    MAP_ANALOG(ImGuiKey_GamepadRStickRight, SDL_GAMEPAD_AXIS_RIGHTX, +thumb_dead_zone, +32767);
    MAP_ANALOG(ImGuiKey_GamepadRStickUp, SDL_GAMEPAD_AXIS_RIGHTY, -thumb_dead_zone, -32768);
    MAP_ANALOG(ImGuiKey_GamepadRStickDown, SDL_GAMEPAD_AXIS_RIGHTY, +thumb_dead_zone, +32767);
#undef MAP_BUTTON
#undef MAP_ANALOG
}

IMGUI_API void ImGui_ImplSdl_NewFrame(ImGui_State *state) {
    ImGuiIO &io = ImGui::GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    int w, h;
    int display_w, display_h;
    SDL_GetWindowSize(state->window, &w, &h);
    SDL_GetWindowSizeInPixels(state->window, &display_w, &display_h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);

    // Setup time step (we don't use SDL_GetTicks() because it is using millisecond resolution)
    static Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 current_time = SDL_GetPerformanceCounter();
    io.DeltaTime = state->time > 0 ? (float)((double)(current_time - state->time) / frequency) : (1.0f / 60.0f);
    state->time = current_time;

    if (state->pending_mouse_leave_frame && state->pending_mouse_leave_frame >= ImGui::GetFrameCount() && state->mouse_buttons_down == 0) {
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        state->pending_mouse_leave_frame = 0;
    }

    ImGui_ImplSDL3_UpdateMouseData(state);
    ImGui_ImplSDL3_UpdateMouseCursor(state);

    // Update game controllers (if enabled and available)
    ImGui_ImplSDL3_UpdateGamepads(state);

    // Start the frame. This call will update the io.WantCaptureMouse, io.WantCaptureKeyboard flag that you can use to dispatch inputs (or not) to your application.
    ImGui::NewFrame();

    if (io.WantTextInput ^ state->is_typing) {
        if (state->is_typing) {
            SDL_StopTextInput(state->window);
        } else {
            SDL_StartTextInput(state->window);
        }
        state->is_typing = io.WantTextInput;
    }
}

IMGUI_API void ImGui_ImplSdl_RenderDrawData(ImGui_State *state) {
    switch (state->renderer->current_backend) {
    case renderer::Backend::OpenGL:
        return ImGui_ImplSdlGL3_RenderDrawData(dynamic_cast<ImGui_GLState &>(*state));

    case renderer::Backend::Vulkan:
        return ImGui_ImplSdlVulkan_RenderDrawData(dynamic_cast<ImGui_VulkanState &>(*state));
    }
}

IMGUI_API ImTextureID ImGui_ImplSdl_CreateTexture(ImGui_State *state, void *data, int width, int height) {
    switch (state->renderer->current_backend) {
    case renderer::Backend::OpenGL:
        return ImGui_ImplSdlGL3_CreateTexture(data, width, height);

    case renderer::Backend::Vulkan:
        return ImGui_ImplSdlVulkan_CreateTexture(dynamic_cast<ImGui_VulkanState &>(*state), data, width, height);

    default:
        LOG_ERROR("Missing ImGui init for backend {}.", static_cast<int>(state->renderer->current_backend));
        return {};
    }
}

IMGUI_API void ImGui_ImplSdl_DeleteTexture(ImGui_State *state, ImTextureID texture) {
    switch (state->renderer->current_backend) {
    case renderer::Backend::OpenGL:
        return ImGui_ImplSdlGL3_DeleteTexture(texture);

    case renderer::Backend::Vulkan:
        return ImGui_ImplSdlVulkan_DeleteTexture(dynamic_cast<ImGui_VulkanState &>(*state), texture);

    default:
        LOG_ERROR("Missing ImGui init for backend {}.", static_cast<int>(state->renderer->current_backend));
    }
}

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdl_InvalidateDeviceObjects(ImGui_State *state) {
    switch (state->renderer->current_backend) {
    case renderer::Backend::OpenGL:
        return ImGui_ImplSdlGL3_InvalidateDeviceObjects(dynamic_cast<ImGui_GLState &>(*state));

    case renderer::Backend::Vulkan:
        return ImGui_ImplSdlVulkan_InvalidateDeviceObjects(dynamic_cast<ImGui_VulkanState &>(*state));

    default:
        LOG_ERROR("Missing ImGui init for backend {}.", static_cast<int>(state->renderer->current_backend));
    }
}
IMGUI_API bool ImGui_ImplSdl_CreateDeviceObjects(ImGui_State *state) {
    switch (state->renderer->current_backend) {
    case renderer::Backend::OpenGL:
        return ImGui_ImplSdlGL3_CreateDeviceObjects(dynamic_cast<ImGui_GLState &>(*state));

    case renderer::Backend::Vulkan:
        return ImGui_ImplSdlVulkan_CreateDeviceObjects(dynamic_cast<ImGui_VulkanState &>(*state));

    default:
        LOG_ERROR("Missing ImGui init for backend {}.", static_cast<int>(state->renderer->current_backend));
        return false;
    }
}

void ImGui_Texture::init(ImGui_State *new_state, ImTextureID texture) {
    assert(!texture_id);

    state = new_state;
    texture_id = texture;
}

void ImGui_Texture::init(ImGui_State *new_state, void *data, int width, int height) {
    init(new_state, ImGui_ImplSdl_CreateTexture(new_state, data, width, height));
}

ImGui_Texture::operator bool() const {
    return texture_id != nullptr;
}

ImGui_Texture::operator ImTextureID() const {
    return texture_id;
}

bool ImGui_Texture::operator==(const ImGui_Texture &texture) {
    return texture_id == texture.texture_id;
}

ImGui_Texture &ImGui_Texture::operator=(ImGui_Texture &&texture) noexcept {
    this->state = texture.state;
    this->texture_id = texture.texture_id;

    texture.state = nullptr;
    texture.texture_id = nullptr;

    return *this;
}

ImGui_Texture::ImGui_Texture(ImGui_State *new_state, void *data, int width, int height) {
    init(new_state, data, width, height);
}

ImGui_Texture::ImGui_Texture(ImGui_Texture &&texture) noexcept
    : state(texture.state)
    , texture_id(texture.texture_id) {
    texture.state = nullptr;
    texture.texture_id = nullptr;
}

ImGui_Texture::~ImGui_Texture() {
    if (texture_id)
        ImGui_ImplSdl_DeleteTexture(state, texture_id);
}
