<<<<<<< Updated upstream
// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
=======
// RPCSV emulator project
// Copyright (C) 2025 RPCSV team
>>>>>>> Stashed changes
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

#include <string>
#include <vector>

struct AppLaunchRequest;
struct EmuEnvState;

ExitCode load_app(int32_t &main_module_id, EmuEnvState &emuenv);
ExitCode load_app(int32_t &main_module_id, EmuEnvState &emuenv, const AppLaunchRequest &launch_request);
ExitCode run_app(EmuEnvState &emuenv, int32_t main_module_id);
ExitCode run_app(EmuEnvState &emuenv, int32_t main_module_id, const AppLaunchRequest &launch_request);
void toggle_texture_replacement(EmuEnvState &emuenv);
void take_screenshot(EmuEnvState &emuenv);
