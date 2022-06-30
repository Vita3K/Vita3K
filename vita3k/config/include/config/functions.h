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

#include <util/exit_code.h>
#include <util/fs.h>

struct Config;
class Root;

namespace config {

/**
 * \brief Save emulator config to a YML file. Call this function if the YAML file needs to be updated.
 * \param cfg Config operations to save.
 * \param output_path The location to save the configuration file.
 * \return Success on saving the config file, otherwise Error.
 */
ExitCode serialize_config(Config &cfg, const fs::path &output_path);

/**
 * \brief Initializes config system, parsing command-line args and handling some basic ones:
 *        --help, --version, --log-level
 * \param cfg Config options are returned via this parameter.
 * \param root_paths Root location used throughout Vita3K.
 * \return Success for completion, QuitRequest if Help or Version is requested, otherwise Error.
 */
ExitCode init_config(Config &cfg, int argc, char **argv, const Root &root_paths);

#ifdef TRACY_ENABLE
/**
 * @brief Detect the Tracy advanced profiling activation state for a given HLE module
 *
 * @param active_modules Vector meant to contain the names of every module with Tracy advanced profiling enabled
 * @param module Name of the module to check the activation state for
 * @param index Variable where to store the calculated index where the name of the module is stored in
 * `emuenv.cfg.tracy_advanced_profiling_modules`. Useful to save on `std::find()` calls. It is recommended to initialize
 * the variable with `-1` before passing it to this function.
 * @return true Advanced profiling using Tracy is enabled for the module
 * @return false Advanced profiling using Tracy is not enabled for the module or module isn't available
 * for advanced profiling.
 */
bool is_tracy_advanced_profiling_active_for_module(std::vector<std::string> &active_modules, std::string module, int *index = nullptr);
#endif // TRACY_ENABLE

} // namespace config
