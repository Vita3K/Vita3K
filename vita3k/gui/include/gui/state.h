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

#include <app/state.h>
#include <compat/state.h>
#include <config/config.h>
#include <emuenv/state.h>
#include <gui/lang_state.h>
#include <lang/state.h>
#include <np/state.h>
#include <util/info_message.h>
#include <util/log.h>

#include <array>

#include <imgui.h>
#include <imgui_memory_editor.h>

#include <gui/imgui_impl_sdl_state.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

struct GuiState;
struct EmuEnvState;

namespace gui {

using SortState = app::UserSortState;
using SortType = app::UserSortType;
using app::APP_VER;
using app::ASCENDANT;
using app::CATEGORY;
using app::COMPAT;
using app::DESCENDANT;
using app::LAST_TIME;
using app::NOT_SORTED;
using app::TITLE;
using app::TITLE_ID;

using App = app::AppEntry;

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

    IconAsyncLoader(GuiState &gui, EmuEnvState &emuenv, const std::vector<gui::App> &app_list);
    ~IconAsyncLoader();
};

struct AppsSelector {
    std::vector<App> sys_apps;
    AppInfo app_info;
    std::optional<IconAsyncLoader> icon_async_loader;
    std::map<std::string, ImGui_Texture> sys_apps_icon;
    std::map<std::string, ImGui_Texture> user_apps_icon;
    bool is_app_list_sorted{ false };
    std::map<SortType, SortState> app_list_sorted;
};

struct VitaAreaState {
    bool app_close = false;
    bool app_information = false;
    bool content_manager = false;
    bool connecting_please_wait = false;
    bool home_screen = false;
    bool information_bar = false;
    bool live_area_screen = false;
    bool manual = false;
    bool online_storage = false;
    bool please_wait = false;
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
    bool v3kn_dialog = false;
};

struct ControlMenuState {
    bool overlay_dialog = false;
    bool controls_dialog = false;
    bool controllers_dialog = false;
};

struct HelpMenuState {
    bool about_dialog = false;
    bool vita3k_update = false;
    bool welcome_dialog = false;
};

} // namespace gui

enum DateTime {
    CLOCK,
    DATE_DETAIL,
    DATE_MINI,
    DAY_MOMENT,
    HOUR,
};

enum class AvatarSize : uint32_t {
    SMALL = 34,
    MEDIUM = 130,
    LARGE = 160,
    V3KN = 80
};

struct AvatarInfo {
    ImVec2 pos;
    ImVec2 size;
};

using User = app::User;

struct InfoBarColor {
    ImU32 bar = 0xFF000000;
    ImU32 indicator = 0xFFFFFFFF;
    ImU32 notice_font = 0xFFFFFFFF;
};

enum NoticeIcon {
    NO,
    NEW
};

enum ThemePreviewType {
    PACKAGE,
    HOME,
    LOCK,
};

inline const std::vector<std::pair<SceSystemParamLang, std::string>> LIST_SYS_LANG = {
    { SCE_SYSTEM_PARAM_LANG_DANISH, "Dansk" },
    { SCE_SYSTEM_PARAM_LANG_GERMAN, "Deutsch" },
    { SCE_SYSTEM_PARAM_LANG_ENGLISH_GB, "English (United Kingdom)" },
    { SCE_SYSTEM_PARAM_LANG_ENGLISH_US, "English (United States)" },
    { SCE_SYSTEM_PARAM_LANG_SPANISH, reinterpret_cast<const char *>(u8"Español") },
    { SCE_SYSTEM_PARAM_LANG_FRENCH, reinterpret_cast<const char *>(u8"Français") },
    { SCE_SYSTEM_PARAM_LANG_ITALIAN, "Italiano" },
    { SCE_SYSTEM_PARAM_LANG_DUTCH, "Nederlands" },
    { SCE_SYSTEM_PARAM_LANG_NORWEGIAN, "Norsk" },
    { SCE_SYSTEM_PARAM_LANG_POLISH, "Polski" },
    { SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR, reinterpret_cast<const char *>(u8"Português (Brasil)") },
    { SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT, reinterpret_cast<const char *>(u8"Português (Portugal)") },
    { SCE_SYSTEM_PARAM_LANG_RUSSIAN, reinterpret_cast<const char *>(u8"Русский") },
    { SCE_SYSTEM_PARAM_LANG_FINNISH, "Suomi" },
    { SCE_SYSTEM_PARAM_LANG_SWEDISH, "Svenska" },
    { SCE_SYSTEM_PARAM_LANG_TURKISH, reinterpret_cast<const char *>(u8"Türkçe") },
    { SCE_SYSTEM_PARAM_LANG_JAPANESE, reinterpret_cast<const char *>(u8"日本語") },
    { SCE_SYSTEM_PARAM_LANG_KOREAN, "Korean" },
    { SCE_SYSTEM_PARAM_LANG_CHINESE_S, reinterpret_cast<const char *>(u8"简体中文") },
    { SCE_SYSTEM_PARAM_LANG_CHINESE_T, reinterpret_cast<const char *>(u8"繁體中文") },
};

