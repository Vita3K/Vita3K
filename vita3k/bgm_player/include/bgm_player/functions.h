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

#include <util/fs.h>

namespace bgm_player {

void destroy_bgm_player();
bool init_bgm(const fs::path &pref_path, const bool is_enable);
void init_bgm_player(const float vol);
void set_bgm_volume(const float vol);
void set_current_bgm_path(const std::pair<std::string, std::string> &path);
void stop_bgm();
void switch_bgm_state(const bool pause);

} // namespace bgm_player
