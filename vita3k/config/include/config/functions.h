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

#include <util/exit_code.h>
#include <util/fs.h>

class Root;
struct ConfigState;

namespace config {

/**
  * \brief Save emulator config to a YML file. Call this function if the YAML file needs to be updated.
  * \param cfg Config operations to save.
  * \param output_path The location to save the configuration file.
  * \return Success on saving the config file, otherwise Error.
  */
ExitCode serialize_config(ConfigState &cfg, const fs::path &output_path);

/**
  * \brief Initializes config system, parsing command-line args and handling some basic ones:
  *        --help, --version, --log-level
  * \param cfg Config options are returned via this parameter.
  * \param root_paths Root location used throughout Vita3K.
  * \return Success for completion, QuitRequest if Help or Version is requested, otherwise Error.
  */
ExitCode init_config(ConfigState &cfg, int argc, char **argv, const Root &root_paths);

} // namespace config
