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

#include <gui/state.h>

#include <host/state.h>

#include <string>

struct Config;
struct HostState;
struct KernelState;
struct MemState;

namespace gui {

enum GenericDialogState {
    UNK_STATE,
    CONFIRM_STATE,
    CANCEL_STATE
};

void init(HostState &host, GuiState &gui);
void init_background(GuiState &gui, const std::string &image_path);
void load_game_background(HostState &host, GuiState &gui, const std::string &title_id);

void draw_begin(HostState &host, GuiState &gui);
void draw_end(SDL_Window *window);
void draw_ui(HostState &host, GuiState &gui);

void draw_common_dialog(HostState &host, GuiState &gui);
void draw_game_selector(HostState &host, GuiState &gui);
void draw_reinstall_dialog(GenericDialogState *status);

} // namespace gui

// Extensions to ImGUi
namespace ImGui {

bool vector_getter(void *vec, int idx, const char **out_text);
}
