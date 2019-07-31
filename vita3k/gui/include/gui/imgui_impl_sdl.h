#pragma once

#include <imgui.h>

#include <host/window.h>

typedef union SDL_Event SDL_Event;

IMGUI_API bool ImGui_ImplSdl_Init(RendererPtr &renderer, SDL_Window *window, const std::string &base_path);
IMGUI_API void ImGui_ImplSdl_Shutdown(RendererPtr &renderer);
IMGUI_API void ImGui_ImplSdl_NewFrame(RendererPtr &renderer, SDL_Window *window);
IMGUI_API void ImGui_ImplSdl_RenderDrawData(RendererPtr &renderer, ImDrawData *draw_data);
IMGUI_API bool ImGui_ImplSdl_ProcessEvent(RendererPtr &renderer, SDL_Event *event);

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdl_InvalidateDeviceObjects(RendererPtr &renderer);
IMGUI_API bool ImGui_ImplSdl_CreateDeviceObjects(RendererPtr &renderer);
