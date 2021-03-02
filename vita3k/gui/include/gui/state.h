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

#include <dialog/state.h>
#include <np/state.h>

#include <imgui.h>
#include <imgui_memory_editor.h>

#include <gui/imgui_impl_sdl_state.h>

#include <glutil/object.h>

#include <queue>
#include <vector>

namespace gui {

enum SelectorState {
    SELECT_APP
};

enum SortState {
    NOT_SORTED,
    ASCENDANT,
    DESCENDANT
};

struct App {
    std::string app_ver;
    std::string category;
    std::string parental_level;
    std::string stitle;
    std::string title;
    std::string title_id;
    std::string path;
};

struct AppInfo {
    std::string trophy;
    tm updated;
    size_t size;
};

struct AppsSelector {
    std::vector<App> sys_apps;
    std::vector<App> user_apps;
    AppInfo app_info;
    std::map<std::string, ImGui_Texture> sys_apps_icon;
    std::map<std::string, ImGui_Texture> user_apps_icon;
    bool is_app_list_sorted{ false };
    SortState title_id_sort_state = NOT_SORTED;
    SortState app_ver_sort_state = NOT_SORTED;
    SortState category_sort_state = NOT_SORTED;
    SortState title_sort_state = NOT_SORTED;
    SelectorState state = SELECT_APP;
};

struct LiveAreaState {
    bool app_selector = false;
    bool content_manager = false;
    bool information_bar = false;
    bool live_area_screen = false;
    bool manual = false;
    bool settings = false;
    bool start_screen = false;
    bool trophy_collection = false;
    bool user_management = false;
};

struct FileMenuState {
    bool firmware_install_dialog = false;
    bool pkg_install_dialog = false;
    bool archive_install_dialog = false;
};

struct DebugMenuState {
    bool threads_dialog = false;
    bool thread_details_dialog = false;
    bool semaphores_dialog = false;
    bool condvars_dialog = false;
    bool lwcondvars_dialog = false;
    bool mutexes_dialog = false;
    bool lwmutexes_dialog = false;
    bool eventflags_dialog = false;
    bool allocations_dialog = false;
    bool memory_editor_dialog = false;
    bool disassembly_dialog = false;
};

struct ConfigurationMenuState {
    bool settings_dialog = false;
};

struct ControlMenuState {
    bool controls_dialog = false;
};

struct HelpMenuState {
    bool about_dialog = false;
    bool welcome_dialog = false;
};

} // namespace gui

enum class TrophyAnimationStage {
    SLIDE_IN = 0,
    STATIC = 1,
    SLIDE_OUT = 2,
    END = 3
};

struct NoticeInfo {
    std::string id;
    std::string group;
    std::string type;
    size_t pos;
    time_t date;
    std::string name;
    std::string msg;
};

enum DateFormat {
    YYYY_MM_DD,
    DD_MM_YYYY,
    MM_DD_YYYY
};

struct User {
    std::string id;
    std::string name;
    DateFormat date_format;
    bool clock_12_hour;
    std::string avatar;
    std::string theme_id;
    bool use_theme_bg;
    std::string start_type;
    std::string start_path;
    std::vector<std::string> backgrounds;
};

struct Lang {
    std::string user_lang;
    std::map<std::string, std::string> main_menubar;
    std::map<std::string, std::string> app_context;
    std::map<std::string, std::string> indicator;
    std::map<std::string, std::string> settings;
    std::map<std::string, std::string> user_management;
    std::vector<std::string> wday;
    std::vector<std::string> ymonth;
};

struct GuiState {
    std::unique_ptr<ImGui_State> imgui_state;

    bool renderer_focused = true;
    gui::FileMenuState file_menu;
    gui::DebugMenuState debug_menu;
    gui::ConfigurationMenuState configuration_menu;
    gui::ControlMenuState controls_menu;
    gui::HelpMenuState help_menu;
    gui::LiveAreaState live_area;
    gui::AppsSelector app_selector;

    Lang lang;

    std::map<std::string, User> users;
    std::map<std::string, ImGui_Texture> users_avatar;

    std::string app_ver;
    bool content_reinstall_confirm = false;

    MemoryEditor memory_editor;
    MemoryEditor gxp_shader_editor;
    size_t memory_editor_start = 0;
    size_t memory_editor_count = 0;

    std::string disassembly_arch = "THUMB";
    char disassembly_address[9] = "00000000";
    char disassembly_count[5] = "100";
    std::vector<std::string> disassembly;

    bool is_capturing_keys = false;
    int old_captured_key = 0;
    int captured_key = 0;

    std::vector<std::pair<std::string, bool>> modules;
    ImGuiTextFilter module_search_bar;

    bool delete_app_icon = false;
    GLuint display = 0;

    ImGuiTextFilter app_search_bar;

    std::vector<std::string> apps_list_opened;
    int32_t current_app_selected = -1;

    std::uint64_t current_theme_bg;
    std::map<std::string, std::map<std::string, ImGui_Texture>> themes_preview;
    std::vector<ImGui_Texture> theme_backgrounds;
    std::vector<ImVec4> theme_backgrounds_font_color;
    std::map<std::string, ImGui_Texture> theme_information_bar_notice;

    std::map<size_t, bool> notice_info_new;
    int notice_info_count_new = 0;
    std::vector<NoticeInfo> notice_info;
    std::map<size_t, ImGui_Texture> notice_info_icon;

    std::uint64_t current_user_bg;
    std::map<std::string, ImGui_Texture> user_backgrounds;

    std::map<std::string, std::map<std::string, ImGui_Texture>> trophy_np_com_id_list_icons;
    std::map<std::string, ImGui_Texture> trophy_list;

    ImGui_Texture start_background;

    std::map<std::string, ImGui_Texture> apps_background;

    std::map<std::string, ImU32> information_bar_color;

    std::map<std::string, std::map<std::string, ImGui_Texture>> live_area_contents;
    std::map<std::string, std::map<std::string, std::map<std::string, std::vector<ImGui_Texture>>>> live_items;

    std::vector<ImGui_Texture> manuals;

    SceUID thread_watch_index = -1;

    std::uint32_t trophy_window_frame_count{ 0 };
    TrophyAnimationStage trophy_window_frame_stage{ TrophyAnimationStage::SLIDE_IN };
    ImTextureID trophy_window_icon{};

    std::queue<NpTrophyUnlockCallbackData> trophy_unlock_display_requests;
    std::mutex trophy_unlock_display_requests_access_mutex;

    ImVec2 trophy_window_pos;

    // imgui
    ImFont *monospaced_font{};
    ImFont *vita_font{};
    ImFont *large_font{};
    bool fw_font = false;
};
