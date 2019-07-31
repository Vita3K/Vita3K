#include <gui/imgui_impl_sdl.h>
#include <gui/imgui_impl_sdl_gl3.h>
#include <gui/imgui_impl_sdl_vulkan.h>

#include <renderer/state.h>
#include <renderer/vulkan/state.h>

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
IMGUI_API bool ImGui_ImplSdl_ProcessEvent(RendererPtr &renderer, SDL_Event *event) {
    switch (renderer->current_backend) {
        case renderer::Backend::OpenGL:
            return ImGui_ImplSdlGL3_ProcessEvent(event);
        case renderer::Backend::Vulkan:
            return ImGui_ImplSdlVulkan_ProcessEvent(renderer, event);
    }
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
