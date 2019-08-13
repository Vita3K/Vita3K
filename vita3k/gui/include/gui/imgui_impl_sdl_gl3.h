// ImGui SDL2 binding with OpenGL3
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan graphics context creation, etc.)
// (GL3W is a helper library to access OpenGL functions since there is no standard header to access modern OpenGL functions easily. Alternatives are GLEW, Glad, etc.)

// Implemented features:
//  [X] User texture binding. Cast 'GLuint' OpenGL texture identifier as void*/ImTextureID. Read the FAQ about ImTextureID in imgui.cpp.
// Missing features:
//  [ ] SDL2 handling of IME under Windows appears to be broken and it explicitly disable the regular Windows IME. You can restore Windows IME by compiling SDL with SDL_DISABLE_WINDOWS_IME.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#pragma once

#include <imgui.h>

struct SDL_Window;
union SDL_Event;

IMGUI_API bool ImGui_ImplSdlGL3_Init(renderer::State *renderer, SDL_Window *window, const char *glsl_version = NULL);
IMGUI_API void ImGui_ImplSdlGL3_Shutdown(renderer::State *renderer);
IMGUI_API void ImGui_ImplSdlGL3_RenderDrawData(renderer::State *renderer, ImDrawData *draw_data);
IMGUI_API void ImGui_ImplSdlGL3_GetDrawableSize(SDL_Window *window, int &width, int &height);

IMGUI_API ImTextureID ImGui_ImplSdlGL3_CreateTexture(renderer::State *renderer, void *data, int width, int height);
IMGUI_API void ImGui_ImplSdlGL3_DeleteTexture(renderer::State *renderer, ImTextureID texture);

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdlGL3_InvalidateDeviceObjects(renderer::State *renderer);
IMGUI_API bool ImGui_ImplSdlGL3_CreateDeviceObjects(renderer::State *renderer);
