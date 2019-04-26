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

#include <boost/optional.hpp>

#include <string>
#include <vector>

using boost::optional;

// Config options
struct Config {
    optional<std::string> vpk_path;
    optional<std::string> run_title_id;
    optional<std::string> recompile_shader_path;
    optional<int> log_level;
    bool log_imports = false;
    bool log_exports = false;
    bool log_active_shaders = false;
    bool log_uniforms = false;
    std::vector<std::string> lle_modules;
    bool pstv_mode = false;
    bool show_gui = false;
    optional<std::string> pref_path;
    bool archive_log = false;
    optional<bool> wait_for_debugger;
    optional<std::string> background_image;
    optional<float> background_alpha;
};
