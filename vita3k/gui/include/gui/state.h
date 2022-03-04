﻿// Vita3K emulator project
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

#include <config/config.h>
#include <dialog/state.h>
#include <ime/state.h>
#include <lang/state.h>
#include <np/state.h>

#include <imgui.h>
#include <imgui_memory_editor.h>

#include <gui/imgui_impl_sdl_state.h>

#include <glutil/object.h>

#include <atomic>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

struct GuiState;
struct HostState;

namespace gui {

enum SelectorState {
    SELECT_APP
};

enum SortState {
    NOT_SORTED,
    ASCENDANT,
    DESCENDANT
};

enum SortType {
    APP_VER,
    CATEGORY,
    LAST_TIME,
    TITLE,
    TITLE_ID
};

struct App {
    std::string app_ver;
    std::string category;
    std::string content_id;
    std::string addcont;
    std::string savedata;
    std::string parental_level;
    std::string stitle;
    std::string title;
    std::string title_id;
    std::string path;
    time_t last_time;
};

struct AppInfo {
    std::string trophy;
    tm updated;
    size_t size;
};

struct IconData {
    int32_t width = 0;
    int32_t height = 0;

    std::unique_ptr<void, void (*)(void *)> data;

    IconData();
};

struct IconAsyncLoader {
    std::mutex mutex;

    std::unordered_map<std::string, IconData> icon_data;

    std::thread thread;
    std::atomic_bool quit = false;

    void commit(GuiState &gui);

