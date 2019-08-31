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

#include <renderer/gl/state.h>
#include <gui/imgui_impl_sdl_state.h>

struct SDL_Window;
union SDL_Event;

struct ImGui_GLState : public ImGui_State {
    char glsl_version[32] = "#version 150";
    uint32_t font_texture = 0;
    uint32_t shader_handle = 0, vertex_handle = 0, fragment_handle = 0;
    uint32_t attribute_location_tex = 0, attribute_projection_mat = 0;
    uint32_t attribute_position_location = 0, attribute_uv_location = 0, attribute_color_location = 0;
    uint32_t vbo = 0, elements = 0;

    // GL actually never needs the renderer. Putting it here just in case it is needed in the future.
    inline renderer::gl::GLState &get_renderer() {
        return dynamic_cast<renderer::gl::GLState &>(*renderer);
    }
};

IMGUI_API ImGui_GLState *ImGui_ImplSdlGL3_Init(renderer::State *renderer, SDL_Window *window, const char *glsl_version = NULL);
IMGUI_API void ImGui_ImplSdlGL3_Shutdown(ImGui_GLState &state);
IMGUI_API void ImGui_ImplSdlGL3_RenderDrawData(ImGui_GLState &state);

IMGUI_API ImTextureID ImGui_ImplSdlGL3_CreateTexture(void *data, int width, int height);
IMGUI_API void ImGui_ImplSdlGL3_DeleteTexture(ImTextureID texture);

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void ImGui_ImplSdlGL3_InvalidateDeviceObjects(ImGui_GLState &state);
IMGUI_API bool ImGui_ImplSdlGL3_CreateDeviceObjects(ImGui_GLState &state);
