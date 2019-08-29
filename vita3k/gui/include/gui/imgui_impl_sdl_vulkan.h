// Temporary Vulkan ImGui Implementation

#pragma once

#include <imgui.h>

#include <host/window.h>

typedef union SDL_Event SDL_Event;

IMGUI_API bool ImGui_ImplSdlVulkan_Init(renderer::State *renderer, const std::string &base_path);
IMGUI_API void ImGui_ImplSdlVulkan_Shutdown(renderer::State *renderer);
IMGUI_API void ImGui_ImplSdlVulkan_RenderDrawData(renderer::State *renderer);

IMGUI_API ImTextureID ImGui_ImplSdlVulkan_CreateTexture(renderer::State *renderer, void *data, int width, int height);
IMGUI_API void ImGui_ImplSdlVulkan_DeleteTexture(renderer::State *renderer, ImTextureID texture);

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdlVulkan_InvalidateDeviceObjects(renderer::State *renderer);
IMGUI_API bool ImGui_ImplSdlVulkan_CreateDeviceObjects(renderer::State *renderer);
