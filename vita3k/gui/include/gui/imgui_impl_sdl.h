#pragma once

#include <imgui.h>

#include <gui/imgui_impl_sdl_state.h>
#include <host/window.h>

#include <string>

union SDL_Event;
struct SDL_Window;

IMGUI_API ImGui_State *ImGui_ImplSdl_Init(renderer::State *renderer, SDL_Window *window, const std::string &base_path);
IMGUI_API void ImGui_ImplSdl_Shutdown(ImGui_State *state);
IMGUI_API void ImGui_ImplSdl_NewFrame(ImGui_State *state);
IMGUI_API void ImGui_ImplSdl_RenderDrawData(ImGui_State *state);
IMGUI_API bool ImGui_ImplSdl_ProcessEvent(ImGui_State *state, SDL_Event *event);
IMGUI_API void ImGui_ImplSdl_GetDrawableSize(ImGui_State *state, int &width, int &height);

IMGUI_API ImTextureID ImGui_ImplSdl_CreateTexture(ImGui_State *state, void *data, int width, int height);
IMGUI_API void ImGui_ImplSdl_DeleteTexture(ImGui_State *state, ImTextureID texture);

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdl_InvalidateDeviceObjects(ImGui_State *state);
IMGUI_API bool ImGui_ImplSdl_CreateDeviceObjects(ImGui_State *state);
