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

#include <glutil/shader.h>
#include <renderer/AreaTex.h>
#include <renderer/SearchTex.h>
#include <renderer/gl/screen_render.h>
#include <util/log.h>

#include <string>
#include <vector>

namespace renderer::gl {

const SharedGLObject &ScreenRenderer::current_shader() const {
    switch (m_filter) {
    case ScreenFilter::FXAA: return m_render_shader_fxaa;
    case ScreenFilter::SMAA: return m_smaa_shader_neighborhood;
    case ScreenFilter::BICUBIC: return m_render_shader_bicubic;
    default: return m_render_shader_nofilter; // Bilinear / Nearest
    }
}

GLuint ScreenRenderer::current_sampler() const {
    return (m_filter == ScreenFilter::NEAREST) ? m_sampler_nearest : m_sampler_linear;
}

void ScreenRenderer::setup_vertex_attributes(GLuint program) {
    const GLint pos_attrib = glGetAttribLocation(program, "position_vertex");
    const GLint uv_attrib = glGetAttribLocation(program, "uv_vertex");

    // 1st attribute: positions
    glVertexAttribPointer(
        pos_attrib, // attribute index
        3, // size
        GL_FLOAT, // type
        GL_FALSE, // normalized?
        screen_vertex_size, // stride
        reinterpret_cast<void *>(0) // array buffer offset
    );
    glEnableVertexAttribArray(pos_attrib);

    // 2nd attribute: uvs
    glVertexAttribPointer(
        uv_attrib, // attribute index
        2, // size
        GL_FLOAT, // type
        GL_FALSE, // normalized?
        screen_vertex_size, // stride
        reinterpret_cast<void *>(3 * sizeof(GLfloat)) // array buffer offset
    );
    glEnableVertexAttribArray(uv_attrib);
}

bool ScreenRenderer::init(const fs::path &static_assets) {
    glGenTextures(1, &m_screen_texture);

    const auto builtin_shaders_path = static_assets / "shaders-builtin/opengl";

    const auto render_main_path_vert = builtin_shaders_path / "render_main.vert";
    const auto render_main_path_frag = builtin_shaders_path / "render_main.frag";
    const auto render_main_path_fxaa_frag = builtin_shaders_path / "render_main_fxaa.frag";
    const auto render_main_path_bicubic_frag = builtin_shaders_path / "render_main_bicubic.frag";

    m_render_shader_nofilter = ::gl::load_shaders(render_main_path_vert, render_main_path_frag);
    m_render_shader_fxaa = ::gl::load_shaders(render_main_path_vert, render_main_path_fxaa_frag);
    m_render_shader_bicubic = ::gl::load_shaders(render_main_path_vert, render_main_path_bicubic_frag);
    if (!m_render_shader_nofilter || !m_render_shader_fxaa || !m_render_shader_bicubic) {
        LOG_CRITICAL("Couldn't compile essential shaders for rendering. Exiting");
        return false;
    }

    glGenSamplers(1, &m_sampler_linear);
    glSamplerParameteri(m_sampler_linear, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(m_sampler_linear, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(m_sampler_linear, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(m_sampler_linear, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenSamplers(1, &m_sampler_nearest);
    glSamplerParameteri(m_sampler_nearest, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(m_sampler_nearest, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(m_sampler_nearest, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(m_sampler_nearest, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // SMAA is optional, the filter simply stays unavailable if it could not be set up
    init_smaa(static_assets);

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

    setup_vertex_attributes(*m_render_shader_nofilter);

    glClearColor(32.0f / 255.0f, 178.0f / 255.0f, 170.0f / 255.0f, 1.0f);
    glClearDepthf(1.0f);

    return true;
}

void ScreenRenderer::set_filter(ScreenFilter new_filter) {
    if (new_filter == ScreenFilter::SMAA && !m_smaa_shader_neighborhood) {
        LOG_WARN("SMAA is not available, falling back to bilinear");
        new_filter = ScreenFilter::BILINEAR;
    }
    m_filter = new_filter;
}

bool ScreenRenderer::init_smaa(const fs::path &static_assets) {
    const auto builtin_shaders_path = static_assets / "shaders-builtin/opengl";

    // the SMAA library is a single header shared with the vulkan backend, it is pasted
    // in front of each shader (along with its configuration) instead of being #included
    std::vector<char> smaa_library;
    if (!fs_utils::read_data(static_assets / "shaders-builtin/SMAA.hlsl", smaa_library)) {
        LOG_ERROR("Couldn't open SMAA.hlsl, SMAA will not be available");
        return false;
    }

    // SMAA_GLSL_3 only needs GLSL 1.30 features, so the same code works on GLES 3.0
    const std::string config = "#define SMAA_GLSL_3 1\n"
                               "#define SMAA_PRESET_HIGH 1\n"
                               "uniform vec4 rt_metrics;\n"
                               "#define SMAA_RT_METRICS rt_metrics\n";
    // the pixel shaders use discard, which is not allowed in a vertex shader
    const std::string vertex_prelude = config + "#define SMAA_INCLUDE_PS 0\n" + std::string(smaa_library.data(), smaa_library.size());
    const std::string fragment_prelude = config + "#define SMAA_INCLUDE_VS 0\n" + std::string(smaa_library.data(), smaa_library.size());

    m_smaa_shader_edge = ::gl::load_shaders(builtin_shaders_path / "smaa_edge.vert",
        builtin_shaders_path / "smaa_edge.frag", vertex_prelude, fragment_prelude);
    m_smaa_shader_blend = ::gl::load_shaders(builtin_shaders_path / "smaa_blend.vert",
        builtin_shaders_path / "smaa_blend.frag", vertex_prelude, fragment_prelude);
    m_smaa_shader_neighborhood = ::gl::load_shaders(builtin_shaders_path / "smaa_neighborhood.vert",
        builtin_shaders_path / "smaa_neighborhood.frag", vertex_prelude, fragment_prelude);

    if (!m_smaa_shader_edge || !m_smaa_shader_blend || !m_smaa_shader_neighborhood) {
        LOG_ERROR("Couldn't compile the SMAA shaders, SMAA will not be available");
        m_smaa_shader_edge.reset();
        m_smaa_shader_blend.reset();
        m_smaa_shader_neighborhood.reset();
        return false;
    }

    // bind each sampler to the texture unit it is given in render_smaa_offscreen / render
    glUseProgram(*m_smaa_shader_blend);
    glUniform1i(glGetUniformLocation(*m_smaa_shader_blend, "edges_tex"), 0);
    glUniform1i(glGetUniformLocation(*m_smaa_shader_blend, "area_tex"), 1);
    glUniform1i(glGetUniformLocation(*m_smaa_shader_blend, "search_tex"), 2);
    glUseProgram(*m_smaa_shader_neighborhood);
    glUniform1i(glGetUniformLocation(*m_smaa_shader_neighborhood, "fb"), 0);
    glUniform1i(glGetUniformLocation(*m_smaa_shader_neighborhood, "blend_tex"), 1);
    glUseProgram(0);

    // upload the two precomputed lookup tables
    // their rows are not always a multiple of 4 bytes (the search texture is 66 wide)
    GLint last_unpack_alignment;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &last_unpack_alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    const auto upload_lut = [](GLuint &texture, GLenum internal_format, GLenum format,
                                GLsizei width, GLsizei height, const void *data, GLint filter) {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    };

    upload_lut(m_smaa_area_texture, GL_RG8, GL_RG, AREATEX_WIDTH, AREATEX_HEIGHT, areaTexBytes, GL_LINEAR);
    upload_lut(m_smaa_search_texture, GL_R8, GL_RED, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, searchTexBytes, GL_NEAREST);

    glPixelStorei(GL_UNPACK_ALIGNMENT, last_unpack_alignment);
    glBindTexture(GL_TEXTURE_2D, 0);

    // the offscreen passes always cover the whole source texture, so unlike the screen
    // quad their uvs never change
    static const screen_vertices_t offscreen_vertices = {
        { { -1.f, -1.f, 0.0f }, { 0.f, 0.f } },
        { { 1.f, -1.f, 0.0f }, { 1.f, 0.f } },
        { { 1.f, 1.f, 0.0f }, { 1.f, 1.f } },
        { { -1.f, 1.f, 0.0f }, { 0.f, 1.f } }
    };

    glGenVertexArrays(1, &m_smaa_vao);
    glBindVertexArray(m_smaa_vao);
    glGenBuffers(1, &m_smaa_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_smaa_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(offscreen_vertices), offscreen_vertices, GL_STATIC_DRAW);
    glBindVertexArray(0);

    glGenFramebuffers(1, &m_smaa_edges_fbo);
    glGenFramebuffers(1, &m_smaa_blend_fbo);
    glGenTextures(1, &m_smaa_edges_texture);
    glGenTextures(1, &m_smaa_blend_texture);

    return true;
}

void ScreenRenderer::resize_smaa_targets(GLsizei width, GLsizei height) {
    if (width == m_smaa_width && height == m_smaa_height)
        return;

    const auto attach = [&](GLuint fbo, GLuint texture) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            LOG_ERROR("Incomplete SMAA framebuffer ({}x{})", width, height);
    };

    attach(m_smaa_edges_fbo, m_smaa_edges_texture);
    attach(m_smaa_blend_fbo, m_smaa_blend_texture);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_smaa_width = width;
    m_smaa_height = height;
}

void ScreenRenderer::render_smaa_offscreen(GLuint texture, const SceFVector2 &texture_size) {
    const auto width = static_cast<GLsizei>(texture_size.x);
    const auto height = static_cast<GLsizei>(texture_size.y);
    resize_smaa_targets(width, height);

    glViewport(0, 0, width, height);
    glBindVertexArray(m_smaa_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_smaa_vbo);

    const auto run_pass = [&](GLuint fbo, GLuint program) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(program);
        glUniform4f(glGetUniformLocation(program, "rt_metrics"),
            1.0f / texture_size.x, 1.0f / texture_size.y, texture_size.x, texture_size.y);
        setup_vertex_attributes(program);
        glDrawArrays(GL_TRIANGLE_FAN, 0, screen_vertex_count);
    };

    // the lookup tables carry their own filtering (linear for the area one, point for
    // the search one), a leftover sampler object would override it
    glBindSampler(0, 0);
    glBindSampler(1, 0);
    glBindSampler(2, 0);

    // pass 1: edge detection, reads the source color
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    run_pass(m_smaa_edges_fbo, *m_smaa_shader_edge);

    // pass 2: blending weight calculation, reads the edges and both lookup tables
    glBindTexture(GL_TEXTURE_2D, m_smaa_edges_texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_smaa_area_texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_smaa_search_texture);
    run_pass(m_smaa_blend_fbo, *m_smaa_shader_blend);

    // leave the lookup tables behind, pass 3 needs unit 1 for the blend texture
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
}

void ScreenRenderer::render(const SceFVector2 &viewport_pos, const SceFVector2 &viewport_size, const float *uvs, const GLuint texture, const SceFVector2 texture_size, GLuint default_fbo) {
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

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
#ifndef __ANDROID__
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

    // the first two SMAA passes render the whole source into offscreen targets, the last
    // one is the screen pass below
    if (m_filter == ScreenFilter::SMAA)
        render_smaa_offscreen(texture, texture_size);

    glBindFramebuffer(GL_FRAMEBUFFER, default_fbo);

    glViewport(static_cast<GLint>(viewport_pos.x), static_cast<GLint>(viewport_pos.y), static_cast<GLsizei>(viewport_size.x),
        static_cast<GLsizei>(viewport_size.y));

    // Clear the whole default framebuffer (covers the letterbox area as well).
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // should not be needed, but just in case
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    const auto &shader = current_shader();

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

    setup_vertex_attributes(*shader);

    glBindTexture(GL_TEXTURE_2D, texture);
    glBindSampler(0, current_sampler());

    if (m_filter == ScreenFilter::FXAA) {
        // FXAA must run at the input (render) resolution, 1:1 with the source texture,
        // otherwise it operates on an already upscaled (blurred) image.
        // Pass 1: run FXAA into an intermediate FBO sized to the render resolution.
        // Pass 2: blit that result to the screen with GL_NEAREST (no bilinear upscale).
        const int fxaa_w = static_cast<int>(texture_size.x);
        const int fxaa_h = static_cast<int>(texture_size.y);

        // (Re)create the intermediate target when the render resolution changes.
        if (fxaa_w != m_fxaa_width || fxaa_h != m_fxaa_height) {
            if (!m_fxaa_fbo) {
                glGenFramebuffers(1, &m_fxaa_fbo);
                glGenTextures(1, &m_fxaa_texture);
            }
            glBindTexture(GL_TEXTURE_2D, m_fxaa_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fxaa_w, fxaa_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindFramebuffer(GL_FRAMEBUFFER, m_fxaa_fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fxaa_texture, 0);
            m_fxaa_width = fxaa_w;
            m_fxaa_height = fxaa_h;

            // restore the source texture binding for the FXAA pass below
            glBindTexture(GL_TEXTURE_2D, texture);
        }

        // Pass 1: FXAA at render resolution
        glBindFramebuffer(GL_FRAMEBUFFER, m_fxaa_fbo);
        glViewport(0, 0, fxaa_w, fxaa_h);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        const GLint invScreenLocation = glGetUniformLocation(*shader, "inv_frame_size");
        glUniform2f(invScreenLocation, 1.0f / texture_size.x, 1.0f / texture_size.y);

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        // Pass 2: nearest-blit to the screen (no bilinear)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fxaa_fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, default_fbo);
        glBlitFramebuffer(
            0, 0, fxaa_w, fxaa_h,
            static_cast<GLint>(viewport_pos.x), static_cast<GLint>(viewport_pos.y),
            static_cast<GLint>(viewport_pos.x + viewport_size.x),
            static_cast<GLint>(viewport_pos.y + viewport_size.y),
            GL_COLOR_BUFFER_BIT, GL_NEAREST);
    } else {
        if (m_filter == ScreenFilter::SMAA) {
            const GLint rt_metrics_location = glGetUniformLocation(*shader, "rt_metrics");
            glUniform4f(rt_metrics_location, 1 / texture_size.x, 1 / texture_size.y, texture_size.x, texture_size.y);
            // the blending weights computed by the offscreen passes cover the whole source,
            // so they are addressed with the same uvs as the color texture
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, m_smaa_blend_texture);
            glActiveTexture(GL_TEXTURE0);
        }
        // Bilinear / Nearest / Bicubic / SMAA draw directly to the screen.
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    if (m_filter == ScreenFilter::SMAA) {
        // only unit 0 is saved and restored below
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
    }

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
    m_render_shader_nofilter.reset();
    m_render_shader_fxaa.reset();
    m_render_shader_bicubic.reset();

    glDeleteSamplers(1, &m_sampler_linear);
    m_sampler_linear = 0;
    glDeleteSamplers(1, &m_sampler_nearest);
    m_sampler_nearest = 0;

    if (m_fxaa_fbo) {
        glDeleteFramebuffers(1, &m_fxaa_fbo);
        m_fxaa_fbo = 0;
    }
    if (m_fxaa_texture) {
        glDeleteTextures(1, &m_fxaa_texture);
        m_fxaa_texture = 0;
    }
    m_fxaa_width = 0;
    m_fxaa_height = 0;

    glDeleteBuffers(1, &m_vbo);
    m_vbo = 0;

    glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;

    glDeleteTextures(1, &m_screen_texture);
    m_screen_texture = 0;

    glDeleteFramebuffers(1, &m_smaa_edges_fbo);
    glDeleteFramebuffers(1, &m_smaa_blend_fbo);
    m_smaa_edges_fbo = 0;
    m_smaa_blend_fbo = 0;

    glDeleteTextures(1, &m_smaa_edges_texture);
    glDeleteTextures(1, &m_smaa_blend_texture);
    glDeleteTextures(1, &m_smaa_area_texture);
    glDeleteTextures(1, &m_smaa_search_texture);
    m_smaa_edges_texture = 0;
    m_smaa_blend_texture = 0;
    m_smaa_area_texture = 0;
    m_smaa_search_texture = 0;

    glDeleteBuffers(1, &m_smaa_vbo);
    m_smaa_vbo = 0;
    glDeleteVertexArrays(1, &m_smaa_vao);
    m_smaa_vao = 0;

    m_smaa_width = 0;
    m_smaa_height = 0;

    m_smaa_shader_edge.reset();
    m_smaa_shader_blend.reset();
    m_smaa_shader_neighborhood.reset();
}

ScreenRenderer::~ScreenRenderer() {
    destroy();
}

} // namespace renderer::gl
