#include <gui/imgui_impl_sdl.h>
#include <gui/imgui_impl_sdl_gl3.h>
#ifdef USE_VULKAN
#include <gui/imgui_impl_sdl_vulkan.h>
#endif

#include <renderer/state.h>
#include <util/log.h>

#include <SDL.h>
#ifdef USE_VULKAN
#include <SDL_vulkan.h>
#endif

IMGUI_API ImGui_State *ImGui_ImplSdl_Init(renderer::State *renderer, SDL_Window *window, const std::string &base_path) {
    switch (renderer->current_backend) {
    case renderer::Backend::OpenGL:
        return dynamic_cast<ImGui_State *>(ImGui_ImplSdlGL3_Init(renderer, window, nullptr));
#ifdef USE_VULKAN
    case renderer::Backend::Vulkan:
        return dynamic_cast<ImGui_State *>(ImGui_ImplSdlVulkan_Init(renderer, window, base_path));
#endif
    default:
        LOG_ERROR("Missing ImGui init for backend {}.", static_cast<int>(renderer->current_backend));
        return nullptr;
    }
}
IMGUI_API void ImGui_ImplSdl_Shutdown(ImGui_State *state) {
    switch (state->renderer->current_backend) {
    case renderer::Backend::OpenGL:
        return ImGui_ImplSdlGL3_Shutdown(dynamic_cast<ImGui_GLState &>(*state));
#ifdef USE_VULKAN
    case renderer::Backend::Vulkan:
        return ImGui_ImplSdlVulkan_Shutdown(dynamic_cast<ImGui_VulkanState &>(*state));
#endif
    default:
        LOG_ERROR("Missing ImGui init for backend {}.", static_cast<int>(state->renderer->current_backend));
    }
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
    io.DeltaTime = state->time > 0 ? (float)((double)(current_time - state->time) / frequency) : (float)(1.0f / 60.0f);
    state->time = current_time;

    // Setup mouse inputs (we already got mouse wheel, keyboard keys & characters from our event handler)
    int mx, my;
    Uint32 mouse_buttons = SDL_GetMouseState(&mx, &my);
    io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
    io.MouseDown[0] = state->mouse_pressed[0] || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0; // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
    io.MouseDown[1] = state->mouse_pressed[1] || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    io.MouseDown[2] = state->mouse_pressed[2] || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
    state->mouse_pressed[0] = state->mouse_pressed[1] = state->mouse_pressed[2] = false;

// We need to use SDL_CaptureMouse() to easily retrieve mouse coordinates outside of the client area. This is only supported from SDL 2.0.4 (released Jan 2016)
#if (SDL_MAJOR_VERSION >= 2) && (SDL_MINOR_VERSION >= 0) && (SDL_PATCHLEVEL >= 4)
    if ((SDL_GetWindowFlags(state->window) & (SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_MOUSE_CAPTURE)) != 0)
        io.MousePos = ImVec2((float)mx, (float)my);
    bool any_mouse_button_down = false;
    for (int n = 0; n < IM_ARRAYSIZE(io.MouseDown); n++)
        any_mouse_button_down |= io.MouseDown[n];
    if (any_mouse_button_down && (SDL_GetWindowFlags(state->window) & SDL_WINDOW_MOUSE_CAPTURE) == 0)
        SDL_CaptureMouse(SDL_TRUE);
    if (!any_mouse_button_down && (SDL_GetWindowFlags(state->window) & SDL_WINDOW_MOUSE_CAPTURE) != 0)
        SDL_CaptureMouse(SDL_FALSE);
#else
    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0)
        io.MousePos = ImVec2((float)mx, (float)my);
#endif

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
#ifdef USE_VULKAN
    case renderer::Backend::Vulkan:
        return ImGui_ImplSdlVulkan_RenderDrawData(dynamic_cast<ImGui_VulkanState &>(*state));
#endif
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
#ifdef USE_VULKAN
    case renderer::Backend::Vulkan:
        SDL_Vulkan_GetDrawableSize(state->window, &width, &height);
        break;
#endif
    default:
        LOG_ERROR("Missing ImGui init for backend {}.", static_cast<int>(state->renderer->current_backend));
    }
}

IMGUI_API ImTextureID ImGui_ImplSdl_CreateTexture(ImGui_State *state, void *data, int width, int height) {
    switch (state->renderer->current_backend) {
    case renderer::Backend::OpenGL:
        return ImGui_ImplSdlGL3_CreateTexture(data, width, height);
#ifdef USE_VULKAN
    case renderer::Backend::Vulkan:
        return ImGui_ImplSdlVulkan_CreateTexture(dynamic_cast<ImGui_VulkanState &>(*state), data, width, height);
#endif
    default:
        LOG_ERROR("Missing ImGui init for backend {}.", static_cast<int>(state->renderer->current_backend));
    }
}

IMGUI_API void ImGui_ImplSdl_DeleteTexture(ImGui_State *state, ImTextureID texture) {
    switch (state->renderer->current_backend) {
    case renderer::Backend::OpenGL:
        return ImGui_ImplSdlGL3_DeleteTexture(texture);
#ifdef USE_VULKAN
    case renderer::Backend::Vulkan:
        return ImGui_ImplSdlVulkan_DeleteTexture(dynamic_cast<ImGui_VulkanState &>(*state), texture);
#endif
    default:
        LOG_ERROR("Missing ImGui init for backend {}.", static_cast<int>(state->renderer->current_backend));
    }
}

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdl_InvalidateDeviceObjects(ImGui_State *state) {
    switch (state->renderer->current_backend) {
    case renderer::Backend::OpenGL:
        return ImGui_ImplSdlGL3_InvalidateDeviceObjects(dynamic_cast<ImGui_GLState &>(*state));
#ifdef USE_VULKAN
    case renderer::Backend::Vulkan:
        return ImGui_ImplSdlVulkan_InvalidateDeviceObjects(dynamic_cast<ImGui_VulkanState &>(*state));
#endif
    default:
        LOG_ERROR("Missing ImGui init for backend {}.", static_cast<int>(state->renderer->current_backend));
    }
}
IMGUI_API bool ImGui_ImplSdl_CreateDeviceObjects(ImGui_State *state) {
    switch (state->renderer->current_backend) {
    case renderer::Backend::OpenGL:
        return ImGui_ImplSdlGL3_CreateDeviceObjects(dynamic_cast<ImGui_GLState &>(*state));
#ifdef USE_VULKAN
    case renderer::Backend::Vulkan:
        return ImGui_ImplSdlVulkan_CreateDeviceObjects(dynamic_cast<ImGui_VulkanState &>(*state));
#endif
    default:
        LOG_ERROR("Missing ImGui init for backend {}.", static_cast<int>(state->renderer->current_backend));
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

ImGui_Texture::operator bool() {
    return texture_id != nullptr;
}

ImGui_Texture::operator ImTextureID() {
    return texture_id;
}

bool ImGui_Texture::operator==(const ImGui_Texture &texture) {
    return texture_id == texture.texture_id;
}

ImGui_Texture::ImGui_Texture(ImGui_State *new_state, void *data, int width, int height) {
    init(new_state, data, width, height);
}

ImGui_Texture::~ImGui_Texture() {
    if (texture_id)
        ImGui_ImplSdl_DeleteTexture(state, texture_id);
}
