// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_vulkan.h>

static const char *ImGui_ImplSdl_GetClipboardText(ImGuiContext *) {
    return SDL_GetClipboardText();
}

static void ImGui_ImplSdl_SetClipboardText(ImGuiContext *, const char *text) {
    SDL_SetClipboardText(text);
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

    // Setup back-end capabilities flags
    ImGuiIO &io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors; // We can honor GetMouseCursor() values (optional)

    // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
    io.KeyMap[ImGuiKey_Tab] = SDL_SCANCODE_TAB;
    io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
    io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
    io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
    io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
    io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
    io.KeyMap[ImGuiKey_Insert] = SDL_SCANCODE_INSERT;
    io.KeyMap[ImGuiKey_Delete] = SDL_SCANCODE_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = SDL_SCANCODE_BACKSPACE;
    io.KeyMap[ImGuiKey_Space] = SDL_SCANCODE_SPACE;
    io.KeyMap[ImGuiKey_Enter] = SDL_SCANCODE_RETURN;
    io.KeyMap[ImGuiKey_Escape] = SDL_SCANCODE_ESCAPE;
    io.KeyMap[ImGuiKey_A] = SDL_SCANCODE_A;
    io.KeyMap[ImGuiKey_C] = SDL_SCANCODE_C;
    io.KeyMap[ImGuiKey_V] = SDL_SCANCODE_V;
    io.KeyMap[ImGuiKey_X] = SDL_SCANCODE_X;
    io.KeyMap[ImGuiKey_Y] = SDL_SCANCODE_Y;
    io.KeyMap[ImGuiKey_Z] = SDL_SCANCODE_Z;

    ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_SetClipboardTextFn = ImGui_ImplSdl_SetClipboardText;
    platform_io.Platform_GetClipboardTextFn = ImGui_ImplSdl_GetClipboardText;
    platform_io.Platform_ClipboardUserData = nullptr;
    platform_io.Platform_SetImeDataFn = nullptr;

    state->mouse_cursors[ImGuiMouseCursor_Arrow] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    state->mouse_cursors[ImGuiMouseCursor_TextInput] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
    state->mouse_cursors[ImGuiMouseCursor_ResizeAll] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
    state->mouse_cursors[ImGuiMouseCursor_ResizeNS] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
    state->mouse_cursors[ImGuiMouseCursor_ResizeEW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    state->mouse_cursors[ImGuiMouseCursor_ResizeNESW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
    state->mouse_cursors[ImGuiMouseCursor_ResizeNWSE] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
    state->mouse_cursors[ImGuiMouseCursor_Hand] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    state->mouse_cursors[ImGuiMouseCursor_NotAllowed] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);

    // TODO: is this needed/useful ?
    // Set platform dependent data in viewport
    // Our mouse update function expect PlatformHandle to be filled for the main viewport
    ImGuiViewport *main_viewport = ImGui::GetMainViewport();
    main_viewport->PlatformHandle = (void *)window;
    main_viewport->PlatformHandleRaw = nullptr;
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info)) {
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
        main_viewport->PlatformHandleRaw = (void *)info.info.win.window;
#elif defined(__APPLE__) && defined(SDL_VIDEO_DRIVER_COCOA)
        main_viewport->PlatformHandleRaw = (void *)info.info.cocoa.window;
#endif
    }

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

    // Destroy SDL mouse cursors
    for (auto &mouse_cursor : state->mouse_cursors)
        SDL_FreeCursor(mouse_cursor);
    memset(state->mouse_cursors, 0, sizeof(state->mouse_cursors));
}
IMGUI_API void ImGui_ImplSdl_NewFrame(ImGui_State *state) {
    ImGuiIO &io = ImGui::GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    int w, h;
    int display_w, display_h;
    SDL_GetWindowSize(state->window, &w, &h);
    ImGui_ImplSdl_GetDrawableSize(state, display_w, display_h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);

    // Setup time step (we don't use SDL_GetTicks() because it is using millisecond resolution)
    static Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 current_time = SDL_GetPerformanceCounter();
    io.DeltaTime = state->time > 0 ? (float)((double)(current_time - state->time) / frequency) : (1.0f / 60.0f);
    state->time = current_time;

    // Setup mouse inputs (we already got mouse wheel, keyboard keys & characters from our event handler)
    int mx, my;
    Uint32 mouse_buttons = SDL_GetMouseState(&mx, &my);
    io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
    io.MouseDown[0] = state->mouse_pressed[0] || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0; // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
    io.MouseDown[1] = state->mouse_pressed[1] || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    io.MouseDown[2] = state->mouse_pressed[2] || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
    state->mouse_pressed[0] = state->mouse_pressed[1] = state->mouse_pressed[2] = false;

    if ((SDL_GetWindowFlags(state->window) & SDL_WINDOW_INPUT_FOCUS) != 0)
        io.MousePos = ImVec2((float)mx, (float)my);

    // Update OS/hardware mouse cursor if imgui isn't drawing a software cursor
    if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) == 0) {
        ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
        if (io.MouseDrawCursor || cursor == ImGuiMouseCursor_None) {
            SDL_ShowCursor(0);
        } else {
            SDL_SetCursor(state->mouse_cursors[cursor] ? state->mouse_cursors[cursor] : state->mouse_cursors[ImGuiMouseCursor_Arrow]);
            SDL_ShowCursor(1);
        }
    }

    // Start the frame. This call will update the io.WantCaptureMouse, io.WantCaptureKeyboard flag that you can use to dispatch inputs (or not) to your application.
    ImGui::NewFrame();
}
IMGUI_API void ImGui_ImplSdl_RenderDrawData(ImGui_State *state) {
    switch (state->renderer->current_backend) {
    case renderer::Backend::OpenGL:
        return ImGui_ImplSdlGL3_RenderDrawData(dynamic_cast<ImGui_GLState &>(*state));

    case renderer::Backend::Vulkan:
        return ImGui_ImplSdlVulkan_RenderDrawData(dynamic_cast<ImGui_VulkanState &>(*state));
    }
}

// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
bool ImGui_ImplSdl_ProcessEvent(ImGui_State *state, SDL_Event *event) {
    ImGuiIO &io = ImGui::GetIO();
    switch (event->type) {
    case SDL_MOUSEWHEEL: {
        if (event->wheel.x > 0)
            io.MouseWheelH += 1;
        if (event->wheel.x < 0)
            io.MouseWheelH -= 1;
        if (event->wheel.y > 0)
            io.MouseWheel += 1;
        if (event->wheel.y < 0)
            io.MouseWheel -= 1;
        return true;
    }
    case SDL_MOUSEBUTTONDOWN: {
        if (event->button.button == SDL_BUTTON_LEFT)
            state->mouse_pressed[0] = true;
        if (event->button.button == SDL_BUTTON_RIGHT)
            state->mouse_pressed[1] = true;
        if (event->button.button == SDL_BUTTON_MIDDLE)
            state->mouse_pressed[2] = true;
        return true;
    }
    case SDL_TEXTINPUT: {
        io.AddInputCharactersUTF8(event->text.text);
        return true;
    }
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
        int key = event->key.keysym.scancode;
        IM_ASSERT(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown));
        io.KeysDown[key] = (event->type == SDL_KEYDOWN);
        io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
        io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
        io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
        io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
        return true;
    }
    }
    return false;
}

IMGUI_API void ImGui_ImplSdl_GetDrawableSize(ImGui_State *state, int &width, int &height) {
    // Does this even matter?
    switch (state->renderer->current_backend) {
    case renderer::Backend::OpenGL:
        SDL_GL_GetDrawableSize(state->window, &width, &height);
        break;

    case renderer::Backend::Vulkan:
        SDL_Vulkan_GetDrawableSize(state->window, &width, &height);
        break;

    default:
        LOG_ERROR("Missing ImGui init for backend {}.", static_cast<int>(state->renderer->current_backend));
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
