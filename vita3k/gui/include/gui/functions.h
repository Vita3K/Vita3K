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

#include <gui/state.h>

#include <cstdint>
#include <string>
#include <vector>

struct DisplayState;
struct HostState;
struct SDL_Window;
struct NpTrophyUnlockCallbackData;

namespace gui {

enum GenericDialogState {
    UNK_STATE,
    CONFIRM_STATE,
    CANCEL_STATE
};

void delete_app(GuiState &gui, HostState &host, const std::string &app_path);
void get_app_info(GuiState &gui, HostState &host, const std::string &app_path);
size_t get_app_size(GuiState &gui, HostState &host, const std::string &app_path);
std::vector<App>::const_iterator get_app_index(GuiState &gui, const std::string &app_path);
std::map<std::string, ImGui_Texture>::const_iterator get_app_icon(GuiState &gui, const std::string &app_path);
std::vector<std::string>::iterator get_app_open_list_index(GuiState &gui, const std::string &app_path);
std::map<std::string, std::string> get_date_time(GuiState &gui, HostState &host, const tm &date_time);
std::string get_unit_size(const size_t &size);
void get_app_param(GuiState &gui, HostState &host, const std::string &app_path);
void get_notice_list(HostState &host);
void get_param_info(HostState &host, const vfs::FileBuffer &param);
void get_time_apps(GuiState &gui, HostState &host);
void get_user_apps_title(GuiState &gui, HostState &host);
void get_users_list(GuiState &gui, HostState &host);
bool get_sys_apps_state(GuiState &gui);
void get_sys_apps_title(GuiState &gui, HostState &host);
void init(GuiState &gui, HostState &host);
void init_app_background(GuiState &gui, HostState &host, const std::string &app_path);
void init_app_icon(GuiState &gui, HostState &host, const std::string &app_path);
void init_apps_icon(GuiState &gui, HostState &host, const std::vector<gui::App> &app_list);
void init_config(GuiState &gui, HostState &host, const std::string &app_path);
void init_content_manager(GuiState &gui, HostState &host);
void init_home(GuiState &gui, HostState &host);
void init_lang(GuiState &gui, HostState &host);
void init_live_area(GuiState &gui, HostState &host);
bool init_manual(GuiState &gui, HostState &host, const std::string &app_path);
void init_notice_info(GuiState &gui, HostState &host);
bool init_theme(GuiState &gui, HostState &host, const std::string &content_id);
void init_themes(GuiState &gui, HostState &host);
void init_theme_start_background(GuiState &gui, HostState &host, const std::string &content_id);
void init_trophy_collection(GuiState &gui, HostState &host);
void init_user(GuiState &gui, HostState &host, const std::string &user_id);
void init_user_app(GuiState &gui, HostState &host, const std::string &app_path);
bool init_user_background(GuiState &gui, HostState &host, const std::string &user_id, const std::string &background_path);
bool init_user_start_background(GuiState &gui, const std::string &image_path);
void open_trophy_unlocked(GuiState &gui, HostState &host, const std::string &np_com_id, const std::string &trophy_id);
void open_user(GuiState &gui, HostState &host);
void pre_load_app(GuiState &gui, HostState &host, bool live_area, const std::string &app_path);
void pre_run_app(GuiState &gui, HostState &host, const std::string &app_path);
bool refresh_app_list(GuiState &gui, HostState &host);
void save_apps_cache(GuiState &gui, HostState &host);
void save_user(GuiState &gui, HostState &host, const std::string &user_id);
void set_config(GuiState &gui, HostState &host, const std::string &app_path);
void update_apps_list_opened(GuiState &gui, HostState &host, const std::string &app_path);
void update_last_time_app_used(GuiState &gui, HostState &host, const std::string &app);
void update_notice_info(GuiState &gui, HostState &host, const std::string &type);
void update_time_app_used(GuiState &gui, HostState &host, const std::string &app);
void save_notice_list(HostState &host);

void draw_begin(GuiState &gui, HostState &host);
void draw_end(GuiState &host, SDL_Window *window);
void draw_live_area(GuiState &gui, HostState &host);
void draw_ui(GuiState &gui, HostState &host);

void draw_app_context_menu(GuiState &gui, HostState &host, const std::string &app_path);
void draw_common_dialog(GuiState &gui, HostState &host);
void draw_ime(Ime &ime, HostState &host);
void draw_reinstall_dialog(GenericDialogState *status, HostState &host);
void draw_trophies_unlocked(GuiState &gui, HostState &host);
void draw_perf_overlay(GuiState &gui, HostState &host);

ImTextureID load_image(GuiState &gui, const char *data, const std::uint32_t size);

} // namespace gui

// Extensions to ImGui
namespace ImGui {

bool vector_getter(void *vec, int idx, const char **out_text);
}
