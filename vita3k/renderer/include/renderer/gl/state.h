// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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
#include <renderer/texture_cache_state.h>
#include <renderer/types.h>

#include "types.h"
#include <features/state.h>

#include <SDL.h>

#include <map>
#include <string>
#include <vector>

namespace renderer::gl {
struct GLState : public renderer::State {
    GLContextPtr context;

    ShaderCache fragment_shader_cache;
    ShaderCache vertex_shader_cache;
    ProgramCache program_cache;

    GLTextureCacheState texture_cache;
    GLSurfaceCache surface_cache;

    std::vector<ShadersHash> shaders_cache_hashs;
    std::string shader_version = "v1";

    ScreenRenderer screen_renderer;

    bool init(const char *base_path, const bool hashless_texture_cache) override;
    void render_frame(const SceFVector2 &viewport_pos, const SceFVector2 &viewport_size, const DisplayState &display,
        const MemState &mem) override;
};

} // namespace renderer::gl
