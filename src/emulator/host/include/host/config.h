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

#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <boost/optional/optional_io.hpp>

#include <string>

using boost::optional;

// Config options
struct Config {
    optional<std::string> vpk_path;
    optional<std::string> run_title_id;
    optional<int> log_level;
};

namespace config {

/**
  * \brief Initializes config system, parsing command-line args and handling some basic ones:
  *        --help, --version, -log-level
  * \param cfg Config options are returend via this parameter.
  * \return True on success, false on error.
  */
bool init(Config &cfg, int argc, char **argv);

} // namespace config
