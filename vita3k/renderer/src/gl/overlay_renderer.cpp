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
// Copyright RPCS3
// SPDX-License-Identifier: GPL-2.0
// Code heavily referenced/taken from https://github.com/RPCS3/rpcs3/tree/master/rpcs3/Emu/RSX/Overlays

#include <renderer/gl/overlay_renderer.h>
#include <renderer/state.h>

#include <glutil/shader.h>
#include <overlay/font.h>
#include <util/log.h>

#include <chrono>
#include <cmath>

namespace renderer::gl {

static constexpr float VIRTUAL_WIDTH = 960.f;
static constexpr float VIRTUAL_HEIGHT = 544.f;

static constexpr GLint TEX_UNIT_2D = 31;
static constexpr GLint TEX_UNIT_ARRAY = 30;

OverlayRenderer::~OverlayRenderer() {
    destroy();
}

bool OverlayRenderer::init(const fs::path &static_assets, const fs::path &vita_fs_path, int sys_lang) {
    const auto overlay_shaders_path = static_assets / "shaders-builtin" / "overlay";
    const auto vert_path = overlay_shaders_path / "overlay.vert";
    const auto frag_path = overlay_shaders_path / "overlay.frag";

    m_program = ::gl::load_shaders(vert_path, frag_path);
    if (!m_program) {
        LOG_ERROR("Failed to compile overlay shaders");
        return false;
    }

    const GLuint prog = *m_program;

    u_ui_scale = glGetUniformLocation(prog, "ui_scale");
    u_viewport = glGetUniformLocation(prog, "viewport");
    u_albedo = glGetUniformLocation(prog, "albedo");
    u_clip_bounds = glGetUniformLocation(prog, "clip_bounds");
    u_vertex_config = glGetUniformLocation(prog, "vertex_config");
    u_fragment_config = glGetUniformLocation(prog, "fragment_config");
    u_timestamp = glGetUniformLocation(prog, "timestamp");
    u_blur_intensity = glGetUniformLocation(prog, "blur_intensity");
    u_sdf_params = glGetUniformLocation(prog, "sdf_params");
    u_sdf_origin = glGetUniformLocation(prog, "sdf_origin");
    u_sdf_border_color = glGetUniformLocation(prog, "sdf_border_color");

    glUseProgram(prog);
    GLint u_fs0 = glGetUniformLocation(prog, "fs0");
    GLint u_fs1 = glGetUniformLocation(prog, "fs1");
    if (u_fs0 >= 0)
        glUniform1i(u_fs0, TEX_UNIT_2D);
    if (u_fs1 >= 0)
        glUniform1i(u_fs1, TEX_UNIT_ARRAY);
    glUseProgram(0);

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(overlay::vertex), reinterpret_cast<void *>(0));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return true;
}

void OverlayRenderer::destroy() {
    m_program.reset();

    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }

    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }

    for (auto &[ptr, entry] : m_font_cache) {
        if (entry.texture)
            glDeleteTextures(1, &entry.texture);
    }
    m_font_cache.clear();

    for (auto &entry : m_view_cache) {
        if (entry.texture)
            glDeleteTextures(1, &entry.texture);
    }
    m_view_cache.clear();

    for (auto &[ptr, entry] : m_raw_image_cache) {
        if (entry.texture)
            glDeleteTextures(1, &entry.texture);
    }
    m_raw_image_cache.clear();
    m_quad_firsts.clear();
    m_quad_counts.clear();

    m_resources.free_resources();
    m_resources_loaded = false;
}

GLuint OverlayRenderer::get_font_texture(const overlay::font *f) {
    if (!f)
        return 0;

    const auto dims = f->get_glyph_data_dimensions();
    if (dims.depth == 0)
        return 0;

    auto &entry = m_font_cache[f];

    if (entry.texture && entry.depth == dims.depth)
        return entry.texture;

    const auto &glyph_data = f->get_glyph_data();
    if (glyph_data.empty())
        return 0;

    if (entry.texture)
        glDeleteTextures(1, &entry.texture);

    glGenTextures(1, &entry.texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, entry.texture);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_R8,
        static_cast<GLsizei>(dims.width),
        static_cast<GLsizei>(dims.height),
        static_cast<GLsizei>(dims.depth));

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    const size_t page_size = static_cast<size_t>(dims.width) * dims.height;
    for (uint32_t layer = 0; layer < dims.depth; ++layer) {
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, static_cast<GLint>(layer),
            static_cast<GLsizei>(dims.width),
            static_cast<GLsizei>(dims.height),
            1, GL_RED, GL_UNSIGNED_BYTE,
            glyph_data.data() + layer * page_size);
    }

    entry.depth = dims.depth;
    return entry.texture;
}

