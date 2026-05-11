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
#include <overlay/compiled_resource.h>
#include <overlay/controls.h>
#include <overlay/display_manager.h>
#include <util/fs.h>

#include <unordered_map>
#include <vector>

namespace renderer {
struct WindowCallbacks;
}

namespace renderer::gl {

class OverlayRenderer {
public:
    OverlayRenderer() = default;
    ~OverlayRenderer();

    bool init(const fs::path &static_assets, const fs::path &pref_path,
        const renderer::WindowCallbacks &callbacks, int sys_lang);
    void destroy();

    void render(const overlay::display_manager &manager,
        float viewport_x, float viewport_y,
        float viewport_w, float viewport_h,
        float framebuffer_w, float framebuffer_h,
        GLuint default_fbo);

private:
    UniqueGLObject m_program;

    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    GLint u_ui_scale = -1;
    GLint u_viewport = -1;
    GLint u_albedo = -1;
    GLint u_clip_bounds = -1;
    GLint u_vertex_config = -1;
    GLint u_fragment_config = -1;
    GLint u_timestamp = -1;
    GLint u_blur_intensity = -1;
    GLint u_sdf_params = -1;
    GLint u_sdf_origin = -1;
    GLint u_sdf_border_color = -1;

    struct font_entry {
        GLuint texture = 0;
        uint32_t depth = 0;
    };
    std::unordered_map<const overlay::font *, font_entry> m_font_cache;

    struct image_entry {
        GLuint texture = 0;
        int w = 0;
        int h = 0;
    };

    std::vector<image_entry> m_view_cache;
    std::unordered_map<const void *, image_entry> m_raw_image_cache;

    overlay::resource_config m_resources;
    bool m_resources_loaded = false;

    std::vector<GLint> m_quad_firsts;
    std::vector<GLsizei> m_quad_counts;

    GLuint get_font_texture(const overlay::font *f);
    void upload_image(const overlay::image_info_base *img, image_entry &entry);
    void bind_texture(const overlay::compiled_resource::command_config &config);
    void draw_command(const overlay::compiled_resource::command &cmd,
        float viewport_w, float viewport_h, float viewport_x, float viewport_y);
};

} // namespace renderer::gl
