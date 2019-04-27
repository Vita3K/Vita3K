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

#include <glutil/shader.h>
#include <host/state.h>
#include <util/log.h>

namespace app {
bool ScreenRenderer::init(const std::string &base_path) {
    glGenTextures(1, &texture);

    const auto builtin_shaders_path = base_path + "shaders-builtin/";

    program = gl::load_shaders(builtin_shaders_path + "render_main.vert", builtin_shaders_path + "render_main.frag");
    if (!program) {
        LOG_CRITICAL("Couldn't compile essential shaders for rendering. Exiting");
        return false;
    }

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    struct Vertex {
        SceFVector3 pos;
        SceFVector2 uv;
    };

    static const Vertex vertex_buffer_data[] = {
        { { -1.f, -1.f, 0.0f }, { 0.f, 1.f } },
        { { 1.f, -1.f, 0.0f }, { 1.f, 1.f } },
        { { 1.f, 1.f, 0.0f }, { 1.f, 0.f } },
        { { -1.f, 1.f, 0.0f }, { 0.f, 0.f } }
    };

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

    GLint posAttrib = glGetAttribLocation(*program, "position_vertex");
    GLint uvAttrib = glGetAttribLocation(*program, "uv_vertex");

    // 1st attribute: positions
    glVertexAttribPointer(
        posAttrib, // attribute index
        3, // size
        GL_FLOAT, // type
        GL_FALSE, // normalized?
        sizeof(Vertex), // stride
        nullptr // array buffer offset
    );
    glEnableVertexAttribArray(posAttrib);

    // 2nd attribute: uvs
    glVertexAttribPointer(
        uvAttrib, // attribute index
        2, // size
        GL_FLOAT, // type
        GL_FALSE, // normalized?
        sizeof(Vertex), // stride
        reinterpret_cast<const void *>(offsetof(Vertex, uv)) // array buffer offset
    );
    glEnableVertexAttribArray(uvAttrib);

    glClearColor(1.0, 0.0, 0.5, 1.0);
    glClearDepth(1.0f);

    return true;
}

void ScreenRenderer::render(const HostState &host) {
    const DisplayState &display = host.display;
    const MemState &mem = host.mem;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(host.viewport_pos.x, host.viewport_pos.y, host.viewport_size.x, host.viewport_size.y);

    if ((display.image_size.x > 0) && (display.image_size.y > 0)) {
        glUseProgram(*program);
        glBindVertexArray(vao);

        glBindTexture(GL_TEXTURE_2D, texture);
        const auto pixels = display.base.cast<void>().get(mem);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, display.pitch);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, display.image_size.x, display.image_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
}

ScreenRenderer::~ScreenRenderer() {
    glDeleteBuffers(1, &vbo);
    vbo = 0;

    glDeleteVertexArrays(1, &vao);
    vao = 0;

    glDeleteTextures(1, &texture);
    texture = 0;
}
} // namespace app
