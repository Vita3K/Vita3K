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

#include <config/config.h>
#include <util/exit_code.h>
#include <util/fs.h>

class Root;

namespace app {

/**
  * \brief Save emulator config to a YML file.
  * \param cfg Config operations to save.
  * \param output_path The location to save the configuration file.
  * \return Success on saving the config file, otherwise Error.
  */
ExitCode serialize_config(Config &cfg, fs::path output_path);

/**
  * \brief Compare and merge two different configurations. This can also reset the configuration.
  * \param lhs The configuration to be modified.
  * \param rhs The new configuration to be added.
  * \param cur_pref_path Default preference path.
  * \param init Used when the configuration is being initialized.
  */
void merge_configs(Config &lhs, const Config &rhs, const std::string &cur_pref_path = std::string{}, bool init = false);

/**
  * \brief Initializes config system, parsing command-line args and handling some basic ones:
  *        --help, --version, --log-level
  * \param cfg Config options are returned via this parameter.
  * \param root_paths Root location used throughout Vita3K.
  * \return Success for completion, QuitRequest if Help or Version is requested, otherwise Error.
  */
ExitCode init_config(Config &cfg, int argc, char **argv, const Root &root_paths);

} // namespace app
