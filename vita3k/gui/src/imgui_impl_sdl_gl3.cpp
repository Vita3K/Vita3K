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

// CHANGELOG
// (minor and older changes stripped away, please see git history for details)
//  2018-03-20: Misc: Setup io.BackendFlags ImGuiBackendFlags_HasMouseCursors flag + honor ImGuiConfigFlags_NoMouseCursorChange flag.
//  2018-03-06: OpenGL: Added const char* glsl_version parameter to ImGui_ImplSdlGL3_Init() so user can override the GLSL version e.g. "#version 150".
//  2018-02-23: OpenGL: Create the VAO in the render function so the setup can more easily be used with multiple shared GL context.
//  2018-02-16: Inputs: Added support for mouse cursors, honoring ImGui::GetMouseCursor() value.
//  2018-02-16: Misc: Obsoleted the io.RenderDrawListsFn callback and exposed ImGui_ImplSdlGL3_RenderDrawData() in the .h file so you can call it yourself.
//  2018-02-06: Misc: Removed call to ImGui::Shutdown() which is not available from 1.60 WIP, user needs to call CreateContext/DestroyContext themselves.
//  2018-02-06: Inputs: Added mapping for ImGuiKey_Space.
//  2018-02-05: Misc: Using SDL_GetPerformanceCounter() instead of SDL_GetTicks() to be able to handle very high framerate (1000+ FPS).
//  2018-02-05: Inputs: Keyboard mapping is using scancodes everywhere instead of a confusing mixture of keycodes and scancodes.
//  2018-01-20: Inputs: Added Horizontal Mouse Wheel support.
//  2018-01-19: Inputs: When available (SDL 2.0.4+) using SDL_CaptureMouse() to retrieve coordinates outside of client area when dragging. Otherwise (SDL 2.0.3 and before) testing for SDL_WINDOW_INPUT_FOCUS instead of SDL_WINDOW_MOUSE_FOCUS.
//  2018-01-18: Inputs: Added mapping for ImGuiKey_Insert.
//  2018-01-07: OpenGL: Changed GLSL shader version from 330 to 150.
//  2017-09-01: OpenGL: Save and restore current bound sampler. Save and restore current polygon mode.
//  2017-08-25: Inputs: MousePos set to -FLT_MAX,-FLT_MAX when mouse is unavailable/missing (instead of -1,-1).
//  2017-05-01: OpenGL: Fixed save and restore of current blend func state.
//  2017-05-01: OpenGL: Fixed save and restore of current GL_ACTIVE_TEXTURE.
//  2016-10-15: Misc: Added a void* user_data parameter to Clipboard function handlers.
//  2016-09-05: OpenGL: Fixed save and restore of current scissor rectangle.
//  2016-07-29: OpenGL: Explicitly setting GL_UNPACK_ROW_LENGTH to reduce issues because SDL changes it. (#752)

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <gui/imgui_impl_sdl.h>
#include <gui/imgui_impl_sdl_gl3.h>

#include <renderer/gl/state.h>
#include <renderer/types.h>

