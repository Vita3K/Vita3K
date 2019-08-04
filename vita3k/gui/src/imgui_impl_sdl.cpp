#include <gui/imgui_impl_sdl.h>
#include <gui/imgui_impl_sdl_gl3.h>
#include <gui/imgui_impl_sdl_vulkan.h>

#include <renderer/state.h>
#include <renderer/vulkan/state.h>

#include <SDL.h>

// SDL data
uint64_t g_Time = 0;
bool g_MousePressed[3] = { false, false, false };
SDL_Cursor *g_MouseCursors[ImGuiMouseCursor_COUNT] = { 0 };

IMGUI_API bool ImGui_ImplSdl_Init(RendererPtr &renderer, SDL_Window *window, const std::string &base_path) {
    switch (renderer->current_backend) {
    case renderer::Backend::OpenGL:
        return ImGui_ImplSdlGL3_Init(window, nullptr);
    case renderer::Backend::Vulkan:
        return ImGui_ImplSdlVulkan_Init(renderer, window, base_path);
    }
}
IMGUI_API void ImGui_ImplSdl_Shutdown(RendererPtr &renderer) {
    switch (renderer->current_backend) {
        case renderer::Backend::OpenGL:
            return ImGui_ImplSdlGL3_Shutdown();
        case renderer::Backend::Vulkan:
            return ImGui_ImplSdlVulkan_Shutdown(renderer);
    }
}
IMGUI_API void ImGui_ImplSdl_NewFrame(RendererPtr &renderer, SDL_Window *window) {
    switch (renderer->current_backend) {
        case renderer::Backend::OpenGL:
            return ImGui_ImplSdlGL3_NewFrame(window);
        case renderer::Backend::Vulkan:
            return ImGui_ImplSdlVulkan_NewFrame(renderer, window);
    }
}
IMGUI_API void ImGui_ImplSdl_RenderDrawData(RendererPtr &renderer, ImDrawData *draw_data) {
    switch (renderer->current_backend) {
        case renderer::Backend::OpenGL:
            return ImGui_ImplSdlGL3_RenderDrawData(draw_data);
        case renderer::Backend::Vulkan:
            return ImGui_ImplSdlVulkan_RenderDrawData(renderer, draw_data);
    }
}

// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
bool ImGui_ImplSdl_ProcessEvent(SDL_Event *event) {
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
                g_MousePressed[0] = true;
            if (event->button.button == SDL_BUTTON_RIGHT)
                g_MousePressed[1] = true;
            if (event->button.button == SDL_BUTTON_MIDDLE)
                g_MousePressed[2] = true;
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

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdl_InvalidateDeviceObjects(RendererPtr &renderer) {
    switch (renderer->current_backend) {
        case renderer::Backend::OpenGL:
            return ImGui_ImplSdlGL3_InvalidateDeviceObjects();
        case renderer::Backend::Vulkan:
            return ImGui_ImplSdlVulkan_InvalidateDeviceObjects(renderer);
    }
}
IMGUI_API bool ImGui_ImplSdl_CreateDeviceObjects(RendererPtr &renderer) {
    switch (renderer->current_backend) {
        case renderer::Backend::OpenGL:
            return ImGui_ImplSdlGL3_CreateDeviceObjects();
        case renderer::Backend::Vulkan:
            return ImGui_ImplSdlVulkan_CreateDeviceObjects(renderer);
    }
}
