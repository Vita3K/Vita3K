#include <gui/imgui_impl_sdl.h>
#include <gui/imgui_impl_sdl_gl3.h>
#include <gui/imgui_impl_sdl_vulkan.h>

#include <renderer/state.h>

#include <SDL.h>

IMGUI_API bool ImGui_ImplSdl_Init(RendererPtr &renderer, SDL_Window *window, const std::string &base_path) {
    switch (renderer->current_backend) {
    case renderer::Backend::OpenGL:
        return ImGui_ImplSdlGL3_Init(renderer, window, nullptr);
    case renderer::Backend::Vulkan:
        return ImGui_ImplSdlVulkan_Init(renderer, window, base_path);
    }
}
IMGUI_API void ImGui_ImplSdl_Shutdown(RendererPtr &renderer) {
    switch (renderer->current_backend) {
        case renderer::Backend::OpenGL:
            return ImGui_ImplSdlGL3_Shutdown(renderer);
        case renderer::Backend::Vulkan:
            return ImGui_ImplSdlVulkan_Shutdown(renderer);
    }
}
IMGUI_API void ImGui_ImplSdl_NewFrame(RendererPtr &renderer, SDL_Window *window) {
    if (!renderer->gui.init)
        ImGui_ImplSdl_CreateDeviceObjects(renderer);

    ImGuiIO &io = ImGui::GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    int w, h;
    int display_w, display_h;
    SDL_GetWindowSize(window, &w, &h);
    SDL_GL_GetDrawableSize(window, &display_w, &display_h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);

    // Setup time step (we don't use SDL_GetTicks() because it is using millisecond resolution)
    static Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 current_time = SDL_GetPerformanceCounter();
    io.DeltaTime = renderer->gui.time > 0 ? (float)((double)(current_time - renderer->gui.time) / frequency) : (float)(1.0f / 60.0f);
    renderer->gui.time = current_time;

    // Setup mouse inputs (we already got mouse wheel, keyboard keys & characters from our event handler)
    int mx, my;
    Uint32 mouse_buttons = SDL_GetMouseState(&mx, &my);
    io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
    io.MouseDown[0] = renderer->gui.mouse_pressed[0] || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0; // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
    io.MouseDown[1] = renderer->gui.mouse_pressed[1] || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
    io.MouseDown[2] = renderer->gui.mouse_pressed[2] || (mouse_buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
    renderer->gui.mouse_pressed[0] = renderer->gui.mouse_pressed[1] = renderer->gui.mouse_pressed[2] = false;

// We need to use SDL_CaptureMouse() to easily retrieve mouse coordinates outside of the client area. This is only supported from SDL 2.0.4 (released Jan 2016)
#if (SDL_MAJOR_VERSION >= 2) && (SDL_MINOR_VERSION >= 0) && (SDL_PATCHLEVEL >= 4)
    if ((SDL_GetWindowFlags(window) & (SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_MOUSE_CAPTURE)) != 0)
        io.MousePos = ImVec2((float)mx, (float)my);
    bool any_mouse_button_down = false;
    for (int n = 0; n < IM_ARRAYSIZE(io.MouseDown); n++)
        any_mouse_button_down |= io.MouseDown[n];
    if (any_mouse_button_down && (SDL_GetWindowFlags(window) & SDL_WINDOW_MOUSE_CAPTURE) == 0)
        SDL_CaptureMouse(SDL_TRUE);
    if (!any_mouse_button_down && (SDL_GetWindowFlags(window) & SDL_WINDOW_MOUSE_CAPTURE) != 0)
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
            SDL_SetCursor(renderer->gui.mouse_cursors[cursor] ? renderer->gui.mouse_cursors[cursor] : renderer->gui.mouse_cursors[ImGuiMouseCursor_Arrow]);
            SDL_ShowCursor(1);
        }
    }

    // Start the frame. This call will update the io.WantCaptureMouse, io.WantCaptureKeyboard flag that you can use to dispatch inputs (or not) to your application.
    ImGui::NewFrame();
}
IMGUI_API void ImGui_ImplSdl_RenderDrawData(RendererPtr &renderer, ImDrawData *draw_data) {
    switch (renderer->current_backend) {
        case renderer::Backend::OpenGL:
            return ImGui_ImplSdlGL3_RenderDrawData(renderer, draw_data);
        case renderer::Backend::Vulkan:
            return ImGui_ImplSdlVulkan_RenderDrawData(renderer, draw_data);
    }
}

// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
bool ImGui_ImplSdl_ProcessEvent(RendererPtr &renderer, SDL_Event *event) {
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
                renderer->gui.mouse_pressed[0] = true;
            if (event->button.button == SDL_BUTTON_RIGHT)
                renderer->gui.mouse_pressed[1] = true;
            if (event->button.button == SDL_BUTTON_MIDDLE)
                renderer->gui.mouse_pressed[2] = true;
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

IMGUI_API void ImGui_ImplSdl_GetDrawableSize(RendererPtr &renderer, SDL_Window *window, int &width, int &height) {
    switch (renderer->current_backend) {
        case renderer::Backend::OpenGL:
            ImGui_ImplSdlGL3_GetDrawableSize(window, width, height);
        case renderer::Backend::Vulkan:
            ImGui_ImplSdlVulkan_GetDrawableSize(window, width, height);
    }
}

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdl_InvalidateDeviceObjects(RendererPtr &renderer) {
    switch (renderer->current_backend) {
        case renderer::Backend::OpenGL:
            return ImGui_ImplSdlGL3_InvalidateDeviceObjects(renderer);
        case renderer::Backend::Vulkan:
            return ImGui_ImplSdlVulkan_InvalidateDeviceObjects(renderer);
    }
}
IMGUI_API bool ImGui_ImplSdl_CreateDeviceObjects(RendererPtr &renderer) {
    switch (renderer->current_backend) {
        case renderer::Backend::OpenGL:
            return ImGui_ImplSdlGL3_CreateDeviceObjects(renderer);
        case renderer::Backend::Vulkan:
            return ImGui_ImplSdlVulkan_CreateDeviceObjects(renderer);
    }
}
