// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

#include "screen_render.h"

#include <host/state.h>
#include <util/log.h>

#include <SDL_video.h>

bool gl_screen_renderer::init(const std::string &base_path) {
    glGenTextures(1, &m_screen_texture);

    const auto builtin_shaders_path = base_path + "shaders-builtin/";

    m_render_shader = gl::load_shaders(builtin_shaders_path + "render_main.vert", builtin_shaders_path + "render_main.frag");
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
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

    GLint posAttrib = glGetAttribLocation(*m_render_shader, "position_vertex");
    GLint uvAttrib = glGetAttribLocation(*m_render_shader, "uv_vertex");

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

    glClearColor(1.0, 0.0, 0.5, 1.0);
    glClearDepth(1.0f);

    return true;
}

void gl_screen_renderer::render(const HostState &host) {
    const DisplayState &display = host.display;
    const MemState &mem = host.mem;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(static_cast<gl::GLint>(host.viewport_pos.x), static_cast<gl::GLint>(host.viewport_pos.y), static_cast<gl::GLsizei>(host.viewport_size.x), 
        static_cast<gl::GLsizei>(host.viewport_size.y));

    if ((display.image_size.x > 0) && (display.image_size.y > 0)) {
        glUseProgram(*m_render_shader);
        glBindVertexArray(m_vao);

        glBindTexture(GL_TEXTURE_2D, m_screen_texture);
        const auto pixels = display.base.cast<void>().get(mem);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, display.pitch);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, display.image_size.x, display.image_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
}

void gl_screen_renderer::destroy() {
    glDeleteBuffers(1, &m_vbo);
    m_vbo = 0;

    glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;

    glDeleteTextures(1, &m_screen_texture);
    m_screen_texture = 0;
}

gl_screen_renderer::~gl_screen_renderer() {
    destroy();
}
