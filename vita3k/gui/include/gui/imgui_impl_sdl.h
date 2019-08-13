#pragma once

#include <imgui.h>

#include <host/window.h>

union SDL_Event;
struct SDL_Cursor;

IMGUI_API bool ImGui_ImplSdl_Init(renderer::State *renderer, SDL_Window *window, const std::string &base_path);
IMGUI_API void ImGui_ImplSdl_Shutdown(renderer::State *renderer);
IMGUI_API void ImGui_ImplSdl_NewFrame(renderer::State *renderer, SDL_Window *window);
IMGUI_API void ImGui_ImplSdl_RenderDrawData(renderer::State *renderer, ImDrawData *draw_data);
IMGUI_API bool ImGui_ImplSdl_ProcessEvent(renderer::State *renderer, SDL_Event *event);
IMGUI_API void ImGui_ImplSdl_GetDrawableSize(renderer::State *renderer, SDL_Window *window, int &width, int &height);

IMGUI_API ImTextureID ImGui_ImplSdl_CreateTexture(renderer::State *renderer, void *data, int width, int height);
IMGUI_API void ImGui_ImplSdl_DeleteTexture(renderer::State *renderer, ImTextureID texture);

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdl_InvalidateDeviceObjects(renderer::State *renderer);
IMGUI_API bool ImGui_ImplSdl_CreateDeviceObjects(renderer::State *renderer);

class ImGui_Texture {
    renderer::State *renderer = nullptr;
    ImTextureID texture_id = nullptr;

public:
    ImGui_Texture() = default;
    ImGui_Texture(renderer::State *renderer, void *data, int width, int height);

    void init(renderer::State *state, ImTextureID texture);
    void init(renderer::State *renderer, void *data, int width, int height);

    operator bool();
    operator ImTextureID();
    bool operator==(const ImGui_Texture &texture);

    ~ImGui_Texture();
};

