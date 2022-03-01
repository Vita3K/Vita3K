// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

struct ImVec4;

namespace gui {

#define RGBA_TO_FLOAT(r, g, b, a) ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f)

const ImVec4 GUI_COLOR_TEXT_MENUBAR = RGBA_TO_FLOAT(242, 150, 58, 255);
const ImVec4 GUI_COLOR_TEXT_TITLE = RGBA_TO_FLOAT(247, 198, 51, 255);
const ImVec4 GUI_COLOR_TEXT = RGBA_TO_FLOAT(255, 255, 255, 255);
const ImVec4 GUI_COLOR_TEXT_BLACK = RGBA_TO_FLOAT(0, 0, 0, 255);
const ImVec4 GUI_COLOR_SEARCH_BAR_TEXT = RGBA_TO_FLOAT(200, 200, 200, 255);
const ImVec4 GUI_COLOR_SEARCH_BAR_BG = RGBA_TO_FLOAT(42, 42, 42, 255);
const ImVec4 GUI_SMOOTH_GRAY = ImVec4(0.76f, 0.75f, 0.76f, 1.0f);
const ImVec4 GUI_PROGRESS_BAR_BG = RGBA_TO_FLOAT(82, 82, 91, 30);
const ImVec4 GUI_PROGRESS_BAR = RGBA_TO_FLOAT(141, 180, 83, 255);
const ImVec4 GUI_COMMON_DIALOG_BG = RGBA_TO_FLOAT(30, 30, 30, 255);

// Ime Color
const ImVec4 IME_BUTTON_BG = RGBA_TO_FLOAT(155, 137, 157, 255);
const ImVec4 IME_NUMERIC_BG = RGBA_TO_FLOAT(59, 42, 61, 255);

#undef RGBA_TO_FLOAT

void draw_main_menu_bar(GuiState &gui, HostState &host);
void draw_firmware_install_dialog(GuiState &gui, HostState &host);
void draw_pkg_install_dialog(GuiState &gui, HostState &host);
void draw_archive_install_dialog(GuiState &gui, HostState &host);
void draw_license_install_dialog(GuiState &gui, HostState &host);
void draw_threads_dialog(GuiState &gui, HostState &host);
void draw_thread_details_dialog(GuiState &gui, HostState &host);
void draw_semaphores_dialog(GuiState &gui, HostState &host);
void draw_mutexes_dialog(GuiState &gui, HostState &host);
void draw_lw_mutexes_dialog(GuiState &gui, HostState &host);
void draw_lw_condvars_dialog(GuiState &gui, HostState &host);
void draw_condvars_dialog(GuiState &gui, HostState &host);
void draw_event_flags_dialog(GuiState &gui, HostState &host);
void draw_allocations_dialog(GuiState &gui, HostState &host);
void draw_disassembly_dialog(GuiState &gui, HostState &host);
void draw_settings_dialog(GuiState &gui, HostState &host);
void draw_controls_dialog(GuiState &gui, HostState &host);
void draw_controllers_dialog(GuiState &gui, HostState &host);
void draw_about_dialog(GuiState &gui, HostState &host);
void draw_welcome_dialog(GuiState &gui, HostState &host);

void draw_app_close(GuiState &gui, HostState &host);
void draw_app_selector(GuiState &gui, HostState &host);
void draw_content_manager(GuiState &gui, HostState &host);
void draw_information_bar(GuiState &gui, HostState &host);
void draw_live_area_screen(GuiState &gui, HostState &host);
void draw_manual(GuiState &gui, HostState &host);
void draw_settings(GuiState &gui, HostState &host);
void draw_start_screen(GuiState &gui, HostState &host);
void draw_trophy_collection(GuiState &gui, HostState &host);
void draw_user_management(GuiState &gui, HostState &host);

void reevaluate_code(GuiState &gui, HostState &host);

} // namespace gui