    IconAsyncLoader(GuiState &gui, HostState &host, const std::vector<gui::App> &app_list);
    ~IconAsyncLoader();
};

struct AppsSelector {
    std::vector<App> sys_apps;
    std::vector<App> user_apps;
    uint32_t apps_cache_lang;
    AppInfo app_info;
    std::optional<IconAsyncLoader> icon_async_loader;
    std::map<std::string, ImGui_Texture> sys_apps_icon;
    std::map<std::string, ImGui_Texture> user_apps_icon;
    bool is_app_list_sorted{ false };
    std::map<SortType, SortState> app_list_sorted;
    SelectorState state = SELECT_APP;
};

struct LiveAreaState {
    bool app_close = false;
    bool app_information = false;
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
    bool license_install_dialog = false;
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
    bool custom_settings_dialog = false;
    bool settings_dialog = false;
};

struct ControlMenuState {
    bool controls_dialog = false;
    bool controllers_dialog = false;
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

enum DateTime {
    CLOCK,
    DATE_DETAIL,
    DATE_MINI,
    DAY_MOMENT,
    HOUR,
};

struct User {
    std::string id;
    std::string name = "Vita3K";
    std::string avatar = "default";
    gui::SortType sort_apps_type = gui::TITLE;
    gui::SortState sort_apps_state = gui::ASCENDANT;
    std::string theme_id = "default";
    bool use_theme_bg = true;
    std::string start_type = "default";
    std::string start_path;
    std::vector<std::string> backgrounds;
};

struct TimeApp {
    std::string app;
    time_t last_time_used;
    int64_t time_used;
};

struct InfoBarColor {
    ImU32 bar = 0xFF000000;
    ImU32 indicator = 0xFFFFFFFF;
    ImU32 notice_font = 0xFFFFFFFF;
};

enum NoticeIcon {
    NO,
    NEW
};

enum ModulesModeType {
    MODE,
    DESCRIPTION,
};

enum ThemePreviewType {
    PACKAGE,
    HOME,
    LOCK,
};

enum ShadersCompiledDisplay {
    Time,
    Count
};

static constexpr auto MODULES_MODE_COUNT = 3;
using ConfigModuleMode = std::array<std::vector<const char *>, MODULES_MODE_COUNT>;

inline ConfigModuleMode init_modules_mode() {
    ConfigModuleMode m;

    m[ModulesMode::AUTOMATIC] = { "Automatic", "Select Automatic mode to use a preset list of modules." };
    m[ModulesMode::AUTO_MANUAL] = { "Auto & Manual", "Select this mode to load Automatic module and selected modules from the list below." };
    m[ModulesMode::MANUAL] = { "Manual", "Select Manual mode to load selected modules from the list below." };

    return m;
}

const ConfigModuleMode config_modules_mode = init_modules_mode();

const std::vector<std::pair<SceSystemParamLang, std::string>> LIST_SYS_LANG = {
    { SCE_SYSTEM_PARAM_LANG_DANISH, "Dansk" },
    { SCE_SYSTEM_PARAM_LANG_GERMAN, "Deutsch" },
    { SCE_SYSTEM_PARAM_LANG_ENGLISH_GB, "English (United Kingdom)" },
    { SCE_SYSTEM_PARAM_LANG_ENGLISH_US, "English (United States)" },
    { SCE_SYSTEM_PARAM_LANG_SPANISH, u8"Español" },
    { SCE_SYSTEM_PARAM_LANG_FRENCH, u8"Français" },
    { SCE_SYSTEM_PARAM_LANG_ITALIAN, "Italiano" },
    { SCE_SYSTEM_PARAM_LANG_DUTCH, "Nederlands" },
    { SCE_SYSTEM_PARAM_LANG_NORWEGIAN, "Norsk" },
    { SCE_SYSTEM_PARAM_LANG_POLISH, "Polskis" },
    { SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR, u8"Português (Brasil)" },
    { SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT, u8"Português (Portugal)" },
    { SCE_SYSTEM_PARAM_LANG_RUSSIAN, u8"Русский" },
    { SCE_SYSTEM_PARAM_LANG_FINNISH, "Suomi" },
    { SCE_SYSTEM_PARAM_LANG_SWEDISH, "Svenska" },
    { SCE_SYSTEM_PARAM_LANG_TURKISH, u8"Türkçe" },
    { SCE_SYSTEM_PARAM_LANG_JAPANESE, u8"日本語" },
    { SCE_SYSTEM_PARAM_LANG_KOREAN, "Korean" },
    { SCE_SYSTEM_PARAM_LANG_CHINESE_S, "Chinese - Simplified" },
    { SCE_SYSTEM_PARAM_LANG_CHINESE_T, "Chinese - Traditional" },
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

    LangState lang;

    std::map<std::string, User> users;
    std::map<std::string, ImGui_Texture> users_avatar;

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

    GLuint display = 0;

    ImGuiTextFilter app_search_bar;

    std::vector<std::string> apps_list_opened;
    int32_t current_app_selected = -1;

    std::map<std::string, std::vector<TimeApp>> time_apps;

    std::uint64_t current_theme_bg;
    std::map<std::string, std::map<ThemePreviewType, ImGui_Texture>> themes_preview;
    std::vector<ImGui_Texture> theme_backgrounds;
    std::vector<ImVec4> theme_backgrounds_font_color;
    std::map<NoticeIcon, ImGui_Texture> theme_information_bar_notice;

    std::map<time_t, ImGui_Texture> notice_info_icon;

    std::uint64_t current_user_bg;
    std::map<std::string, ImGui_Texture> user_backgrounds;

    std::map<std::string, std::map<std::string, ImGui_Texture>> trophy_np_com_id_list_icons;
    std::map<std::string, ImGui_Texture> trophy_list;

    ImGui_Texture start_background;

    std::map<std::string, ImGui_Texture> apps_background;

    InfoBarColor information_bar_color;

    std::map<std::string, std::map<std::string, ImGui_Texture>> live_area_contents;
    std::map<std::string, std::map<std::string, std::map<std::string, std::vector<ImGui_Texture>>>> live_items;

    std::vector<ImGui_Texture> manuals;

    std::map<ShadersCompiledDisplay, uint64_t> shaders_compiled_display;

    SceUID thread_watch_index = -1;

    std::uint32_t trophy_window_frame_count{ 0 };
    TrophyAnimationStage trophy_window_frame_stage{ TrophyAnimationStage::SLIDE_IN };
    ImTextureID trophy_window_icon{};

    std::vector<NpTrophyUnlockCallbackData> trophy_unlock_display_requests;
    std::mutex trophy_unlock_display_requests_access_mutex;

    ImVec2 trophy_window_pos;

    // imgui
    ImFont *monospaced_font{};
    ImFont *vita_font{};
    ImFont *large_font{};
    bool fw_font = false;
};
