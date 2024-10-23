// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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
#include <ime/state.h>
#include <io/vfs.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct Config;
struct EmuEnvState;

namespace gui {

enum GenericDialogState {
    UNK_STATE,
    CONFIRM_STATE,
    CANCEL_STATE
};

void browse_home_apps_list(GuiState &gui, EmuEnvState &emuenv, const uint32_t button);
void browse_live_area_apps_list(GuiState &gui, EmuEnvState &emuenv, const uint32_t button);
void browse_pages_manual(GuiState &gui, EmuEnvState &emuenv, const uint32_t button);
void browse_save_data_dialog(GuiState &gui, EmuEnvState &emuenv, const uint32_t button);
void browse_users_management(GuiState &gui, EmuEnvState &emuenv, const uint32_t button);
void close_and_run_new_app(EmuEnvState &emuenv, const std::string &app_path);
void close_live_area_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void close_system_app(GuiState &gui, EmuEnvState &emuenv);
void delete_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void destroy_bgm_player();
void erase_app_notice(GuiState &gui, const std::string &title_id);
void get_app_info(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
size_t get_app_size(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
App *get_app_index(GuiState &gui, const std::string &app_path);
std::map<std::string, ImGui_Texture>::const_iterator get_app_icon(GuiState &gui, const std::string &app_path);
std::vector<std::string>::iterator get_live_area_current_open_apps_list_index(GuiState &gui, const std::string &app_path);
std::map<DateTime, std::string> get_date_time(GuiState &gui, EmuEnvState &emuenv, const tm &date_time);
std::string get_unit_size(const size_t size);
void get_app_param(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void get_firmware_file(EmuEnvState &emuenv);
void get_modules_list(GuiState &gui, EmuEnvState &emuenv);
void get_notice_list(EmuEnvState &emuenv);
std::string get_theme_title_from_buffer(const vfs::FileBuffer &buffer);
std::vector<TimeApp>::iterator get_time_app_index(GuiState &gui, EmuEnvState &emuenv, const std::string &app);
void get_time_apps(GuiState &gui, EmuEnvState &emuenv);
void get_user_apps_title(GuiState &gui, EmuEnvState &emuenv);
void get_users_list(GuiState &gui, EmuEnvState &emuenv);
bool get_sys_apps_state(GuiState &gui);
void get_sys_apps_title(GuiState &gui, EmuEnvState &emuenv);
std::string get_sys_lang_name(uint32_t lang_id);
void init(GuiState &gui, EmuEnvState &emuenv);
void init_app_background(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void init_app_icon(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void init_apps_icon(GuiState &gui, EmuEnvState &emuenv, const std::vector<gui::App> &app_list);
bool init_bgm(EmuEnvState &emuenv, const std::pair<std::string, std::string> &path_bgm);
void init_bgm_player(const float vol);
void init_config(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void init_content_manager(GuiState &gui, EmuEnvState &emuenv);
vfs::FileBuffer init_default_icon(GuiState &gui, EmuEnvState &emuenv);
void init_home(GuiState &gui, EmuEnvState &emuenv);
void init_live_area(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
bool init_manual(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void init_notice_info(GuiState &gui, EmuEnvState &emuenv);
bool init_theme(GuiState &gui, EmuEnvState &emuenv, const std::string &content_id);
void init_theme_start_background(GuiState &gui, EmuEnvState &emuenv, const std::string &content_id);
void init_last_time_apps(GuiState &gui, EmuEnvState &emuenv);
void init_trophy_collection(GuiState &gui, EmuEnvState &emuenv);
void init_user(GuiState &gui, EmuEnvState &emuenv, const std::string &user_id);
void init_user_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void init_user_apps(GuiState &gui, EmuEnvState &emuenv);
bool init_user_background(GuiState &gui, EmuEnvState &emuenv, const std::string &background_path);
bool init_user_backgrounds(GuiState &gui, EmuEnvState &emuenv);
void init_user_management(GuiState &gui, EmuEnvState &emuenv);
bool init_user_start_background(GuiState &gui, const std::string &image_path);
void load_and_update_compat_user_apps(GuiState &gui, EmuEnvState &emuenv);
void open_live_area(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void open_manual(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void open_path(const std::string &path);
void open_search(const std::string &title);
void open_trophy_unlocked(GuiState &gui, EmuEnvState &emuenv, const std::string &np_com_id, const std::string &trophy_id);
void open_user(GuiState &gui, EmuEnvState &emuenv);
bool init_vita3k_update(GuiState &gui);
void pre_init(GuiState &gui, EmuEnvState &emuenv);
void pre_load_app(GuiState &gui, EmuEnvState &emuenv, bool live_area, const std::string &app_path);
void pre_run_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void reset_controller_binding(EmuEnvState &emuenv);
void save_apps_cache(GuiState &gui, EmuEnvState &emuenv);
void save_user(GuiState &gui, EmuEnvState &emuenv, const std::string &user_id);
void select_app(GuiState &gui, const std::string &title_id);
void set_bgm_volume(const float vol);
void set_config(EmuEnvState &emuenv);
void set_current_config(EmuEnvState &emuenv, const std::string &app_path);
bool set_scroll_animation(float &scroll, float target_scroll, const std::string &target_id, std::function<void(float)> set_scroll);
void set_shaders_compiled_display(GuiState &gui, EmuEnvState &emuenv);
void stop_bgm();
void switch_bgm_state(const bool pause);
void update_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void update_last_time_app_used(GuiState &gui, EmuEnvState &emuenv, const std::string &app);
void update_live_area_current_open_apps_list(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void update_notice_info(GuiState &gui, EmuEnvState &emuenv, const std::string &type);
void update_time_app_used(GuiState &gui, EmuEnvState &emuenv, const std::string &app);
void save_notice_list(EmuEnvState &emuenv);
void set_controller_overlay_state(int overlay_mask, bool edit = false, bool reset = false);
void set_controller_overlay_scale(float scale);
void set_controller_overlay_opacity(int opacity);
int get_overlay_display_mask(const Config &cfg);

void draw_begin(GuiState &gui, EmuEnvState &emuenv);
void draw_end(GuiState &gui);
void draw_vita_area(GuiState &gui, EmuEnvState &emuenv);
void draw_ui(GuiState &gui, EmuEnvState &emuenv);

void draw_app_context_menu(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void draw_background(GuiState &gui, EmuEnvState &emuenv);
void draw_common_dialog(GuiState &gui, EmuEnvState &emuenv);
void draw_ime(Ime &ime, EmuEnvState &emuenv);
void draw_info_message(GuiState &gui, EmuEnvState &emuenv);
void draw_initial_setup(GuiState &gui, EmuEnvState &emuenv);
void draw_reinstall_dialog(GenericDialogState *status, GuiState &gui, EmuEnvState &emuenv);
void draw_pre_compiling_shaders_progress(GuiState &gui, EmuEnvState &emuenv, const uint32_t &total);
void draw_shaders_count_compiled(GuiState &gui, EmuEnvState &emuenv);
void draw_trophies_unlocked(GuiState &gui, EmuEnvState &emuenv);
void draw_touchpad_cursor(EmuEnvState &emuenv);
void draw_perf_overlay(GuiState &gui, EmuEnvState &emuenv);

ImTextureID load_image(GuiState &gui, const uint8_t *data, const int size);

} // namespace gui

// Extensions to ImGui
namespace ImGui {

void ScrollWhenDragging();
} // namespace ImGui