// This is the main rendering function that you have to implement and provide to ImGui (via setting up 'RenderDrawListsFn' in the ImGuiIO structure)
// Note that this implementation is little overcomplicated because we are saving/setting up/restoring every OpenGL state explicitly, in order to be able to run within any OpenGL engine that doesn't do so.
// If text or lines are blurry when integrating ImGui in your engine: in your Render function, try translating your projection matrix by (0.5f,0.5f) or (0.375f,0.375f)
void ImGui_ImplSdlGL3_RenderDrawData(ImGui_GLState &state) {
    ImDrawData *draw_data = ImGui::GetDrawData();

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    ImGuiIO &io = ImGui::GetIO();
    int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (fb_width == 0 || fb_height == 0)
        return;
    draw_data->ScaleClipRects(io.DisplayFramebufferScale);

    // Backup GL state
    GLint last_framebuffer;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &last_framebuffer);
    GLint last_active_texture;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &last_active_texture);
    glActiveTexture(GL_TEXTURE0);
    GLint last_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    GLint last_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    GLint last_sampler;
    glGetIntegerv(GL_SAMPLER_BINDING, &last_sampler);
    GLint last_array_buffer;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
    GLint last_element_array_buffer;
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
    GLint last_vertex_array;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
    GLint last_polygon_mode[2];
    glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
    GLint last_viewport[4];
    glGetIntegerv(GL_VIEWPORT, last_viewport);
    GLint last_scissor_box[4];
    glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
    GLenum last_blend_src_rgb;
    glGetIntegerv(GL_BLEND_SRC_RGB, (GLint *)&last_blend_src_rgb);
    GLenum last_blend_dst_rgb;
    glGetIntegerv(GL_BLEND_DST_RGB, (GLint *)&last_blend_dst_rgb);
    GLenum last_blend_src_alpha;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, (GLint *)&last_blend_src_alpha);
    GLenum last_blend_dst_alpha;
    glGetIntegerv(GL_BLEND_DST_ALPHA, (GLint *)&last_blend_dst_alpha);
    GLenum last_blend_equation_rgb;
    glGetIntegerv(GL_BLEND_EQUATION_RGB, (GLint *)&last_blend_equation_rgb);
    GLenum last_blend_equation_alpha;
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, (GLint *)&last_blend_equation_alpha);
    GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
    GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
    GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
    GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean last_color_mask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, last_color_mask);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, polygon fill
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    if (state.do_clear_screen)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Setup viewport, orthographic projection matrix
    glViewport(0, 0, fb_width, fb_height);
    const float ortho_projection[4][4] = {
        { 2.0f / io.DisplaySize.x, 0.0f, 0.0f, 0.0f },
        { 0.0f, 2.0f / -io.DisplaySize.y, 0.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f, 0.0f },
        { -1.0f, 1.0f, 0.0f, 1.0f },
    };
    glUseProgram(state.shader_handle);
    glUniform1i(state.attribute_location_tex, 0);
    glUniformMatrix4fv(state.attribute_projection_mat, 1, GL_FALSE, &ortho_projection[0][0]);
    glBindSampler(0, 0); // Rely on combined texture/sampler state.

    // Recreate the VAO every time
    // (This is to easily allow multiple GL contexts. VAO are not shared among GL contexts, and we don't track creation/deletion of windows so we don't have an obvious key to use to cache them.)
    GLuint vao_handle = 0;
    glGenVertexArrays(1, &vao_handle);
    glBindVertexArray(vao_handle);
    glBindBuffer(GL_ARRAY_BUFFER, state.vbo);
    glEnableVertexAttribArray(state.attribute_position_location);
    glEnableVertexAttribArray(state.attribute_uv_location);
    glEnableVertexAttribArray(state.attribute_color_location);
    glVertexAttribPointer(state.attribute_position_location, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid *)offsetof(ImDrawVert, pos));
    glVertexAttribPointer(state.attribute_uv_location, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid *)offsetof(ImDrawVert, uv));
    glVertexAttribPointer(state.attribute_color_location, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid *)offsetof(ImDrawVert, col));

    // Draw
    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList *cmd_list = draw_data->CmdLists[n];
        const ImDrawIdx *idx_buffer_offset = 0;

        glBindBuffer(GL_ARRAY_BUFFER, state.vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), (const GLvoid *)cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state.elements);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), (const GLvoid *)cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
            const ImDrawCmd *pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback) {
                pcmd->UserCallback(cmd_list, pcmd);
            } else {
                glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
                glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
                // fix the offset issues as per instructed in release notes of imgui v1.86
                glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer_offset + pcmd->IdxOffset);
            }
        }
    }
    glDeleteVertexArrays(1, &vao_handle);

    // Restore modified GL state
    glUseProgram(last_program);
    glBindFramebuffer(GL_FRAMEBUFFER, last_framebuffer);
    glBindTexture(GL_TEXTURE_2D, last_texture);
    glBindSampler(0, last_sampler);
    glActiveTexture(last_active_texture);
    glBindVertexArray(last_vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
    glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
    glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha);
    if (last_enable_blend)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    if (last_enable_cull_face)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    if (last_enable_depth_test)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (last_enable_scissor_test)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, (GLenum)last_polygon_mode[0]);
    glViewport(last_viewport[0], last_viewport[1], last_viewport[2], last_viewport[3]);
    glScissor(last_scissor_box[0], last_scissor_box[1], last_scissor_box[2], last_scissor_box[3]);
    glColorMask(last_color_mask[0], last_color_mask[1], last_color_mask[2], last_color_mask[3]);
}

static void ImGui_ImplSdlGL3_CreateFontsTexture(ImGui_GLState &state) {
    // Build texture atlas
    ImGuiIO &io = ImGui::GetIO();
    unsigned char *pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height); // Load as RGBA 32-bits for OpenGL3 demo because it is more likely to be compatible with user's existing shader.

    // Upload texture to graphics system
    GLint last_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    glGenTextures(1, &state.font_texture);
    glBindTexture(GL_TEXTURE_2D, state.font_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // Store our identifier
    io.Fonts->TexID = (ImTextureID)(intptr_t)state.font_texture;

    // Restore state
    glBindTexture(GL_TEXTURE_2D, last_texture);
}

