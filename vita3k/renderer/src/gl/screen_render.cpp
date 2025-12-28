// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <glutil/shader.h>
#include <renderer/gl/screen_render.h>
#include <util/log.h>

namespace renderer::gl {

bool ScreenRenderer::init(const fs::path &static_assets) {
    glGenTextures(1, &m_screen_texture);

    const auto builtin_shaders_path = static_assets / "shaders-builtin/opengl";

    const auto render_main_path_vert = builtin_shaders_path / "render_main.vert";
    const auto render_main_path_frag = builtin_shaders_path / "render_main.frag";
    const auto render_main_path_fxaa_frag = builtin_shaders_path / "render_main_fxaa.frag";

    m_render_shader_nofilter = ::gl::load_shaders(render_main_path_vert, render_main_path_frag);
    m_render_shader_fxaa = ::gl::load_shaders(render_main_path_vert, render_main_path_fxaa_frag);
    if (!m_render_shader_nofilter || !m_render_shader_fxaa) {
        LOG_CRITICAL("Couldn't compile essential shaders for rendering. Exiting");
        return false;
    }

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    static const screen_vertices_t vertex_buffer_data = {
        { { -1.f, -1.f, 0.0f }, { 0.f, 1.f } },
        { { 1.f, -1.f, 0.0f }, { 1.f, 1.f } },
        { { 1.f, 1.f, 0.0f }, { 1.f, 0.f } },
        { { -1.f, 1.f, 0.0f }, { 0.f, 0.f } }
    };

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_DYNAMIC_DRAW);

    const auto &shader = enable_fxaa ? m_render_shader_fxaa : m_render_shader_nofilter;

    GLint posAttrib = glGetAttribLocation(*shader, "position_vertex");
    GLint uvAttrib = glGetAttribLocation(*shader, "uv_vertex");

    // 1st attribute: positions
    glVertexAttribPointer(
        posAttrib, // attribute index
        3, // size
        GL_FLOAT, // type
        GL_FALSE, // normalized?
        screen_vertex_size, // stride
        reinterpret_cast<void *>(0) // array buffer offset
    );
    glEnableVertexAttribArray(posAttrib);

    // 2nd attribute: uvs
    glVertexAttribPointer(
        uvAttrib, // attribute index
        2, // size
        GL_FLOAT, // type
        GL_FALSE, // normalized?
        screen_vertex_size, // stride
        reinterpret_cast<void *>(3 * sizeof(GLfloat)) // array buffer offset
    );
    glEnableVertexAttribArray(uvAttrib);

    glClearColor(32.0f / 255.0f, 178.0f / 255.0f, 170.0f / 255.0f, 1.0f);
    glClearDepthf(1.0f);

    return true;
}

void ScreenRenderer::render(const SceFVector2 &viewport_pos, const SceFVector2 &viewport_size, const float *uvs, const GLuint texture, const SceFVector2 texture_size) {
    // Code for backup and restore is taken from ImGui project ImGui_ImplSdlGL3_RenderDrawData
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
#ifndef __ANDROID__
    GLint last_polygon_mode[2];
    glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
#endif
    GLint last_viewport[4];
    glGetIntegerv(GL_VIEWPORT, last_viewport);
    GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
    GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
    GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
    GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean last_color_mask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, last_color_mask);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glViewport(static_cast<GLint>(viewport_pos.x), static_cast<GLint>(viewport_pos.y), static_cast<GLsizei>(viewport_size.x),
        static_cast<GLsizei>(viewport_size.y));

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // should not be needed, but just in case
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    const auto &shader = enable_fxaa ? m_render_shader_fxaa : m_render_shader_nofilter;

    const GLint posAttrib = glGetAttribLocation(*shader, "position_vertex");
    const GLint uvAttrib = glGetAttribLocation(*shader, "uv_vertex");

    glUseProgram(*shader);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    const float default_uv[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

    if (!uvs) {
        uvs = default_uv;
    }

    if ((uvs[0] != last_uvs[0]) || (uvs[1] != last_uvs[1]) || (uvs[2] != last_uvs[2]) || (uvs[3] != last_uvs[3])) {
        // Reupload the data again
        screen_vertices_t vertex_buffer_data = {
            { { -1.f, -1.f, 0.0f }, { 0.f, 1.f } },
            { { 1.f, -1.f, 0.0f }, { 1.f, 1.f } },
            { { 1.f, 1.f, 0.0f }, { 1.f, 0.f } },
            { { -1.f, 1.f, 0.0f }, { 0.f, 0.f } }
        };

        vertex_buffer_data[0].uv[0] = uvs[0];
        vertex_buffer_data[0].uv[1] = uvs[3];

        vertex_buffer_data[1].uv[0] = uvs[2];
        vertex_buffer_data[1].uv[1] = uvs[3];

        vertex_buffer_data[2].uv[0] = uvs[2];
        vertex_buffer_data[2].uv[1] = uvs[1];

        vertex_buffer_data[3].uv[0] = uvs[0];
        vertex_buffer_data[3].uv[1] = uvs[1];

        last_uvs[0] = uvs[0];
        last_uvs[1] = uvs[1];
        last_uvs[2] = uvs[2];
        last_uvs[3] = uvs[3];

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), nullptr, GL_DYNAMIC_DRAW);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_DYNAMIC_DRAW);
    }

    // 1st attribute: positions
    glVertexAttribPointer(
        posAttrib, // attribute index
        3, // size
        GL_FLOAT, // type
        GL_FALSE, // normalized?
        screen_vertex_size, // stride
        reinterpret_cast<void *>(0) // array buffer offset
    );
    glEnableVertexAttribArray(posAttrib);

    // 2nd attribute: uvs
    glVertexAttribPointer(
        uvAttrib, // attribute index
        2, // size
        GL_FLOAT, // type
        GL_FALSE, // normalized?
        screen_vertex_size, // stride
        reinterpret_cast<void *>(3 * sizeof(GLfloat)) // array buffer offset
    );
    glEnableVertexAttribArray(uvAttrib);

    if (enable_fxaa) {
        const GLint invScreenLocation = glGetUniformLocation(*shader, "inv_frame_size");
        glUniform2f(invScreenLocation, 1 / texture_size.x, 1 / texture_size.y);
    }

    glBindTexture(GL_TEXTURE_2D, texture);
#ifndef __ANDROID__
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Restore modified GL state
    glUseProgram(last_program);
    glBindFramebuffer(GL_FRAMEBUFFER, last_framebuffer);
    glBindTexture(GL_TEXTURE_2D, last_texture);
    glBindSampler(0, last_sampler);
    glActiveTexture(last_active_texture);
    glBindVertexArray(last_vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
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
#ifndef __ANDROID__
    glPolygonMode(GL_FRONT_AND_BACK, (GLenum)last_polygon_mode[0]);
#endif
    glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
    glColorMask(last_color_mask[0], last_color_mask[1], last_color_mask[2], last_color_mask[3]);
}

void ScreenRenderer::destroy() {
    glDeleteBuffers(1, &m_vbo);
    m_vbo = 0;

    glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;

    glDeleteTextures(1, &m_screen_texture);
    m_screen_texture = 0;
}

ScreenRenderer::~ScreenRenderer() {
    destroy();
}

} // namespace renderer::gl
