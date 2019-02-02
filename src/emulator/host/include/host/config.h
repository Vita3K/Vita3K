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

#include <host/app.h>

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>

#include <string>
#include <vector>

using boost::optional;

// Config options
struct Config {
    optional<std::string> vpk_path;
    optional<std::string> run_title_id;
    optional<int> log_level;
    bool log_imports = false;
    bool log_exports = false;
    bool log_active_shaders = false;
    bool log_uniforms = false;
    std::vector<std::string> lle_modules;
    bool pstv_mode = false;
    optional<std::string> pref_path;
};

namespace config {

/*
 * \brief Initializes config system from a YML file.
 * 
 * \param cfg Config options are returned via this parameter.
 * \return True on loading success.
*/
ExitCode init_from_config_file(Config &cfg, const char *config_file_name);

/*
 * \brief Save emulator config to a YML file.
 *
 * \return True on saving success.
*/
ExitCode save_config_to_file(Config &cfg, const char *config_file_name);

/**
  * \brief Initializes config system, parsing command-line args and handling some basic ones:
  *        --help, --version, --log-level
  * \param cfg Config options are returned via this parameter.
  * \return True on success, false on error.
  */
ExitCode init(Config &cfg, int argc, char **argv);

} // namespace config