bool ImGui_ImplSdlGL3_CreateDeviceObjects(ImGui_GLState &state) {
    // Backup GL state
    GLint last_texture, last_array_buffer, last_vertex_array;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

    const GLchar *vertex_shader = "uniform mat4 ProjMtx;\n"
                                  "in vec2 Position;\n"
                                  "in vec2 UV;\n"
                                  "in vec4 Color;\n"
                                  "out vec2 Frag_UV;\n"
                                  "out vec4 Frag_Color;\n"
                                  "void main()\n"
                                  "{\n"
                                  "	Frag_UV = UV;\n"
                                  "	Frag_Color = Color;\n"
                                  "	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
                                  "}\n";

    const GLchar *fragment_shader = "uniform sampler2D Texture;\n"
                                    "in vec2 Frag_UV;\n"
                                    "in vec4 Frag_Color;\n"
                                    "out vec4 Out_Color;\n"
                                    "void main()\n"
                                    "{\n"
                                    "	Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
                                    "}\n";

    const GLchar *vertex_shader_with_version[2] = { state.glsl_version, vertex_shader };
    const GLchar *fragment_shader_with_version[2] = { state.glsl_version, fragment_shader };

    state.shader_handle = glCreateProgram();
    state.vertex_handle = glCreateShader(GL_VERTEX_SHADER);
    state.fragment_handle = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(state.vertex_handle, 2, vertex_shader_with_version, NULL);
    glShaderSource(state.fragment_handle, 2, fragment_shader_with_version, NULL);
    glCompileShader(state.vertex_handle);
    glCompileShader(state.fragment_handle);
    glAttachShader(state.shader_handle, state.vertex_handle);
    glAttachShader(state.shader_handle, state.fragment_handle);
    glLinkProgram(state.shader_handle);

    state.attribute_location_tex = glGetUniformLocation(state.shader_handle, "Texture");
    state.attribute_projection_mat = glGetUniformLocation(state.shader_handle, "ProjMtx");
    state.attribute_position_location = glGetAttribLocation(state.shader_handle, "Position");
    state.attribute_uv_location = glGetAttribLocation(state.shader_handle, "UV");
    state.attribute_color_location = glGetAttribLocation(state.shader_handle, "Color");

    glGenBuffers(1, &state.vbo);
    glGenBuffers(1, &state.elements);

    ImGui_ImplSdlGL3_CreateFontsTexture(state);

    // Restore modified GL state
    glBindTexture(GL_TEXTURE_2D, last_texture);
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBindVertexArray(last_vertex_array);

    state.init = true;

    return true;
}

void ImGui_ImplSdlGL3_InvalidateDeviceObjects(ImGui_GLState &state) {
    if (state.vbo)
        glDeleteBuffers(1, &state.vbo);
    if (state.elements)
        glDeleteBuffers(1, &state.elements);
    state.vbo = state.elements = 0;

    if (state.shader_handle && state.vertex_handle)
        glDetachShader(state.shader_handle, state.vertex_handle);
    if (state.vertex_handle)
        glDeleteShader(state.vertex_handle);
    state.vertex_handle = 0;

    if (state.shader_handle && state.fragment_handle)
        glDetachShader(state.shader_handle, state.fragment_handle);
    if (state.fragment_handle)
        glDeleteShader(state.fragment_handle);
    state.fragment_handle = 0;

    if (state.shader_handle)
        glDeleteProgram(state.shader_handle);
    state.shader_handle = 0;

    if (state.font_texture) {
        glDeleteTextures(1, &state.font_texture);
        ImGui::GetIO().Fonts->TexID = 0;
        state.font_texture = 0;
    }
}

IMGUI_API ImGui_GLState *ImGui_ImplSdlGL3_Init(renderer::State *renderer, SDL_Window *window, const char *glsl_version) {
    auto *state = new ImGui_GLState;
    state->renderer = renderer;
    state->window = window;

    // Store GL version string so we can refer to it later in case we recreate shaders.
    if (glsl_version == NULL)
        glsl_version = "#version 150";
    IM_ASSERT((int)strlen(glsl_version) + 2 < IM_ARRAYSIZE(state->glsl_version));
    strcpy(state->glsl_version, glsl_version);
    strcat(state->glsl_version, "\n");

    return state;
}

void ImGui_ImplSdlGL3_Shutdown(ImGui_GLState &state) {
    // Destroy OpenGL objects
    ImGui_ImplSdlGL3_InvalidateDeviceObjects(state);
}

IMGUI_API ImTextureID ImGui_ImplSdlGL3_CreateTexture(void *data, int width, int height) {
    GLuint texture;
    glGenTextures(1, &texture);

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(texture));
}

IMGUI_API void ImGui_ImplSdlGL3_DeleteTexture(ImTextureID texture) {
    auto texture_name = static_cast<GLuint>(reinterpret_cast<uintptr_t>(texture));
    glDeleteTextures(1, &texture_name);
}