enum class GateAnimationState {
    None, // No gate animation is active.
    EnterApp, // Expand the gate to full screen before entering the app.
    PreRunApp, // EnterApp finished; the app is ready to be started or resumed.
    ReturnApp // Shrink the app view into its gate while opening the LiveArea.
};

struct GateAnimation {
    GateAnimationState state = GateAnimationState::None;
    static constexpr float duration = 0.32f;
    float progress = 0.f;
    std::chrono::steady_clock::time_point start_time;

    void start(GateAnimationState anim_state);
    void update();
};

struct GuiState {
    std::unique_ptr<ImGui_State> imgui_state;
    std::mutex render_mutex;
    std::function<void()> key_capture_completion;
    int controller_binding_capture = -1;

    gui::FileMenuState file_menu;
    gui::DebugMenuState debug_menu;
    gui::ConfigurationMenuState configuration_menu;
    gui::ControlMenuState controls_menu;
    gui::HelpMenuState help_menu;
    gui::VitaAreaState vita_area;
    gui::AppsSelector app_selector;

    compat::CompatState compat;
    std::atomic_bool compat_db_update_started = false;
    gui::LangState lang;

    util::InfoMessage info_message{};

    std::map<std::string, ImGui_Texture> users_avatar;
    std::map<std::string, std::map<AvatarSize, AvatarInfo>> users_avatar_infos;
    ImGui_Texture v3kn_avatar;
    ImGui_Texture v3kn_panel;

    MemoryEditor memory_editor;
    MemoryEditor gxp_shader_editor;
    size_t memory_editor_start = 0;
    size_t memory_editor_count = 0;

    std::string disassembly_arch = "THUMB";

    // Original size of disassembly_address was 9.
    // Changed to 12 due to critical GitHub CodeQL alert.
    char disassembly_address[12] = "00000000";
    char disassembly_count[5] = "100";
    std::vector<std::string> disassembly;

    bool is_capturing_keys = false;
    bool is_key_capture_dropped = false;
    int old_captured_key = 0;
    int captured_key = 0;

    ImGuiTextFilter module_search_bar;

    ImGuiTextFilter app_search_bar;

    bool is_nav_button = false;
    bool is_key_locked = false;

    std::vector<std::string> live_area_current_open_apps_list;
    int32_t live_area_app_current_open = -1;

    std::uint64_t current_theme_bg = 0;
    std::map<std::string, std::map<ThemePreviewType, ImGui_Texture>> themes_preview;
    std::vector<ImGui_Texture> theme_backgrounds;
    std::vector<ImVec4> theme_backgrounds_font_color;
    std::map<NoticeIcon, ImGui_Texture> theme_information_bar_notice;

    std::map<time_t, ImGui_Texture> notice_info_icon;

    std::uint64_t current_user_bg = 0;
    std::map<std::string, ImGui_Texture> user_backgrounds;

    struct UserBackgroundInfos {
        ImVec2 prev_pos;
        ImVec2 pos;
        ImVec2 prev_size;
        ImVec2 size;
    };
    std::map<std::string, UserBackgroundInfos> user_backgrounds_infos;

    std::map<std::string, std::map<std::string, ImGui_Texture>> trophy_np_com_id_list_icons;
    std::map<std::string, ImGui_Texture> trophy_list;

    ImGui_Texture start_background;

    std::map<std::string, ImGui_Texture> apps_background;

    InfoBarColor information_bar_color;

    GateAnimation gate_animation{};
    ImGui_Texture live_area_last_app_frame;
    std::map<std::string, std::map<std::string, ImGui_Texture>> live_area_contents;
    std::map<std::string, std::map<std::string, std::map<std::string, std::vector<ImGui_Texture>>>> live_items;

    std::vector<ImGui_Texture> manuals;

    uint64_t shaders_compiled_display_count = 0;
    uint64_t shaders_compiled_display_time = 0;

    SceUID thread_watch_index = -1;

    std::vector<NpTrophyUnlockCallbackData> trophy_unlock_display_requests;
    std::mutex trophy_unlock_display_requests_access_mutex;

    // imgui
    ImFont *monospaced_font[FontScaleCandidatesSize]{};
    ImFont *vita_font[FontScaleCandidatesSize]{};
    ImFont *large_font[FontScaleCandidatesSize]{};
    bool fw_font = false;
};
