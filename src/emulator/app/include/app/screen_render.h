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

#pragma once

#include <glutil/shader.h>

struct HostState;

namespace app {

class gl_screen_renderer {
public:
    gl_screen_renderer() {}
    ~gl_screen_renderer();

    bool init(const std::string &base_path);
    void render(const HostState &state);
    void destroy();

private:
    struct screen_vertex {
        GLfloat pos[3];
        GLfloat uv[2];
    };

    static constexpr size_t screen_vertex_size = sizeof(screen_vertex);
    static constexpr uint32_t screen_vertex_count = 4;

    using screen_vertices_t = screen_vertex[screen_vertex_count];

private:
    GLuint m_vao{ 0 };
    GLuint m_vbo{ 0 };
    SharedGLObject m_render_shader;
    GLuint m_screen_texture{ 0 };
};

} // namespace app
