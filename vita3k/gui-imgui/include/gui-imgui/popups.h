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

#include <util/log.h>

#include <string>

struct GuiState;
struct EmuEnvState;

namespace gui {

struct InfoMessage {
    std::string function;
    std::string title;
    spdlog::level::level_enum level = spdlog::level::info;
    std::string msg;
};

void init_info_message(InfoMessage message);
void draw_info_message(GuiState &gui, EmuEnvState &emuenv);
void draw_popups(GuiState &gui, EmuEnvState &emuenv);

} // namespace gui