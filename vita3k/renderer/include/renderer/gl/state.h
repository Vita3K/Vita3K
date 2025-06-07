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

#pragma once

#include <renderer/gl/screen_render.h>
#include <renderer/gl/surface_cache.h>
#include <renderer/state.h>
#include <renderer/types.h>

#include "types.h"

#include <string_view>
#include <vector>

namespace renderer::gl {
struct GLState : public renderer::State {
    GLContextPtr context;

    ShaderCache fragment_shader_cache;
    ShaderCache vertex_shader_cache;
    ProgramCache program_cache;

    GLTextureCache texture_cache;
    GLSurfaceCache surface_cache;

    ScreenRenderer screen_renderer;

    bool init() override;
    void late_init(const Config &cfg, const std::string_view game_id, MemState &mem) override;

    TextureCache *get_texture_cache() override {
        return &texture_cache;
    }

    void render_frame(const SceFVector2 &viewport_pos, const SceFVector2 &viewport_size, DisplayState &display,
        const GxmState &gxm, MemState &mem) override;
    void swap_window(SDL_Window *window) override;
    std::vector<uint32_t> dump_frame(DisplayState &display, uint32_t &width, uint32_t &height) override;

    int get_supported_filters() override;
    void set_screen_filter(const std::string_view &filter) override;
    int get_max_anisotropic_filtering() override;
    void set_anisotropic_filtering(int anisotropic_filtering) override;
    int get_max_2d_texture_width() override;

    std::string_view get_gpu_name() override;

    void precompile_shader(const ShadersHash &hash) override;
    void preclose_action() override;
};

} // namespace renderer::gl
