// Temporary Vulkan ImGui Implementation

#pragma once

#include <imgui.h>

#include <host/window.h>

typedef union SDL_Event SDL_Event;

IMGUI_API bool ImGui_ImplSdlVulkan_Init(RendererPtr &renderer, SDL_Window *window, const std::string &base_path);
IMGUI_API void ImGui_ImplSdlVulkan_Shutdown(RendererPtr &renderer);
IMGUI_API void ImGui_ImplSdlVulkan_NewFrame(RendererPtr &renderer, SDL_Window *window);
IMGUI_API void ImGui_ImplSdlVulkan_RenderDrawData(RendererPtr &renderer, ImDrawData *draw_data);

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdlVulkan_InvalidateDeviceObjects(RendererPtr &renderer);
IMGUI_API bool ImGui_ImplSdlVulkan_CreateDeviceObjects(RendererPtr &renderer);