void OverlayRenderer::upload_image(const overlay::image_info_base *img, image_entry &entry) {
    if (!img || !img->get_data())
        return;

    if (entry.texture == 0)
        glGenTextures(1, &entry.texture);

    glBindTexture(GL_TEXTURE_2D, entry.texture);

    const GLenum fmt = (img->channels == 4) ? GL_RGBA : GL_RED;
    const GLenum internal_fmt = (img->channels == 4) ? GL_RGBA8 : GL_R8;

    if (entry.w == img->w && entry.h == img->h) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
            img->w, img->h, fmt, GL_UNSIGNED_BYTE, img->get_data());
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internal_fmt),
            img->w, img->h, 0, fmt, GL_UNSIGNED_BYTE, img->get_data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        entry.w = img->w;
        entry.h = img->h;
    }

    img->dirty = false;
}

void OverlayRenderer::bind_texture(const overlay::compiled_resource::command_config &config) {
    const uint8_t ref = config.texture_ref;

    if (ref == overlay::image_resource_none
        || ref == overlay::game_icon
        || ref == overlay::backbuffer) {
        glActiveTexture(GL_TEXTURE0 + TEX_UNIT_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        return;
    }

    if (config.font_ref) {
        GLuint font_tex = get_font_texture(config.font_ref);
        glActiveTexture(GL_TEXTURE0 + TEX_UNIT_ARRAY);
        glBindTexture(GL_TEXTURE_2D_ARRAY, font_tex);
        return;
    }

    if (ref == overlay::raw_image && config.external_data_ref) {
        auto &entry = m_raw_image_cache[config.external_data_ref];
        const auto *img = static_cast<const overlay::image_info_base *>(config.external_data_ref);
        if (entry.texture == 0 || img->dirty) {
            upload_image(img, entry);
        }
        glActiveTexture(GL_TEXTURE0 + TEX_UNIT_2D);
        glBindTexture(GL_TEXTURE_2D, entry.texture);
        return;
    }

    if (!m_resources_loaded) {
        m_resources.load_files();
        m_resources_loaded = true;
        m_view_cache.clear();
    }

    const size_t idx = static_cast<size_t>(ref) - 1;
    if (idx >= m_resources.texture_raw_data.size())
        return;

    if (m_view_cache.size() <= idx)
        m_view_cache.resize(idx + 1);

    auto &entry = m_view_cache[idx];
    const auto *img = m_resources.texture_raw_data[idx].get();
    if (img && (entry.texture == 0 || img->dirty)) {
        upload_image(img, entry);
    }

    if (entry.texture) {
        glActiveTexture(GL_TEXTURE0 + TEX_UNIT_2D);
        glBindTexture(GL_TEXTURE_2D, entry.texture);
    }
}

void OverlayRenderer::draw_command(const overlay::compiled_resource::command &cmd,
    float viewport_w, float viewport_h,
    float viewport_x, float viewport_y) {
    if (cmd.verts.empty())
        return;

    const auto &config = cmd.config;

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    const auto data_size = static_cast<GLsizeiptr>(cmd.verts.size() * sizeof(overlay::vertex));
    glBufferData(GL_ARRAY_BUFFER, data_size, cmd.verts.data(), GL_STREAM_DRAW);

    glUniform4f(u_albedo, config.color.r, config.color.g, config.color.b, config.color.a);

    uint32_t vert_cfg = 0;
    if (config.disable_vertex_snap)
        vert_cfg |= 1u;
    glUniform1ui(u_vertex_config, vert_cfg);

    uint32_t frag_cfg = 0;
    if (config.clip_region)
        frag_cfg |= 1u;
    if (config.pulse_glow)
        frag_cfg |= 2u;

    uint32_t sampler_mode = 0;
    if (config.font_ref) {
        sampler_mode = 2;
    } else if (config.texture_ref == overlay::font_file) {
        sampler_mode = 1;
    } else if (config.texture_ref != overlay::image_resource_none
        && config.texture_ref != overlay::game_icon
        && config.texture_ref != overlay::backbuffer) {
        sampler_mode = 3;
    }
    frag_cfg |= (sampler_mode & 3u) << 2u;

    const bool is_sdf = config.active_effect == overlay::compiled_resource::effect_type::sdf
        && config.effect.sdf.func != overlay::sdf_function::none;
    const bool is_gloss = config.active_effect == overlay::compiled_resource::effect_type::gloss;
    const bool is_btn_gloss = config.active_effect == overlay::compiled_resource::effect_type::btn_gloss;
    const bool has_sdf_btn_gloss = is_sdf && config.effect.sdf.btn_gloss_height > 0.f;

    uint32_t sdf_type = is_sdf ? static_cast<uint32_t>(config.effect.sdf.func) : 0u;
    frag_cfg |= (sdf_type & 3u) << 4u;
    if (is_gloss)
        frag_cfg |= 64u;
    if (is_btn_gloss || has_sdf_btn_gloss)
        frag_cfg |= 128u;

    glUniform1ui(u_fragment_config, frag_cfg);

    glUniform1f(u_timestamp, config.get_sinus_value());

    glUniform1f(u_blur_intensity, static_cast<float>(config.blur_strength));

    glUniform4f(u_clip_bounds, config.clip_rect.x1, config.clip_rect.y1,
        config.clip_rect.x2, config.clip_rect.y2);

    if (is_sdf) {
        auto sdf = config.effect.sdf;
        overlay::areaf target_vp;
        target_vp.x1 = viewport_x;
        target_vp.y1 = viewport_y;
        target_vp.x2 = viewport_x + viewport_w;
        target_vp.y2 = viewport_y + viewport_h;
        sdf.transform(target_vp, { VIRTUAL_WIDTH, VIRTUAL_HEIGHT });

        // Flip cy for GL's bottom-up gl_FragCoord.y
        sdf.cy = 2.0f * viewport_y + viewport_h - sdf.cy;

        glUniform4f(u_sdf_params,
            sdf.hx, sdf.hy,
            sdf.br, sdf.bw);
        glUniform4f(u_sdf_origin, sdf.cx, sdf.cy,
            has_sdf_btn_gloss ? sdf.btn_gloss_height : 0.f,
            has_sdf_btn_gloss ? (std::round(sdf.btn_gloss_opacity * 100.f) + sdf.btn_gloss_bottom_opacity) : 0.f);
        glUniform4f(u_sdf_border_color,
            sdf.border_color.r,
            sdf.border_color.g,
            sdf.border_color.b,
            sdf.border_color.a);
    } else if (is_gloss) {
        const auto &g = config.effect.gloss;
        glUniform4f(u_sdf_params,
            g.height, g.feather,
            g.opacity, 0.f);
    } else if (is_btn_gloss) {
        const auto &bg = config.effect.btn_gloss;
        glUniform4f(u_sdf_params,
            bg.height, bg.curve_lift,
            bg.opacity, bg.border_radius_frac);
        glUniform4f(u_sdf_origin,
            bg.aspect, bg.bottom_opacity, 0.f, 0.f);
    }

    bind_texture(config);

    GLenum gl_mode = GL_TRIANGLE_STRIP;
    switch (config.primitives) {
    case overlay::primitive_type::quad_list: {
        const GLsizei vert_count = static_cast<GLsizei>(cmd.verts.size());
        const int num_quads = vert_count / 4;
        if (num_quads > 0) {
            if (glad_glMultiDrawArrays) {
                m_quad_firsts.resize(num_quads);
                m_quad_counts.resize(num_quads);
                for (int i = 0; i < num_quads; ++i) {
                    m_quad_firsts[i] = i * 4;
                    m_quad_counts[i] = 4;
                }
                glMultiDrawArrays(GL_TRIANGLE_STRIP, m_quad_firsts.data(), m_quad_counts.data(), num_quads);
            } else {
                // GLES does not expose core glMultiDrawArrays in our loader
                for (int i = 0; i < num_quads; ++i) {
                    glDrawArrays(GL_TRIANGLE_STRIP, i * 4, 4);
                }
            }
        }
        return;
    }
    case overlay::primitive_type::triangle_strip:
        gl_mode = GL_TRIANGLE_STRIP;
        break;
    case overlay::primitive_type::line_list:
        gl_mode = GL_LINES;
        break;
    case overlay::primitive_type::line_strip:
        gl_mode = GL_LINE_STRIP;
        break;
    case overlay::primitive_type::triangle_fan:
        gl_mode = GL_TRIANGLE_FAN;
        break;
    }

    glDrawArrays(gl_mode, 0, static_cast<GLsizei>(cmd.verts.size()));
}

void OverlayRenderer::render(const overlay::display_manager &manager,
    float viewport_x, float viewport_y,
    float viewport_w, float viewport_h,
    float framebuffer_w, float framebuffer_h,
    GLuint default_fbo) {
    if (!m_program || !manager.has_visible())
        return;

    GLint last_framebuffer;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &last_framebuffer);
    GLint last_active_texture;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &last_active_texture);
    GLint last_program;
    glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
    GLint last_texture_2d;
    glActiveTexture(GL_TEXTURE0 + TEX_UNIT_2D);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture_2d);
    GLint last_texture_array;
    glActiveTexture(GL_TEXTURE0 + TEX_UNIT_ARRAY);
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &last_texture_array);
    GLint last_vao;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vao);
    GLint last_vbo;
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_vbo);
    GLint last_viewport[4];
    glGetIntegerv(GL_VIEWPORT, last_viewport);
    GLboolean last_blend = glIsEnabled(GL_BLEND);
    GLboolean last_depth = glIsEnabled(GL_DEPTH_TEST);
    GLboolean last_cull = glIsEnabled(GL_CULL_FACE);
    GLboolean last_scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLint last_blend_src_rgb, last_blend_dst_rgb, last_blend_src_alpha, last_blend_dst_alpha;
    glGetIntegerv(GL_BLEND_SRC_RGB, &last_blend_src_rgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &last_blend_dst_rgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &last_blend_src_alpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &last_blend_dst_alpha);
    GLint last_blend_eq_rgb, last_blend_eq_alpha;
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &last_blend_eq_rgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &last_blend_eq_alpha);
    GLboolean last_color_mask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, last_color_mask);

    glBindFramebuffer(GL_FRAMEBUFFER, default_fbo);
    glViewport(static_cast<GLint>(viewport_x), static_cast<GLint>(viewport_y),
        static_cast<GLsizei>(viewport_w), static_cast<GLsizei>(viewport_h));

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_SCISSOR_TEST);
    glScissor(static_cast<GLint>(viewport_x), static_cast<GLint>(viewport_y),
        static_cast<GLsizei>(viewport_w), static_cast<GLsizei>(viewport_h));
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glUseProgram(*m_program);
    glBindVertexArray(m_vao);

    glUniform4f(u_ui_scale, VIRTUAL_WIDTH, VIRTUAL_HEIGHT, 1.f, 1.f);

    glUniform4f(u_viewport, viewport_w, viewport_h, viewport_x, viewport_y);

    const auto timestamp_us = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());

    std::vector<std::shared_ptr<overlay::overlay>> views_snapshot;
    {
        manager.lock_shared();
        const auto &views = manager.get_views();
        views_snapshot.reserve(views.size());
        for (const auto &view : views) {
            if (view && view->visible.load())
                views_snapshot.push_back(view);
        }
        manager.unlock_shared();
    }

    for (const auto &view : views_snapshot) {
        view->set_render_viewport(
            static_cast<uint16_t>(std::min(viewport_w, 65535.f)),
            static_cast<uint16_t>(std::min(viewport_h, 65535.f)));

        auto compiled = view->get_compiled();

        for (const auto &cmd : compiled.draw_commands) {
            draw_command(cmd, viewport_w, viewport_h, viewport_x, viewport_y);
        }

        view->update(timestamp_us);
    }

    glUseProgram(last_program);
    glBindVertexArray(last_vao);
    glBindBuffer(GL_ARRAY_BUFFER, last_vbo);
    glBindFramebuffer(GL_FRAMEBUFFER, last_framebuffer);
    glActiveTexture(GL_TEXTURE0 + TEX_UNIT_2D);
    glBindTexture(GL_TEXTURE_2D, last_texture_2d);
    glActiveTexture(GL_TEXTURE0 + TEX_UNIT_ARRAY);
    glBindTexture(GL_TEXTURE_2D_ARRAY, last_texture_array);
    glActiveTexture(last_active_texture);
    glViewport(last_viewport[0], last_viewport[1],
        static_cast<GLsizei>(last_viewport[2]), static_cast<GLsizei>(last_viewport[3]));

    if (last_blend)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    glBlendFuncSeparate(last_blend_src_rgb, last_blend_dst_rgb,
        last_blend_src_alpha, last_blend_dst_alpha);
    glBlendEquationSeparate(last_blend_eq_rgb, last_blend_eq_alpha);
    if (last_depth)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (last_cull)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    if (last_scissor)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
    glColorMask(last_color_mask[0], last_color_mask[1], last_color_mask[2], last_color_mask[3]);
}

} // namespace renderer::gl
