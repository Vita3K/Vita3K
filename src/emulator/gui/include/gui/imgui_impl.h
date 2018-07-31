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

#include <host/state.h>

// clang-format off
#include <imgui.h>
#include <gui/imgui_impl_sdl_gl3.h>
// clang-format on

#include <glutil/gl.h>

namespace imgui {

void init(WindowPtr window);
void draw_begin(WindowPtr window);
void draw_main(HostState &host, GLuint TextureID);
void draw_end(WindowPtr window);

} // namespace imgui
