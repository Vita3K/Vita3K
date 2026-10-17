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

#include <config/settings.h>
#include <gui-imgui/state.h>
#include <ime/state.h>
#include <io/vfs.h>
#include <util/exit_code.h>
#include <v3kn/friend.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct Config;
struct EmuEnvState;
struct SocialEvent;

namespace net_utils {
struct WebResponse;
}

namespace v3kn {
struct Error;
}

namespace app {
class AppSessionController;
}

namespace gui {

inline bool is_emu_app(const std::string &path) {
    return path.starts_with("emu:");
}

inline std::string app_launch_path(const std::string &path) {
    return path == "emu:vsh/shell" ? "vs0:vsh/shell" : path;
}

inline std::string app_ui_path(const std::string &path) {
    return path == "vs0:vsh/shell" ? "emu:vsh/shell" : path;
}

inline std::string emu_app_path(const std::string &id) {
    return id == "NPXS19999" ? "emu:vsh/shell" : "emu:app/" + id;
}

inline std::string app_asset_path(const std::string &path) {
    if (path == "emu:vsh/shell")
        return "vs0:vsh/shell";
    return path.starts_with("emu:app/") ? "vs0:app/" + path.substr(8) : path;
}

enum GenericDialogState {
    UNK_STATE,
    CONFIRM_STATE,
    CANCEL_STATE
};

void begin_update_download(GuiState &gui, EmuEnvState &emuenv, const std::string &id);
void browse_settings(GuiState &gui, EmuEnvState &emuenv, const uint32_t button);
void browse_home_apps_list(GuiState &gui, EmuEnvState &emuenv, const uint32_t button, app::AppSessionController *session = nullptr);
void browse_live_area_apps_list(GuiState &gui, EmuEnvState &emuenv, const uint32_t button, app::AppSessionController *session = nullptr);
void browse_pages_manual(GuiState &gui, EmuEnvState &emuenv, const uint32_t button);
void browse_users_management(GuiState &gui, EmuEnvState &emuenv, const uint32_t button);
std::optional<bool> check_has_patch(EmuEnvState &emuenv, const std::string &title_id, const std::string &current_version);
void close_and_run_new_app(EmuEnvState &emuenv, const std::string &app_path);
void close_live_area_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void close_start_screen(GuiState &gui, EmuEnvState &emuenv);
void close_system_app(GuiState &gui, EmuEnvState &emuenv);
void commit_friend_avatars(GuiState &gui, EmuEnvState &emuenv);
ImVec4 compatibility_color(compat::CompatibilityState state);
std::optional<gui::IconData> decode_v3kn_image(const v3kn::ImageData &image, const std::string &online_id);
void delete_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path, app::AppSessionController *session = nullptr);
void destroy_download_manager();
void destroy_pkg_install_thread();
void destroy_update_check_thread(GuiState &gui);
void erase_app_notice(GuiState &gui, const std::string &title_id);
const ImGui_Texture *get_app_icon(GuiState &gui, const std::string &app_path);
bool has_live_area_update(GuiState &gui, const std::string &app_path, const std::string &title_id);
std::optional<App> get_app_index(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void get_app_info(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
size_t get_app_size(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
std::map<DateTime, std::string> get_date_time(GuiState &gui, EmuEnvState &emuenv, const tm &date_time);
std::vector<config::RestartRequiredSetting> get_emulator_restart_required_settings(
    const Config::CurrentConfig &before,
    const Config::CurrentConfig &after);
void get_firmware_file(EmuEnvState &emuenv);
std::vector<std::string>::iterator get_live_area_current_open_apps_list_index(GuiState &gui, const std::string &app_path);
bool get_local_update_history(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path, const std::string &title_id);
void get_notice_list(EmuEnvState &emuenv);
int get_overlay_display_mask(const Config &cfg);
ImU32 get_panel_edge_dominant_color(const gui::IconData &panel_data);
bool get_remote_update_info(GuiState &gui, EmuEnvState &emuenv, const std::string &id);
ImU32 get_selectable_color_pulse(const float max_alpha = 255.f);
bool get_sys_apps_state(GuiState &gui);
void get_sys_apps_title(GuiState &gui, EmuEnvState &emuenv);
std::string get_sys_lang_name(uint32_t lang_id);
std::string get_theme_title_from_buffer(const vfs::FileBuffer &buffer);
std::string get_unit_size(const size_t size);
std::string get_v3kn_error_message(const LangState &lang, const net_utils::WebResponse &response);
std::string get_v3kn_error_message(const LangState &lang, const v3kn::Error &error);
bool has_available_modules();
void init(GuiState &gui, EmuEnvState &emuenv);
void init_app_background(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void init_app_icon(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void init_apps_icon(GuiState &gui, EmuEnvState &emuenv, const std::vector<gui::App> &app_list);
void init_config(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void init_content_manager(GuiState &gui, EmuEnvState &emuenv);
vfs::FileBuffer init_default_icon(GuiState &gui, EmuEnvState &emuenv);
void init_home(GuiState &gui, EmuEnvState &emuenv);
void init_ime_lang(Ime &ime, const SceImeLanguage &lang);
void init_live_area(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
bool init_manual(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void init_notice_info(GuiState &gui, EmuEnvState &emuenv);
bool init_theme(GuiState &gui, EmuEnvState &emuenv, const std::string &content_id);
bool apply_theme(GuiState &gui, EmuEnvState &emuenv, const std::string &content_id);
void init_theme_start_background(GuiState &gui, EmuEnvState &emuenv, const std::string &content_id);
void init_trophy_collection(GuiState &gui, EmuEnvState &emuenv);
void init_user(GuiState &gui, EmuEnvState &emuenv, const std::string &user_id);
void init_user_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void init_user_apps(GuiState &gui, EmuEnvState &emuenv);
bool init_user_background(GuiState &gui, EmuEnvState &emuenv, const std::string &background_path);
bool init_user_backgrounds(GuiState &gui, EmuEnvState &emuenv);
void init_user_management(GuiState &gui, EmuEnvState &emuenv);
bool init_user_start_background(GuiState &gui, const std::string &image_path);
void init_users_avatars(GuiState &gui, EmuEnvState &emuenv);
void init_v3kn_user_info(GuiState &gui, EmuEnvState &emuenv);
bool init_vita3k_update(GuiState &gui);
void start_vita3k_update_check();
void poll_vita3k_update_check(GuiState &gui);
void load_and_update_compat_user_apps(GuiState &gui, EmuEnvState &emuenv);
void load_fonts(GuiState &gui, EmuEnvState &emuenv, bool reload);
ImTextureID load_image(GuiState &gui, const uint8_t *data, const int size);
void open_friend(GuiState &gui, EmuEnvState &emuenv);
void open_friend_activity(GuiState &gui, EmuEnvState &emuenv, time_t created_at, const std::string &online_id);
void open_friend_profile(GuiState &gui, EmuEnvState &emuenv, const std::string &online_id);
void open_friend_requests(GuiState &gui, EmuEnvState &emuenv);
void open_live_area(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void open_manual(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void open_online_storage(GuiState &gui, EmuEnvState &emuenv, const std::string &titleid);
void open_path(const std::string &path);
void open_search(const std::string &title);
void open_trophy_unlocked(GuiState &gui, EmuEnvState &emuenv, const std::string &np_com_id, const std::string &trophy_id);
void open_user(GuiState &gui, EmuEnvState &emuenv);
void pkg_install(GuiState &gui, EmuEnvState &emuenv, const fs::path &pkg_path, const std::string &zrif, const std::string &id = {});
void pre_init(GuiState &gui, EmuEnvState &emuenv);
void pre_load_app(GuiState &gui, EmuEnvState &emuenv, bool live_area, const std::string &app_path, app::AppSessionController *session = nullptr);
void pre_run_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path, app::AppSessionController *session = nullptr);
void process_live_area_update(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void process_pending_notice_updates(GuiState &gui, EmuEnvState &emuenv);
void add_social_notice(GuiState &gui, EmuEnvState &emuenv, const SocialEvent &event);
void push_update_notification(GuiState &gui, const std::string &id, const std::string &title, const std::string &message);
void queue_notice_info(const std::string &type, const std::string &id = {});
void refresh_and_check_patch(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void refresh_current_friends_screen(GuiState &gui, EmuEnvState &emuenv, bool reload_avatar = false);
void refresh_live_area(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void refresh_modules_list(const EmuEnvState &emuenv);
void reset_controller_binding(EmuEnvState &emuenv);
// Owns the ImGui window and its renderer resources until the frontend exits.
ExitCode run_frontend(EmuEnvState &emuenv, char *argv[] = nullptr);
void save_notice_list(EmuEnvState &emuenv);
void select_app(GuiState &gui, EmuEnvState &emuenv, const std::string &title_id);
void set_controller_overlay_opacity(int opacity);
void set_controller_overlay_scale(float scale);
void set_controller_overlay_state(int overlay_mask, bool edit = false, bool reset = false);
bool set_scroll_animation(float &scroll, float target_scroll, const std::string &target_id, std::function<void(float)> set_scroll);
void update_install(GuiState &gui, EmuEnvState &emuenv, const std::string &id);
void update_live_area_current_open_apps_list(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path);
void update_notice_info(GuiState &gui, EmuEnvState &emuenv, const std::string &type, const std::string &id = {});

void draw_app_context_menu(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path, app::AppSessionController *session);
void draw_background(GuiState &gui, EmuEnvState &emuenv, bool is_image_background = true);
void draw_begin(GuiState &gui, EmuEnvState &emuenv);
void draw_compatibility_badge(compat::CompatibilityState state, const ImVec2 &position, float compat_radius, float full_compat_radius);
void draw_ellipsis_text(const std::string &text, float wrap_width, ImVec2 init_pos, ImVec2 align, const ImVec4 &col, uint32_t max_lines = 2);
void draw_end(GuiState &gui);
void draw_friend_status_dot(ImDrawList *draw_list, const ImVec2 &pos, const ImVec2 &scale, PresenceStatus presence_status);
void draw_ime(GuiState &gui, Ime &ime, EmuEnvState &emuenv);
void draw_initial_setup(GuiState &gui, EmuEnvState &emuenv);
void draw_notifications(GuiState &gui, EmuEnvState &emuenv);
void draw_pkg_install(GuiState &gui, EmuEnvState &emuenv);
void draw_stretchable_horizontal_image(ImDrawList *draw_list, ImTextureID texture, const ImVec2 &pos, const ImVec2 &size,
    float texture_width, float texture_height, float left_slice_width, float right_slice_width, ImU32 tint = IM_COL32_WHITE,
    float source_x = 0.f, float source_y = 0.f, float source_width = 0.f, float source_height = 0.f);
void draw_stretchable_9slice_image(ImDrawList *draw_list, ImTextureID texture, const ImVec2 &pos, const ImVec2 &size,
    float texture_width, float texture_height, float left_slice_width, float right_slice_width, float top_slice_height, float bottom_slice_height, ImU32 tint = IM_COL32_WHITE,
    float source_x = 0.f, float source_y = 0.f, float source_width = 0.f, float source_height = 0.f,
    float dest_left_width = -1.f, float dest_right_width = -1.f, float dest_top_height = -1.f, float dest_bottom_height = -1.f);
void draw_touchpad_cursor(GuiState &gui, EmuEnvState &emuenv);
void draw_ui(GuiState &gui, EmuEnvState &emuenv, app::AppSessionController *session);
void draw_vita_area(GuiState &gui, EmuEnvState &emuenv, app::AppSessionController *session);

} // namespace gui

// Extensions to ImGui
namespace ImGui {

void ScrollWhenDragging();
} // namespace ImGui
