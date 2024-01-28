// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <glutil/object.h>
#include <util/fs.h>
#include <util/types.h>

namespace renderer::gl {

enum class ScreenFilter {
    BILINEAR,
    NEAREST,
    BICUBIC,
    FXAA,
    SMAA,
};

class ScreenRenderer {
public:
    ScreenRenderer() = default;
    ~ScreenRenderer();

    bool init(const fs::path &static_assets);
    void render(const SceFVector2 &viewport_pos, const SceFVector2 &viewport_size, const float *uvs, const GLuint texture, const SceFVector2 texture_size, GLuint default_fbo = 0);

    void destroy();

    // Fallback in case surface cache does not contain what we want
    // For homebrew that does not use GXM
    GLuint get_resident_texture() const {
        return m_screen_texture;
    }

    void set_filter(ScreenFilter new_filter);

private:
    struct screen_vertex {
        GLfloat pos[3];
        GLfloat uv[2];
    };

    static constexpr size_t screen_vertex_size = sizeof(screen_vertex);
    static constexpr uint32_t screen_vertex_count = 4;

    using screen_vertices_t = screen_vertex[screen_vertex_count];

    // compiles the three SMAA programs and uploads the precomputed lookup tables,
    // returns false (and leaves SMAA unusable) if anything went wrong
    bool init_smaa(const fs::path &static_assets);
    // (re)creates the offscreen edges/blend targets when the source resolution changes
    void resize_smaa_targets(GLsizei width, GLsizei height);
    // runs the edge detection and blending weight passes into the offscreen targets
    void render_smaa_offscreen(GLuint texture, const SceFVector2 &texture_size);
    void setup_vertex_attributes(GLuint program);

    ScreenFilter m_filter = ScreenFilter::BILINEAR;

    GLuint m_vao{ 0 };
    GLuint m_vbo{ 0 };
    SharedGLObject m_render_shader_nofilter;
    SharedGLObject m_render_shader_fxaa;
    SharedGLObject m_render_shader_bicubic;
    GLuint m_screen_texture{ 0 };
    GLuint m_sampler_linear{ 0 };
    GLuint m_sampler_nearest{ 0 };

    // Intermediate target for running FXAA at the rendering resolution
    GLuint m_fxaa_fbo{ 0 };
    GLuint m_fxaa_texture{ 0 };
    int m_fxaa_width{ 0 };
    int m_fxaa_height{ 0 };

    const SharedGLObject &current_shader() const;
    GLuint current_sampler() const;

    float last_uvs[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

    // SMAA: two offscreen passes at the source resolution followed by a screen pass
    SharedGLObject m_smaa_shader_edge;
    SharedGLObject m_smaa_shader_blend;
    SharedGLObject m_smaa_shader_neighborhood;
    // full source quad (uv 0..1), used by the two offscreen passes
    GLuint m_smaa_vao{ 0 };
    GLuint m_smaa_vbo{ 0 };
    GLuint m_smaa_area_texture{ 0 };
    GLuint m_smaa_search_texture{ 0 };
    GLuint m_smaa_edges_texture{ 0 };
    GLuint m_smaa_blend_texture{ 0 };
    GLuint m_smaa_edges_fbo{ 0 };
    GLuint m_smaa_blend_fbo{ 0 };
    GLsizei m_smaa_width{ 0 };
    GLsizei m_smaa_height{ 0 };
};

} // namespace renderer::gl
