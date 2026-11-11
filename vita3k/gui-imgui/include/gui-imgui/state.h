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
#include <gui-imgui/lang_state.h>
#include <gui-imgui/popups.h>
#include <lang/state.h>
#include <np/state.h>
#include <sdl/sdl_window.h>
#include <util/fs.h>
#include <util/log.h>
#include <vita_shell/state.h>

#include <array>

#include <imgui.h>
#include <imgui_memory_editor.h>

#include <SDL3/SDL_video.h>
#include <gui-imgui/imgui_impl_sdl_state.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

struct GuiState;
namespace pfsmgr {
class Mounts;
}
struct EmuEnvState;

namespace renderer {
class FrameHost;
}

namespace gui {

inline constexpr std::array<float, 3> FontScaleCandidates = { 1.f, 1.5f, 2.f };
inline constexpr int FontScaleCandidatesSize = static_cast<int>(FontScaleCandidates.size());

struct WindowState {
    explicit WindowState(EmuEnvState &emuenv);
    ~WindowState();

    EmuEnvState &emuenv;
    std::unique_ptr<renderer::FrameHost> frame_host;
    bool renderer_initialized = false;
    SDL_GLContext gl_context = nullptr;
};

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
    uint32_t app_close_button = 0;
    bool app_information = false;
    bool content_manager = false;
    bool connecting_please_wait = false;
    bool friends = false;
    bool friend_profile = false;
    bool friend_activity = false;
    bool home_screen = false;
    bool information_bar = false;
    bool pkg_install = false;
    bool messages = false;
    bool manual = false;
    bool online_storage = false;
    bool please_wait = false;
    bool settings = false;
    bool start_screen = false;
    bool trophy_collection = false;
    bool update_history_info = false;
    uint32_t update_history_button = 0;
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

struct Notification {
    std::string app_id;
    std::string title;
    std::string message;
    double start_time = 0.0;
    bool active = false;
};

struct AppUpdateInfo {
    std::string titleid;
    std::string version;
    uint64_t size = 0;
    std::string url;
    std::string content_id;
    std::string title;
};

enum class UpdateState {
    NONE,
    DOWNLOADING,
    WAITING_INSTALL,
    INSTALLING,
    SUCCESS,
    FAILED
};

struct UpdateInstall {
    std::string content_id;
    fs::path pkg_path;
    UpdateState state = UpdateState::NONE;
};

enum NoticeIcon {
    NO,
    NEW
};

enum class PageIndicator {
    BASE,
    CUR,
};

enum ThemePreviewType {
    HOME,
    LOCK,
};

namespace gui {
struct ThemePreviewRequest {
    std::string content_id;
    std::string path;
};

struct ThemePreviewLoader {
    struct Result {
        std::string content_id;
        IconData image;
    };
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<Result> ready;
    std::thread thread;
    std::atomic_bool quit = false;

    ThemePreviewLoader(const fs::path &vita_fs_path, IOState &io, std::vector<ThemePreviewRequest> requests);
    ~ThemePreviewLoader();
    void commit(GuiState &gui);
};
} // namespace gui

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
    std::optional<AppLaunchRequest> launch_request;

    void start(GateAnimationState anim_state);
    void update();
};

struct GuiState {
    // Destroy the window and renderer only after the GUI resources that use them.
    std::unique_ptr<gui::WindowState> window_state;
    int max_font_level = 0;
    int current_font_level = 0;

    std::unique_ptr<ImGui_State> imgui_state;
    std::atomic<bool> imgui_render{ true };
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

    gui::InfoMessage info_message{};

    std::map<std::string, ImGui_Texture> users_avatar;
    std::map<std::string, std::map<AvatarSize, AvatarInfo>> users_avatar_infos;
    std::map<std::string, ImGui_Texture> friends_panel;
    std::map<std::string, ImGui_Texture> friends_avatar;
    std::unordered_map<std::string, ImU32> friends_panel_bg_color;
    std::optional<ImU32> self_panel_bg_color;
    ImGui_Texture v3kn_avatar;
    ImGui_Texture v3kn_panel;
    ImGui_Texture friend_profile_panel;
    std::string friend_profile_panel_online_id;
    std::optional<ImU32> friend_profile_panel_bg_color;
    ImGui_Texture friend_profile_avatar;
    std::string friend_profile_avatar_online_id;

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
    std::string live_area_app_closing;
    std::string pending_app_deletion;

    float bg_transition_alpha = 1.f;
    std::uint64_t current_theme_bg = 0;
    std::map<std::string, ImGui_Texture> themes_package;
    std::map<ThemePreviewType, ImGui_Texture> theme_preview;
    std::string theme_preview_id;
    std::optional<gui::ThemePreviewLoader> theme_preview_loader;
    std::optional<std::future<std::optional<vita_shell::ThemeCatalog>>> theme_catalog_loader;
    std::string theme_catalog_error;
    std::map<std::string, ImGui_Texture> vita_icons;
    std::vector<ImGui_Texture> theme_backgrounds;
    std::vector<ImVec4> theme_backgrounds_font_color;
    std::map<NoticeIcon, ImGui_Texture> theme_information_bar_notice;
    std::map<PageIndicator, ImGui_Texture> theme_page_indicator;

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

    std::map<std::string, AppUpdateInfo> new_update_infos;
    std::map<std::string, bool> app_has_update;
    std::map<std::string, UpdateInstall> updates_install;
    std::mutex update_state_mutex;
    std::jthread update_check_thread;
    std::mutex notifications_mutex;
    std::vector<Notification> notifications;

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
    ImFont *monospaced_font[gui::FontScaleCandidatesSize]{};
    ImFont *vita_font[gui::FontScaleCandidatesSize]{};
    ImFont *large_font[gui::FontScaleCandidatesSize]{};
    bool fw_font = false;

    std::atomic_bool apps_list_scan_running = false;
    std::jthread apps_list_scan_thread;
    std::jthread compat_db_update_thread;
};
