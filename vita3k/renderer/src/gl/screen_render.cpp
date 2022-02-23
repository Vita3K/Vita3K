// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include <display/state.h>
#include <mem/state.h>
#include <renderer/gl/screen_render.h>

#include <util/log.h>

namespace renderer::gl {

bool ScreenRenderer::init(const std::string &base_path) {
    glGenTextures(1, &m_screen_texture);

    const auto builtin_shaders_path = base_path + "shaders-builtin/";

    m_render_shader = ::gl::load_shaders(builtin_shaders_path + "render_main.vert", builtin_shaders_path + "render_main.frag");
    if (!m_render_shader) {
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

    posAttrib = glGetAttribLocation(*m_render_shader, "position_vertex");
    uvAttrib = glGetAttribLocation(*m_render_shader, "uv_vertex");

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

    glClearColor(0.125490203f, 0.698039234f, 0.666666687f, 1.0f);
    glClearDepth(1.0f);

    return true;
}

void ScreenRenderer::render(const SceFVector2 &viewport_pos, const SceFVector2 &viewport_size, const float *uvs, const GLuint texture) {
    // Backup GL state
    glActiveTexture(GL_TEXTURE0);
    GLint last_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
    GLint last_viewport[4];
    glGetIntegerv(GL_VIEWPORT, last_viewport);
    GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
    GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean last_enable_depth = glIsEnabled(GL_DEPTH_TEST);
    GLboolean last_enable_cull = glIsEnabled(GL_CULL_FACE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    glViewport(static_cast<GLint>(viewport_pos.x), static_cast<GLint>(viewport_pos.y), static_cast<GLsizei>(viewport_size.x),
        static_cast<GLsizei>(viewport_size.y));

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(*m_render_shader);
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

    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glBindTexture(GL_TEXTURE_2D, last_texture);

    if (last_enable_blend)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (last_enable_scissor_test)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

    if (last_enable_depth)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (last_enable_cull)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);

    glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
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
