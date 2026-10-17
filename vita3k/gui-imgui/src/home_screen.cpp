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

#include "private.h"

#include <app/functions.h>
#include <app/session_controller.h>
#include <bgm_player/functions.h>
#include <config/config.h>
#include <config/state.h>
#include <ctrl/ctrl.h>
#include <dialog/state.h>
#include <display/state.h>
#include <gui-imgui/functions.h>
#include <io/state.h>
#include <kernel/state.h>
#include <v3kn/friend.h>
#include <v3kn/messages.h>
#include <v3kn/state.h>

#include <limits>

#include <util/log.h>
#include <util/safe_time.h>
#include <util/string_utils.h>
#include <util/vector_utils.h>

#include <imgui_internal.h>

namespace gui {

void init_user_apps(GuiState &gui, EmuEnvState &emuenv) {
    if (gui.apps_list_scan_running.exchange(true)) {
        LOG_DEBUG("Application list refresh is already running.");
        return;
    }

    gui.apps_background.clear();
    gui.app_selector.user_apps_icon.clear();
    gui.live_area_app_current_open = -1;
    gui.live_area_current_open_apps_list.clear();
    gui.live_area_app_closing.clear();
    gui.live_area_contents.clear();
    gui.live_items.clear();
    if (gui.app_selector.icon_async_loader)
        gui.app_selector.icon_async_loader->quit = true;

    gui.app_selector.is_app_list_sorted = false;
    load_and_update_compat_user_apps(gui, emuenv);

    gui.apps_list_scan_thread = std::jthread([&gui, &emuenv]() {
        auto apps_list_size = app::get_apps(emuenv).size();
        if (!app::scan_apps(emuenv)) {
            LOG_ERROR("Failed to refresh the application list.");
            gui.apps_list_scan_running = false;
            return;
        }

        auto apps = app::get_apps(emuenv);
        if (apps.empty()) {
            gui.apps_list_scan_running = false;
            return;
        }

        const auto new_apps_list_size = apps.size();
        if (apps_list_size != new_apps_list_size) {
            std::string change_app_list = "new application(s) added";
            if (apps_list_size > new_apps_list_size) {
                change_app_list = "application(s) removed";
                apps_list_size -= new_apps_list_size;
            } else
                apps_list_size = new_apps_list_size - apps_list_size;

            LOG_INFO("{} {}", apps_list_size, change_app_list);
        }

        {
            std::lock_guard<std::mutex> lock(gui.render_mutex);
            init_apps_icon(gui, emuenv, apps);
            gui.app_selector.is_app_list_sorted = false;
        }
        // A refresh can join this worker while holding render_mutex once it is idle.
        gui.apps_list_scan_running = false;
    });
}

std::vector<std::string>::iterator get_live_area_current_open_apps_list_index(GuiState &gui, const std::string &app_path) {
    return std::find(gui.live_area_current_open_apps_list.begin(), gui.live_area_current_open_apps_list.end(), app_path);
}

void update_live_area_current_open_apps_list(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    if ((get_live_area_current_open_apps_list_index(gui, app_path) != gui.live_area_current_open_apps_list.end()) && (gui.live_area_current_open_apps_list.front() != app_path))
        gui.live_area_current_open_apps_list.erase(get_live_area_current_open_apps_list_index(gui, app_path));
    if (get_live_area_current_open_apps_list_index(gui, app_path) == gui.live_area_current_open_apps_list.end())
        gui.live_area_current_open_apps_list.insert(gui.live_area_current_open_apps_list.begin(), app_path);
    gui.live_area_app_current_open = 0;
    if (gui.live_area_current_open_apps_list.size() > 6) {
        const auto &last_app = gui.live_area_current_open_apps_list.back() == app_ui_path(emuenv.io.app_path) ? gui.live_area_current_open_apps_list[gui.live_area_current_open_apps_list.size() - 2] : gui.live_area_current_open_apps_list.back();
        gui.live_area_contents.erase(last_app);
        gui.live_items.erase(last_app);
        gui.live_area_current_open_apps_list.erase(get_live_area_current_open_apps_list_index(gui, last_app));
    }
}

static std::map<std::string, uint64_t> last_time;
static std::string first_visible_app, current_selected_app;
static std::string pending_app_open_path;
static bool pending_app_open_requested = false;
static bool pending_app_open_live_area = false;
static bool pending_app_open_wait_frame_displayed = false;
static bool pending_app_open_controls_please_wait = false;

static void request_app_open(GuiState &gui, const bool live_area, const std::string &app_path) {
    if (app_path.empty())
        return;

    pending_app_open_path = app_path;
    current_selected_app.clear();
    pending_app_open_live_area = live_area;
    pending_app_open_requested = true;
    pending_app_open_wait_frame_displayed = false;
    pending_app_open_controls_please_wait = true;
    gui.vita_area.please_wait = true;
}

void open_live_area(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    const std::string opened_app_path = app_path;
    current_selected_app = opened_app_path;
    if (!gui.live_area_app_closing.empty()) {
        const auto closing_app = get_live_area_current_open_apps_list_index(gui, gui.live_area_app_closing);
        if (closing_app != gui.live_area_current_open_apps_list.end())
            gui.live_area_current_open_apps_list.erase(closing_app);
    }
    gui.live_area_app_closing.clear();
    update_live_area_current_open_apps_list(gui, emuenv, opened_app_path);
    last_time["home"] = 0;
    init_live_area(gui, emuenv, opened_app_path);
    gui.vita_area.home_screen = true;
    gui.vita_area.information_bar = true;
}

void pre_load_app(GuiState &gui, EmuEnvState &emuenv, bool live_area, const std::string &path, app::AppSessionController *session) {
    const auto legacy = path.find(':') == std::string::npos ? get_app_index(gui, emuenv, path) : std::nullopt;
    const auto app_path = legacy ? legacy->path : path;
    if (app_path == "emu:app/NPXS10003") {
        app::update_last_time_app_used(emuenv, app_path);
        gui.app_selector.is_app_list_sorted = false;
        open_path("https://Vita3k.org");
    } else {
        if (live_area)
            open_live_area(gui, emuenv, app_path);
        else
            pre_run_app(gui, emuenv, app_path, session);
    }
}

void pre_run_app(GuiState &gui, EmuEnvState &emuenv, const std::string &path, app::AppSessionController *session) {
    const auto legacy = path.find(':') == std::string::npos ? get_app_index(gui, emuenv, path) : std::nullopt;
    const auto app_path = legacy ? legacy->path : path;
    const auto is_sys = is_emu_app(app_path) && app_path != "emu:vsh/shell";
    const auto launch_path = app_launch_path(app_path);
    if (!is_sys) {
        if (emuenv.io.app_path != launch_path) {
            if (!emuenv.io.app_path.empty())
                gui.vita_area.app_close = true;
            else {
                gui.vita_area.home_screen = false;
                gui.vita_area.information_bar = false;
                auto request = gui.gate_animation.launch_request
                    ? std::move(*gui.gate_animation.launch_request)
                    : AppLaunchRequest{
                          .app_path = launch_path,
                          .self_path = app_path == "emu:vsh/shell" ? "shell.self" : "",
                          .reason = AppLaunchReason::User,
                      };
                gui.gate_animation.launch_request.reset();
                app::request_in_process_launch(emuenv, std::move(request));
            }
        } else {
            gui.vita_area.home_screen = false;
            gui.vita_area.information_bar = false;
            gui.imgui_render = false;
#ifdef __ANDROID__
            set_controller_overlay_state(get_overlay_display_mask(emuenv.cfg));
#endif
            if (session && session->is_running()) {
                session->set_pause_reason(app::AppSessionPauseReason::Menu, false);
                if (!session->is_paused())
                    bgm_player::switch_bgm_state(true);
            }
        }
    } else {
        bgm_player::switch_bgm_state(true);
        emuenv.app_path = app_path;
        gui.vita_area.home_screen = false;
        init_app_background(gui, emuenv, fs::path(app_path).filename().string());
        app::update_last_time_app_used(emuenv, app_path);
        gui.app_selector.is_app_list_sorted = false;

        if (app_path == "emu:app/NPXS10006") {
            open_friend(gui, emuenv);
        } else if (app_path == "emu:app/NPXS10008") {
            init_trophy_collection(gui, emuenv);
            gui.vita_area.trophy_collection = true;
        } else if (app_path == "emu:app/NPXS10014") {
            if (v3kn::is_v3kn_logged_in())
                v3kn::load_friends_list(emuenv, "friends", true);
            v3kn::load_conversations_list(emuenv);
            gui.vita_area.messages = true;
        } else if (app_path == "emu:app/NPXS10015") {
            if (gui.vita_area.content_manager) {
                emuenv.app_path = "emu:app/NPXS10026";
                app::update_app_time_used(emuenv, "emu:app/NPXS10026");
                gui.vita_area.content_manager = false;
            }

            gui.vita_area.settings = true;
        } else {
            init_content_manager(gui, emuenv);
            gui.vita_area.content_manager = true;
        }
    }
}

void close_system_app(GuiState &gui, EmuEnvState &emuenv) {
    if (gui.vita_area.content_manager) {
        gui.vita_area.content_manager = false;
        app::update_app_time_used(emuenv, "emu:app/NPXS10026");
    } else if (gui.vita_area.manual) {
        gui.vita_area.manual = false;

        // Free manual textures from memory when manual is closed
        for (auto &manual : gui.manuals)
            manual = {};
        gui.manuals.clear();
    } else if (gui.vita_area.settings) {
        gui.vita_area.settings = false;
        gui.theme_preview.clear();
        gui.theme_preview_id.clear();
        app::update_app_time_used(emuenv, "emu:app/NPXS10015");
        if (emuenv.app_path == "emu:app/NPXS10026") {
            pre_run_app(gui, emuenv, "emu:app/NPXS10026");
            return;
        }
    } else if (gui.vita_area.friends) {
        gui.vita_area.friends = false;
        gui.vita_area.friend_profile = false;
        gui.vita_area.friend_activity = false;
        app::update_app_time_used(emuenv, "emu:app/NPXS10006");
    } else if (gui.vita_area.messages) {
        gui.vita_area.messages = false;
        app::update_app_time_used(emuenv, "emu:app/NPXS10014");
    } else {
        gui.vita_area.trophy_collection = false;
        app::update_app_time_used(emuenv, "emu:app/NPXS10008");
    }

    if (gui.live_area_app_current_open >= 0
        && gui.live_area_app_current_open < static_cast<int32_t>(gui.live_area_current_open_apps_list.size())
        && gui::get_live_area_current_open_apps_list_index(gui, gui.live_area_current_open_apps_list[gui.live_area_app_current_open]) != gui.live_area_current_open_apps_list.end()) {
        gui.vita_area.home_screen = true;
        gui.vita_area.information_bar = true;
    } else {
        if (emuenv.cfg.show_info_bar)
            gui.vita_area.information_bar = true;
        gui.vita_area.home_screen = true;
    }

    bgm_player::switch_bgm_state(false);
}

void close_and_run_new_app(EmuEnvState &emuenv, const std::string &app_path) {
    bgm_player::switch_bgm_state(true);
    app::request_in_process_launch(emuenv, AppLaunchRequest{
                                               .app_path = app_launch_path(app_path),
                                               .self_path = app_path == "emu:vsh/shell" ? "shell.self" : "",
                                               .reason = AppLaunchReason::User,
                                           });
}

inline static uint64_t current_time() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

enum AppRegion {
    ALL,
    USA,
    EURO,
    JAPAN,
    ASIA,
    COMMERCIAL,
    HOMEBREW,
};

constexpr compat::CompatibilityState ALL_COMPAT_STATE = static_cast<compat::CompatibilityState>(7);

static AppRegion app_region = ALL;
static compat::CompatibilityState app_compat_state = ALL_COMPAT_STATE;

static bool app_filter(const std::string &app) {
    const auto filter_app = [&](const std::vector<std::string> &app_region) {
        const auto app_index = std::ranges::find_if(app_region, [&](const std::string &a) {
            return app.find(a) != std::string::npos;
        });
        return app_index == app_region.end();
    };

    switch (app_region) {
    case ALL:
        break;
    case USA:
        if (filter_app({ "PCSA", "PCSE" }))
            return true;
        break;
    case EURO:
        if (filter_app({ "PCSF", "PCSB" }))
            return true;
        break;
    case JAPAN:
        if (filter_app({ "PCSC", "PCSG" }))
            return true;
        break;
    case ASIA:
        if (filter_app({ "PCSD", "PCSH" }))
            return true;
        break;
    case COMMERCIAL:
        if (filter_app({ "PCS" }))
            return true;
        break;
    case HOMEBREW:
        if (!filter_app({ "PCS", "NPXS" }))
            return true;
        break;
    }

    return false;
}

static void sort_app_list(GuiState &gui, EmuEnvState &emuenv, const SortType &type) {
    auto &sorted = gui.app_selector.app_list_sorted[type];
    if (gui.app_selector.is_app_list_sorted) {
        for (auto &state : gui.app_selector.app_list_sorted)
            state.second = (state.first == type) ? static_cast<gui::SortState>(std::max<int>(1, (state.second + 1) % 3)) : NOT_SORTED;

        emuenv.app.user_list.users[emuenv.io.user_id].sort_apps_type = type;
        emuenv.app.user_list.users[emuenv.io.user_id].sort_apps_state = sorted;

        app::save_user(emuenv, emuenv.io.user_id);
    } else {
        sorted = emuenv.app.user_list.users[emuenv.io.user_id].sort_apps_state;
        gui.app_selector.is_app_list_sorted = true;
    }

    const auto app_times = type == LAST_TIME ? app::get_user_app_times(emuenv) : std::map<std::string, app::AppTime>{};
    const auto split_version = [](const std::string &version) {
        uint32_t major = 0;
        uint32_t minor = 0;
        const auto pos = version.find('.');
        if (pos != std::string::npos) {
            major = static_cast<uint32_t>(std::stoul(version.substr(0, pos)));
            minor = static_cast<uint32_t>(std::stoul(version.substr(pos + 1)));
        }
        return std::make_pair(major, minor);
    };

    app::sort_apps(emuenv, [&](const App &lhs, const App &rhs) {
        const auto lhs_key = app_launch_path(lhs.path);
        const auto rhs_key = app_launch_path(rhs.path);
        const auto lhs_time = app_times.contains(lhs_key) ? app_times.at(lhs_key).last_time_used : 0;
        const auto rhs_time = app_times.contains(rhs_key) ? app_times.at(rhs_key).last_time_used : 0;
        const auto lhs_compat = gui.compat.app_compat_db.contains(lhs.title_id) ? gui.compat.app_compat_db.at(lhs.title_id).state : compat::UNKNOWN;
        const auto rhs_compat = gui.compat.app_compat_db.contains(rhs.title_id) ? gui.compat.app_compat_db.at(rhs.title_id).state : compat::UNKNOWN;
        switch (type) {
        case APP_VER: {
            const auto lhs_version = split_version(lhs.app_ver);
            const auto rhs_version = split_version(rhs.app_ver);
            switch (sorted) {
            case ASCENDANT:
                return lhs_version < rhs_version;
            case DESCENDANT:
                return lhs_version > rhs_version;
            }
            break;
        }
        case CATEGORY:
            switch (sorted) {
            case ASCENDANT:
                return lhs.category < rhs.category;
            case DESCENDANT:
                return lhs.category > rhs.category;
            }
            break;
        case COMPAT:
            switch (sorted) {
            case ASCENDANT:
                return lhs_compat < rhs_compat;
            case DESCENDANT:
                return lhs_compat > rhs_compat;
            }
            break;
        case LAST_TIME:
            switch (sorted) {
            case ASCENDANT:
                return lhs_time > rhs_time;
            case DESCENDANT:
                return lhs_time < rhs_time;
            }
            break;
        case TITLE:
            switch (sorted) {
            case ASCENDANT:
                return string_utils::toupper(lhs.title) < string_utils::toupper(rhs.title);
            case DESCENDANT:
                return string_utils::toupper(lhs.title) > string_utils::toupper(rhs.title);
            }
            break;
        case TITLE_ID:
            switch (sorted) {
            case ASCENDANT:
                return lhs.title_id < rhs.title_id;
            case DESCENDANT:
                return lhs.title_id > rhs.title_id;
            }
            break;
        }
        return false;
    });
}

static std::string get_label_name(GuiState &gui, const SortType &type) {
    std::string label;
    auto &lang = gui.lang.home_screen;
    auto &info = gui.lang.app_context.info;
    switch (type) {
    case APP_VER: label = lang["ver"]; break;
    case CATEGORY: label = lang["cat"]; break;
    case COMPAT: label = lang["comp"]; break;
    case LAST_TIME: label = lang["last_time"]; break;
    case TITLE: label = info["name"]; break;
    case TITLE_ID: label = info["title_id"]; break;
    }

    switch (gui.app_selector.app_list_sorted[type]) {
    case ASCENDANT: label += " >"; break;
    case DESCENDANT: label += " <"; break;
    default: break;
    }

    return label;
}

constexpr uint32_t LINE_PATTERN[] = { 3, 4, 3 }; // line 0 -> 3 cols, line 1 -> 4 cols, line 2 -> 3 cols
const uint32_t line_pattern_size = sizeof(LINE_PATTERN) / sizeof(LINE_PATTERN[0]);

struct AppWithPosition {
    std::string app_path; // App path
    uint32_t line; // Line (1-3)
    uint32_t column; // Column (row)
};
std::vector<AppWithPosition> apps_list_filtered;
void browse_home_apps_list(GuiState &gui, EmuEnvState &emuenv, const uint32_t button, app::AppSessionController *session) {
    if (apps_list_filtered.empty() || pending_app_open_requested || !gui.live_area_app_closing.empty())
        return;

    // When user press a button, enable navigation by buttons
    if (!gui.is_nav_button) {
        gui.is_nav_button = true;

        // When the current selected app index have not any selected, set it to the first visible app index
        if (current_selected_app.empty())
            current_selected_app = first_visible_app;

        return;
    }

    const auto apps_list_filtered_size = static_cast<int32_t>(apps_list_filtered.size() - 1);

    // Find current selected app index in apps list filtered
    const auto current_selected_app_it = std::find_if(apps_list_filtered.begin(), apps_list_filtered.end(), [&](const AppWithPosition &app) {
        return app.app_path == current_selected_app;
    });
    const int32_t current_selected_app_index = (current_selected_app_it != apps_list_filtered.end()) ? std::distance(apps_list_filtered.begin(), current_selected_app_it) : -1;
    if (current_selected_app_index == -1) {
        current_selected_app = first_visible_app;
        return;
    }

    const auto prev_filtered_app = apps_list_filtered[std::max(current_selected_app_index - 1, 0)];
    const auto next_filtered_app = apps_list_filtered[std::min(current_selected_app_index + 1, apps_list_filtered_size)];

    const auto switch_to_first_live_area_open_app = [&gui]() {
        if (!gui.live_area_current_open_apps_list.empty())
            ++gui.live_area_app_current_open;
    };

    const auto app_info = apps_list_filtered[current_selected_app_index];

    switch (button) {
    case SCE_CTRL_UP:
        if (emuenv.cfg.apps_list_grid) {
            if (current_selected_app_index >= 3)
                current_selected_app = apps_list_filtered[current_selected_app_index - ((app_info.line == 0) || ((app_info.line == 1) && (app_info.column != 3)) ? 3 : 4)].app_path;
        } else
            current_selected_app = prev_filtered_app.app_path;
        break;
    case SCE_CTRL_RIGHT:
        if (emuenv.cfg.apps_list_grid) {
            if ((app_info.line == 1) && (app_info.column == 3))
                switch_to_first_live_area_open_app();
            else if ((app_info.line != 1) && (app_info.column == 2))
                current_selected_app = apps_list_filtered[current_selected_app_index + (app_info.line == 0 ? 4 : -3)].app_path;
            else
                current_selected_app = next_filtered_app.app_path;
        } else
            switch_to_first_live_area_open_app();
        break;
    case SCE_CTRL_DOWN:
        if (emuenv.cfg.apps_list_grid) {
            const auto next_index = app_info.line == 1 && app_info.column != 3 ? 4 : 3;
            const auto target_index = current_selected_app_index + next_index;
            if (target_index <= apps_list_filtered_size)
                current_selected_app = apps_list_filtered[target_index].app_path;
            else if (target_index != apps_list_filtered_size)
                current_selected_app = apps_list_filtered[apps_list_filtered_size].app_path;
        } else
            current_selected_app = next_filtered_app.app_path;
        break;
    case SCE_CTRL_LEFT:
        if (emuenv.cfg.apps_list_grid) {
            if (current_selected_app_index > 0 && app_info.column != 0)
                current_selected_app = prev_filtered_app.app_path;
            else if (app_info.line != 1)
                current_selected_app = apps_list_filtered[current_selected_app_index + (app_info.line == 0 ? 3 : -4)].app_path;
        }
        break;
    case SCE_CTRL_R1:
        gui.live_area_app_current_open = std::min(gui.live_area_app_current_open + 1, static_cast<int32_t>(gui.live_area_current_open_apps_list.size() - 1));
        break;
    case SCE_CTRL_CIRCLE:
        if (emuenv.cfg.sys_button == 0)
            request_app_open(gui, emuenv.cfg.show_live_area_screen, current_selected_app);
        break;
    case SCE_CTRL_CROSS:
        if (emuenv.cfg.sys_button == 1)
            request_app_open(gui, emuenv.cfg.show_live_area_screen, current_selected_app);
        break;
    default: break;
    }
}

void select_app(GuiState &gui, EmuEnvState &emuenv, const std::string &title_id) {
    const auto apps = app::get_apps(emuenv);
    const auto app_it = std::find_if(apps.begin(), apps.end(), [&](const App &app) {
        return app.title_id == title_id;
    });

    if (app_it != apps.end()) {
        current_selected_app = app_it->path;
        gui.is_nav_button = true;
    } else
        LOG_ERROR("App with title id {} not found", title_id);
}

void draw_home_screen(GuiState &gui, EmuEnvState &emuenv, app::AppSessionController *session) {
    constexpr ImU32 ARROW_COLOR = 0xFFFFFFFF; // White

    const auto &viewport = sdl::get_viewport();
    const ImVec2 VIEWPORT_POS(viewport.logical_x, viewport.logical_y);
    const ImVec2 VIEWPORT_SIZE(viewport.logical_width, viewport.logical_height);
    const ImVec2 VIEWPORT_RES_SCALE(viewport.scale_x, viewport.scale_y);
    const ImVec2 VIEWPORT_SCALE(VIEWPORT_RES_SCALE.x * viewport.manual_dpi_scale, VIEWPORT_RES_SCALE.y * viewport.manual_dpi_scale);
    const auto INFORMATION_BAR_HEIGHT = 32.f * VIEWPORT_SCALE.y;

    // Clear apps list filtered
    apps_list_filtered.clear();

    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INFORMATION_BAR_HEIGHT);
    ImGui::SetNextWindowPos(ImVec2(VIEWPORT_POS.x, VIEWPORT_POS.y + INFORMATION_BAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::SetNextWindowBgAlpha(emuenv.cfg.background_alpha);
    ImGui::SetNextWindowContentSize(ImVec2((gui.live_area_current_open_apps_list.size() + 1) * WINDOW_SIZE.x, WINDOW_SIZE.y));
    auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;
    if (gui.is_nav_button || pending_app_open_requested || !gui.live_area_app_closing.empty())
        flags |= ImGuiWindowFlags_NoMouseInputs;
    ImGui::Begin("##home_screen", &gui.vita_area.home_screen, flags);
    draw_background(gui, emuenv);

    ImGui::PopStyleVar(2);
    if (!gui.imgui_render || ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
        gui.vita_area.information_bar = true;

    const auto config_dialog = gui.configuration_menu.custom_settings_dialog || gui.configuration_menu.settings_dialog || gui.controls_menu.controls_dialog;
    const auto install_dialog = gui.file_menu.archive_install_dialog || gui.file_menu.firmware_install_dialog || gui.file_menu.pkg_install_dialog;
    if (!config_dialog && !install_dialog && !gui.vita_area.app_close && !gui.vita_area.app_information && !gui.help_menu.about_dialog && !gui.help_menu.welcome_dialog && !gui.is_nav_button && !pending_app_open_requested) {
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || ImGui::IsAnyItemActive() || ImGui::IsAnyItemHovered())
            last_time["start"] = 0;
        else {
            if (last_time["start"] == 0)
                last_time["start"] = current_time();

            while (last_time["start"] + emuenv.cfg.delay_start < current_time()) {
                last_time["start"] += emuenv.cfg.delay_start;
                last_time["home"] = 0;
                gui.vita_area.home_screen = false;
                gui.vita_area.information_bar = true;
                gui.vita_area.start_screen = true;
                v3kn::set_friend_presence_status(emuenv.v3kn.friends_state, PresenceStatus::NotAvailable);
                bgm_player::switch_bgm_state(true);
            }
        }
    }

    static float scroll_x = 0.f;
    static float target_scroll_x = 0.f;
    static uint32_t live_area_scroll_animation_nonce = 0;
    static uint32_t page_scroll_animation_nonce = 0;

    const auto SCREEN_POS = ImGui::GetCursorScreenPos();
    const ImVec2 POS_HOME_SCREEN(SCREEN_POS.x, SCREEN_POS.y);
    ImGui::SetNextWindowBgAlpha(emuenv.cfg.background_alpha);
    ImGui::SetNextWindowPos(POS_HOME_SCREEN, ImGuiCond_Always);
    ImGui::BeginChild("##home_screen", WINDOW_SIZE, ImGuiChildFlags_None, flags);
    static bool is_live_area_animating = false; // Indicates if the animation is ongoing
    const std::function<void(float)> set_current_scroll_y = [](float value) {
        ImGui::SetScrollY(value);
    };
    const std::function<void(float)> set_current_scroll_x = [](float value) {
        ImGui::SetScrollX(value);
    };

    static uint32_t current_page_index = 0; // Current page index
    static bool is_page_animating = false; // Indicates if the animation is ongoing
    static bool is_bg_animating = false; // Indicates if the background is animating
    constexpr float bg_fade_out_speed = 0.2f; // For the disappearance of the old background
    constexpr float bg_fade_in_speed = 0.1f; // For the apparition of the new background
    static uint32_t next_bg_index = 0; // Next background index
    static bool is_fading_out = false; // Indicates if the background is fading out

    const auto set_new_background = [&]() {
        if (emuenv.app.user_list.users[emuenv.io.user_id].use_theme_bg)
            gui.current_theme_bg = next_bg_index;
        else
            gui.current_user_bg = next_bg_index;
    };

    const auto change_bg = [&](const int32_t direction) {
        if (is_bg_animating)
            set_new_background();
        if (emuenv.app.user_list.users[emuenv.io.user_id].use_theme_bg) {
            const auto size = static_cast<int64_t>(gui.theme_backgrounds.size());
            if (size == 0)
                return;
            next_bg_index = static_cast<uint32_t>((static_cast<int64_t>(gui.current_theme_bg) + direction % size + size) % size);
        } else {
            const auto size = static_cast<int64_t>(gui.user_backgrounds.size());
            if (size == 0)
                return;
            next_bg_index = static_cast<uint32_t>((static_cast<int64_t>(gui.current_user_bg) + direction % size + size) % size);
        }
        is_bg_animating = true;
        is_fading_out = true;
    };

    auto fade_towards = [&](float &value, float target, float speed, float threshold = 0.01f) {
        value = std::lerp(value, target, speed);
        if (fabs(value - target) < threshold)
            value = target;
        return value == target;
    };

    const auto is_grid_view = emuenv.cfg.apps_list_grid;

    if (!gui.vita_area.start_screen && (!gui.theme_backgrounds.empty() || !gui.user_backgrounds.empty())) {
        if (!is_grid_view) {
            if (last_time["home"] == 0)
                last_time["home"] = current_time();

            while (last_time["home"] + emuenv.cfg.delay_background < current_time()) {
                last_time["home"] += emuenv.cfg.delay_background;
                change_bg(1);
            }
        }

        if (is_bg_animating) {
            if (is_fading_out) {
                if (fade_towards(gui.bg_transition_alpha, 0.0f, bg_fade_out_speed)) {
                    is_fading_out = false;
                    set_new_background();
                }
            } else {
                if (fade_towards(gui.bg_transition_alpha, 1.0f, bg_fade_in_speed))
                    is_bg_animating = false;
            }
        }
    }

    // Size of the icon depending view mode
    const ImVec2 ICON_SIZE(is_grid_view ? ImVec2(128.f * VIEWPORT_SCALE.x, 128.f * VIEWPORT_SCALE.y) : ImVec2(emuenv.cfg.icon_size * VIEWPORT_SCALE.x, emuenv.cfg.icon_size * VIEWPORT_SCALE.x));

    // Size of column padding
    const float column_padding_size = 20.f * VIEWPORT_SCALE.x;

    // Size of the icon part
    const float column_icon_size = ICON_SIZE.x + column_padding_size + (5.f * VIEWPORT_SCALE.x);

    // Size of the compatibility part
    const auto compat_radius = 12.f * VIEWPORT_SCALE.x;
    const auto full_compat_radius = (3.f * VIEWPORT_SCALE.x) + compat_radius;

    auto &lang = gui.lang.home_screen;

    ImGui::SetWindowFontScale(0.9f * VIEWPORT_RES_SCALE.y);

    // Sort Apps list when is not sorted
    if (!gui.app_selector.is_app_list_sorted)
        sort_app_list(gui, emuenv, emuenv.app.user_list.users[emuenv.io.user_id].sort_apps_type);

    const auto title_id_label = get_label_name(gui, TITLE_ID);
    const float title_id_size = (ImGui::CalcTextSize("PCSX12345").x + (40.f * VIEWPORT_SCALE.x));
    const auto app_ver_label = get_label_name(gui, APP_VER);
    const float app_ver_size = (ImGui::CalcTextSize(app_ver_label.c_str()).x) + (38.f * VIEWPORT_SCALE.x);
    const auto category_label = get_label_name(gui, CATEGORY);
    const float category_size = (ImGui::CalcTextSize(category_label.c_str()).x) + (38.f * VIEWPORT_SCALE.x);
    const auto compat_label = get_label_name(gui, COMPAT);
    const float compat_size = ImGui::CalcTextSize(compat_label.c_str()).x + (column_padding_size / 2.f) + (8 * VIEWPORT_SCALE.x);
    const auto title_label = get_label_name(gui, TITLE);
    const auto last_time_label = get_label_name(gui, LAST_TIME);
    const float last_time_size = (ImGui::CalcTextSize(last_time_label.c_str()).x) + (38.f * VIEWPORT_SCALE.x);
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
    ImGui::SetCursorPosY(ImGui::GetStyle().ItemSpacing.y);
    const ImVec2 BUTTON_HEIGHT(0.f, ImGui::GetFontSize() + (ImGui::GetStyle().FramePadding.y * 2.f));
    if (!is_grid_view) {
        ImGui::Columns(7);
        ImGui::SetColumnWidth(0, column_icon_size);
        if (ImGui::Button(lang["filter"].c_str(), BUTTON_HEIGHT))
            ImGui::OpenPopup("app_filter");
        ImGui::NextColumn();
        ImGui::SetColumnWidth(1, compat_size);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - (column_padding_size / 4.f));
        if (ImGui::Button(compat_label.c_str(), BUTTON_HEIGHT))
            sort_app_list(gui, emuenv, COMPAT);
        ImGui::NextColumn();
        ImGui::SetColumnWidth(2, title_id_size);
        if (ImGui::Button(title_id_label.c_str(), BUTTON_HEIGHT))
            sort_app_list(gui, emuenv, TITLE_ID);
        ImGui::NextColumn();
        ImGui::SetColumnWidth(3, app_ver_size);
        if (ImGui::Button(app_ver_label.c_str(), BUTTON_HEIGHT))
            sort_app_list(gui, emuenv, APP_VER);
        ImGui::NextColumn();
        ImGui::SetColumnWidth(4, category_size);
        if (ImGui::Button(category_label.c_str()))
            sort_app_list(gui, emuenv, CATEGORY);
        ImGui::NextColumn();
        ImGui::SetColumnWidth(5, last_time_size);
        if (ImGui::Button(last_time_label.c_str()))
            sort_app_list(gui, emuenv, LAST_TIME);
        ImGui::NextColumn();
        if (ImGui::Button(title_label.c_str()))
            sort_app_list(gui, emuenv, TITLE);
    } else {
        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, 90 * VIEWPORT_SCALE.x);
        if (ImGui::Button(lang["filter"].c_str(), BUTTON_HEIGHT))
            ImGui::OpenPopup("app_filter");
        ImGui::NextColumn();
        if (ImGui::Button(lang["sort_app"].c_str(), BUTTON_HEIGHT))
            ImGui::OpenPopup("sort_apps");
        if (ImGui::BeginPopup("sort_apps")) {
            ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
            if (ImGui::MenuItem(app_ver_label.c_str(), nullptr, gui.app_selector.app_list_sorted[APP_VER] != NOT_SORTED))
                sort_app_list(gui, emuenv, APP_VER);
            if (ImGui::MenuItem(category_label.c_str(), nullptr, gui.app_selector.app_list_sorted[CATEGORY] != NOT_SORTED))
                sort_app_list(gui, emuenv, CATEGORY);
            if (ImGui::MenuItem(compat_label.c_str(), nullptr, gui.app_selector.app_list_sorted[COMPAT] != NOT_SORTED))
                sort_app_list(gui, emuenv, COMPAT);
            if (ImGui::MenuItem(last_time_label.c_str(), nullptr, gui.app_selector.app_list_sorted[LAST_TIME] != NOT_SORTED))
                sort_app_list(gui, emuenv, LAST_TIME);
            if (ImGui::MenuItem(title_label.c_str(), nullptr, gui.app_selector.app_list_sorted[TITLE] != NOT_SORTED))
                sort_app_list(gui, emuenv, TITLE);
            if (ImGui::MenuItem(title_id_label.c_str(), nullptr, gui.app_selector.app_list_sorted[TITLE_ID] != NOT_SORTED))
                sort_app_list(gui, emuenv, TITLE_ID);
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
    }
    if (ImGui::BeginPopup("app_filter")) {
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
        if (ImGui::MenuItem(lang["all"].c_str(), nullptr, app_region == ALL && app_compat_state == ALL_COMPAT_STATE)) {
            app_region = AppRegion::ALL;
            app_compat_state = ALL_COMPAT_STATE;
        }
        if (ImGui::BeginMenu(lang["by_region"].c_str())) {
            ImGui::SetWindowFontScale(1.1f * VIEWPORT_RES_SCALE.y);
            if (ImGui::MenuItem(lang["usa"].c_str(), nullptr, app_region == USA))
                app_region = USA;
            if (ImGui::MenuItem(lang["europe"].c_str(), nullptr, app_region == EURO))
                app_region = EURO;
            if (ImGui::MenuItem(lang["japan"].c_str(), nullptr, app_region == JAPAN))
                app_region = JAPAN;
            if (ImGui::MenuItem(lang["asia"].c_str(), nullptr, app_region == ASIA))
                app_region = ASIA;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(lang["by_type"].c_str())) {
            ImGui::SetWindowFontScale(1.1f * VIEWPORT_RES_SCALE.y);
            if (ImGui::MenuItem(lang["commercial"].c_str(), nullptr, app_region == COMMERCIAL))
                app_region = COMMERCIAL;
            if (ImGui::MenuItem(lang["homebrew"].c_str(), nullptr, app_region == HOMEBREW))
                app_region = HOMEBREW;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(lang["by_compatibility_state"].c_str())) {
            ImGui::SetWindowFontScale(1.1f * VIEWPORT_RES_SCALE.y);
            if (ImGui::MenuItem(lang["all"].c_str(), nullptr, app_compat_state == ALL_COMPAT_STATE))
                app_compat_state = ALL_COMPAT_STATE;
            auto &lang_compat = gui.lang.compatibility.states;
            for (int32_t i = compat::UNKNOWN; i <= compat::PLAYABLE; i++) {
                const auto compat_state = static_cast<compat::CompatibilityState>(i);
                ImGui::PushStyleColor(ImGuiCol_Text, compatibility_color(compat_state));
                if (ImGui::MenuItem(lang_compat[compat_state].c_str(), nullptr, app_compat_state == compat_state))
                    app_compat_state = compat_state;
                ImGui::PopStyleColor();
            }

            ImGui::EndMenu();
        }
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
    const auto search_bar_size = 120.f * VIEWPORT_SCALE.x;

    ImGui::SameLine(ImGui::GetColumnWidth() - ImGui::CalcTextSize(lang["refresh"].c_str()).x - search_bar_size - (78 * VIEWPORT_SCALE.x));
    if (ImGui::Button(lang["refresh"].c_str(), BUTTON_HEIGHT))
        init_user_apps(gui, emuenv);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_SEARCH_BAR_TEXT);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, GUI_COLOR_SEARCH_BAR_BG);
    gui.app_search_bar.Draw("##app_search_bar", search_bar_size);
    ImGui::PopStyleColor(2);
    ImGui::NextColumn();
    ImGui::Columns(1);
    const float SEPAR_POS = std::floor(BUTTON_HEIGHT.y + (ImGui::GetStyle().ItemSpacing.y * 2));
    ImGui::SetCursorPosY(SEPAR_POS);
    ImGui::Separator();
    const auto MARGIN_HEIGHT = (SEPAR_POS + 1.f);
    const auto GRID_MARGIN_WIDTH = (80.f * VIEWPORT_SCALE.x);
    const auto POS_APP_LIST = ImVec2(SCREEN_POS.x + GRID_MARGIN_WIDTH, SCREEN_POS.y + MARGIN_HEIGHT);
    const auto RAW_SIZE_APP_LIST = ImVec2(is_grid_view ? 820.f * VIEWPORT_SCALE.x : 900.f * VIEWPORT_SCALE.x, VIEWPORT_SIZE.y - INFORMATION_BAR_HEIGHT - MARGIN_HEIGHT);
    const auto SIZE_APP_LIST = ImVec2(floorf(RAW_SIZE_APP_LIST.x), floorf(RAW_SIZE_APP_LIST.y));

    static uint32_t max_pages = 0;

    // Variables to manage the scroll animation
    static float scroll_y; // Initialize the scroll position
    static float target_scroll_y = 0.f; // Target scroll position
    // Constants for page size
    const auto page_height = SIZE_APP_LIST.y + floorf(ImGui::GetStyle().ItemSpacing.y);

    const auto change_app_page = [&](const uint32_t target_page) {
        const auto page_diff = static_cast<int32_t>(target_page) - static_cast<int32_t>(current_page_index);
        change_bg(page_diff);
        current_page_index = target_page;
        target_scroll_y = current_page_index * page_height;
        ++page_scroll_animation_nonce;
        is_page_animating = true;
    };

    bool page_slider_dragging = false;
    bool page_slider_jump = false;
    if (is_grid_view && (max_pages > 0)) {
        const float point_size = 10.f * VIEWPORT_SCALE.x;
        const float spacing_size = 12.f * VIEWPORT_SCALE.y;
        const float point_full_height = point_size + spacing_size;

        const int total_pages = max_pages + 1;
        const int total_blocks = (total_pages + 19) / 20;
        const int current_block = current_page_index / 20;
        const int page_in_block = current_page_index % 20;
        const int pages_in_current_block = std::min(20, total_pages - current_block * 20);

        const auto draw_page_indicator = [&](const ImVec2 &pos, const bool is_current) {
            const auto &DRAW_LIST = ImGui::GetWindowDrawList();
            const auto rayon = point_size / 2.f;
            const auto CENTER_POS = ImVec2(pos.x + rayon, pos.y + rayon);
            const auto INDICATOR_POS_MAX = ImVec2(pos.x + point_size, pos.y + point_size);

            if (is_current) {
                if (gui.theme_page_indicator[PageIndicator::CUR])
                    DRAW_LIST->AddImageRounded(gui.theme_page_indicator[PageIndicator::CUR], pos, INDICATOR_POS_MAX, ImVec2(0, 0), ImVec2(1, 1), ARROW_COLOR, point_size);
                else
                    DRAW_LIST->AddCircleFilled(CENTER_POS, rayon, ARROW_COLOR);
            } else {
                if (gui.theme_page_indicator[PageIndicator::BASE])
                    DRAW_LIST->AddImageRounded(gui.theme_page_indicator[PageIndicator::BASE], pos, INDICATOR_POS_MAX, ImVec2(0, 0), ImVec2(1, 1), ARROW_COLOR, point_size);
                else
                    DRAW_LIST->AddCircleFilled(CENTER_POS, rayon, IM_COL32(200.f, 200.f, 200.f, 80.f));
            }
        };

        const ImVec2 BEGIN_OFFSET(15.f * VIEWPORT_SCALE.x, 2.f * VIEWPORT_SCALE.y);

        // Column of blocks (left) – only if 20+ pages
        if (total_pages > 20) {
            const float total_height_blocks = (total_blocks * point_full_height) - spacing_size;
            const float offset_y_blocks = (WINDOW_SIZE.y - total_height_blocks) / 2.f;
            const ImVec2 base_pos_blocks = ImVec2(SCREEN_POS.x + BEGIN_OFFSET.x, SCREEN_POS.y + offset_y_blocks + BEGIN_OFFSET.y);

            for (int i = 0; i < total_blocks; ++i) {
                const float pos_x = base_pos_blocks.x;
                const float pos_y = base_pos_blocks.y + i * point_full_height;
                draw_page_indicator(ImVec2(pos_x, pos_y), i == current_block);
            }
        }

        // Total height
        const float total_height_pages = (pages_in_current_block * point_full_height) - spacing_size;
        const float offset_y_pages = (WINDOW_SIZE.y - total_height_pages) / 2.f;

        // Position base of the pages column (right)
        const ImVec2 base_pos_pages = ImVec2(SCREEN_POS.x + (total_pages > 20 ? BEGIN_OFFSET.x + point_full_height : BEGIN_OFFSET.x), SCREEN_POS.y + offset_y_pages + BEGIN_OFFSET.y);

        // Column of pages (right)
        for (int i = 0; i < pages_in_current_block; ++i) {
            const float pos_x = base_pos_pages.x;
            const float pos_y = base_pos_pages.y + (i * point_full_height);
            draw_page_indicator(ImVec2(pos_x, pos_y), (i == page_in_block));
        }

        if (max_pages > 0) {
            const float slider_width = 14.f * VIEWPORT_SCALE.x;
            const ImVec2 slider_pos(GRID_MARGIN_WIDTH - slider_width, MARGIN_HEIGHT);
            const ImVec2 slider_size(slider_width, SIZE_APP_LIST.y);
            int current_page = current_page_index;
            ImGui::SetCursorPos(slider_pos);
            const bool page_changed = ImGui::VSliderInt("##page_slider", slider_size, &current_page, max_pages, 0, "", ImGuiSliderFlags_AlwaysClamp);
            page_slider_dragging = ImGui::IsItemActive();
            if (page_changed) {
                const auto page_delta = static_cast<int32_t>(current_page) - static_cast<int32_t>(current_page_index);
                page_slider_jump = page_slider_dragging && std::abs(page_delta) > 1;
                change_app_page(current_page);
            }
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && (ImGui::GetIO().MouseWheel != 0)) {
                const auto wheel = static_cast<int32_t>(ImGui::GetIO().MouseWheel);
                if (((wheel == -1) && (current_page_index < max_pages)) || ((wheel == 1) && (current_page_index > 0)))
                    change_app_page(current_page_index - wheel);
            }
        }
    }

    ImGui::SetNextWindowPos(is_grid_view ? POS_APP_LIST : ImVec2(SCREEN_POS.x, SCREEN_POS.y + MARGIN_HEIGHT), ImGuiCond_Always);
    auto child_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings;
    if (gui.is_nav_button || pending_app_open_requested)
        child_flags |= ImGuiWindowFlags_NoMouseInputs;

    // Disable scrollbar in grid view
    if (is_grid_view)
        child_flags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::BeginChild("##apps_list", SIZE_APP_LIST, ImGuiChildFlags_None, child_flags);
    ImGuiWindow *const apps_list_window = ImGui::GetCurrentWindow();

    const auto APP_LIST_SCREEN_POS = ImGui::GetCursorScreenPos();

    // Get the current scroll position and maximum scroll position
    const float current_scroll_pos = ImGui::GetScrollY();

    const auto current_pos = is_grid_view ? static_cast<float>(current_page_index) : current_scroll_pos;

    const float max_scroll_pos = ImGui::GetScrollMaxY();
    const float max_pos = is_grid_view ? static_cast<float>(max_pages) : max_scroll_pos;

    static bool drag_scroll_active = false;
    static uint32_t drag_scroll_start_page_index = 0;
    if (is_grid_view && !is_page_animating && !drag_scroll_active) {
        // Check if current page index is out of bounds
        if (current_page_index > max_pages)
            current_page_index = 0;

        // Recalibrate scroll position if display size changed in grid view
        const float correct_scroll_y = current_page_index * page_height;
        if (fabs(scroll_y - correct_scroll_y) > 1.0f) {
            scroll_y = correct_scroll_y;
            ImGui::SetScrollY(scroll_y);
        }
    }

    if (is_grid_view) {
        ImGuiIO &io = ImGui::GetIO();

        if (!io.MouseDown[ImGuiMouseButton_Left]) {
            ImGui::ScrollWhenDragging();
            const float max_page_scroll = max_pages * page_height;
            scroll_y = std::clamp(apps_list_window->Scroll.y, 0.f, max_page_scroll);
            const float current_page_scroll = current_page_index * page_height;
            const bool has_partial_scroll = fabsf(scroll_y - current_page_scroll) > 0.01f;

            if (!is_page_animating && (drag_scroll_active || has_partial_scroll)) {
                const uint32_t start_page_index = drag_scroll_active ? drag_scroll_start_page_index : current_page_index;
                scroll_y = std::clamp(apps_list_window->Scroll.y, 0.f, max_page_scroll);
                uint32_t target_page = start_page_index;
                if (drag_scroll_active) {
                    target_page = static_cast<uint32_t>(std::clamp(std::round(scroll_y / page_height), 0.f, static_cast<float>(max_pages)));
                } else {
                    const float page_offset = (scroll_y - (start_page_index * page_height)) / page_height;
                    if ((page_offset >= 0.5f) && (start_page_index < max_pages))
                        target_page = start_page_index + 1;
                    else if ((page_offset <= -0.5f) && (start_page_index > 0))
                        target_page = start_page_index - 1;
                }

                if (target_page != current_page_index)
                    change_app_page(target_page);
                else if (fabsf(scroll_y - (target_page * page_height)) > 0.01f) {
                    target_scroll_y = target_page * page_height;
                    ++page_scroll_animation_nonce;
                    is_page_animating = true;
                }
            }

            drag_scroll_active = false;
        } else {
            ImGuiContext &imgui = *ImGui::GetCurrentContext();
            const auto is_apps_list_window = [&](ImGuiWindow *window) {
                while (window) {
                    if (window == apps_list_window)
                        return true;
                    window = window->ParentWindow;
                }

                return false;
            };

            const bool is_hovering_apps_list = is_apps_list_window(imgui.HoveredWindow);
            if (is_hovering_apps_list) {
                const float max_page_scroll = max_pages * page_height;
                const bool is_new_touch_press = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
                if (is_new_touch_press) {
                    drag_scroll_active = false;
                    scroll_y = std::clamp(apps_list_window->Scroll.y, 0.f, max_page_scroll);
                    drag_scroll_start_page_index = static_cast<uint32_t>(std::clamp(std::round(scroll_y / page_height), 0.f, static_cast<float>(max_pages)));
                    target_scroll_y = scroll_y;
                    ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
                }

                const ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
                const float abs_drag_x = fabsf(drag_delta.x);
                const float abs_drag_y = fabsf(drag_delta.y);

                if (!drag_scroll_active && !is_new_touch_press && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && (abs_drag_y > abs_drag_x)) {
                    drag_scroll_active = true;
                    if (current_page_index != drag_scroll_start_page_index) {
                        change_bg(static_cast<int32_t>(drag_scroll_start_page_index) - static_cast<int32_t>(current_page_index));
                        current_page_index = drag_scroll_start_page_index;
                    }
                    is_page_animating = false;
                    target_scroll_y = scroll_y;

                    if ((imgui.ActiveId != 0) && is_apps_list_window(imgui.ActiveIdWindow))
                        ImGui::ClearActiveID();
                }

                ImGuiWindow *const previous_current_window = imgui.CurrentWindow;
                ImGuiWindow *const previous_hovered_window = imgui.HoveredWindow;
                imgui.CurrentWindow = apps_list_window;
                imgui.HoveredWindow = apps_list_window;
                if (!is_new_touch_press)
                    ImGui::ScrollWhenDragging();
                imgui.CurrentWindow = previous_current_window;
                imgui.HoveredWindow = previous_hovered_window;

                scroll_y = std::clamp(apps_list_window->Scroll.y, 0.f, max_page_scroll);
            }

            if (drag_scroll_active) {
                const float max_page_scroll = max_pages * page_height;
                scroll_y = std::clamp(apps_list_window->Scroll.y, 0.f, max_page_scroll);
                const auto drag_page_index = static_cast<uint32_t>(std::clamp(std::round(scroll_y / page_height), 0.f, static_cast<float>(max_pages)));
                if (drag_page_index != current_page_index) {
                    change_bg(static_cast<int32_t>(drag_page_index) - static_cast<int32_t>(current_page_index));
                    current_page_index = drag_page_index;
                }
            }
        }
    }

    if (is_grid_view && page_slider_jump) {
        scroll_y = current_page_index * page_height;
        target_scroll_y = scroll_y;
        is_page_animating = false;
        ImGui::SetScrollY(scroll_y);
    }

    const uint32_t visible_page_index = is_grid_view
        ? static_cast<uint32_t>(std::clamp(std::round(scroll_y / page_height), 0.f, static_cast<float>(max_pages)))
        : current_page_index;

    enum ShakeType {
        None,
        Vertical,
        Horizontal
    };

    static float current_shake_start_time = 0.0f;
    static ShakeType current_shake = ShakeType::None;

    // Apply the scroll animation
    if (is_page_animating) {
        if (!(is_page_animating = set_scroll_animation(scroll_y, target_scroll_y, fmt::format("##home_screen_{}_{}", current_page_index, page_scroll_animation_nonce), set_current_scroll_y))) {
            current_shake = ShakeType::Vertical;
            current_shake_start_time = ImGui::GetTime(); // Start the shake effect
        }
    }

    const auto GRID_COLUMN_SIZE = ICON_SIZE.x + (80.f * VIEWPORT_SCALE.x);
    const std::map<uint32_t, std::vector<float>> GRID_COLUMN_SIZE_DYNAMIC = {
        { 3, { GRID_COLUMN_SIZE * 1.5f, GRID_COLUMN_SIZE, GRID_COLUMN_SIZE * 1.5f } },
        { 4, { GRID_COLUMN_SIZE, GRID_COLUMN_SIZE, GRID_COLUMN_SIZE, GRID_COLUMN_SIZE } },
    };

    const ImVec2 list_selectable_size(0.f, ICON_SIZE.y + (10.f * VIEWPORT_SCALE.y));
    const ImVec2 SELECTABLE_APP_SIZE = emuenv.cfg.apps_list_grid ? ICON_SIZE : list_selectable_size;

    if (!is_grid_view) {
        ImGui::Columns(7, "##app_list_view", true);
        ImGui::SetColumnWidth(0, column_icon_size);
        ImGui::SetColumnWidth(1, compat_size);
        ImGui::SetColumnWidth(2, title_id_size);
        ImGui::SetColumnWidth(3, app_ver_size);
        ImGui::SetColumnWidth(4, category_size);
        ImGui::SetColumnWidth(5, last_time_size);
    }

    const auto Clamp = [](float value, float min, float max) {
        return std::max(min, std::min(value, max));
    };

    ImGui::SetWindowFontScale(is_grid_view ? VIEWPORT_RES_SCALE.y : 1.f);
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);

    std::vector<std::string> visible_apps{};
    uint32_t drawn_page_count = 0;
    const auto display_app = [&](const std::vector<gui::App> &apps_list) {
        const auto app_times = app::get_user_app_times(emuenv);
        uint32_t current_col = 0;
        uint32_t current_line = 0;
        uint32_t max_cols = LINE_PATTERN[current_line]; // Default to 3 columns

        const auto &DRAW_LIST = ImGui::GetWindowDrawList();

        static ImVec2 animated_circle_pos{};

        int apps_per_page = 10;
        int current_page = 0;
        int apps_drawn = 0;
        uint32_t apps_skipped = 0;
        bool is_child_opened = false;
        float previous_child_pos_y = 0.f;
        bool has_previous_child = false;

        for (const auto &app : apps_list) {
            const bool is_sys_app = is_emu_app(app.path);
            const auto compat_state = gui.compat.app_compat_db.contains(app.title_id)
                ? gui.compat.app_compat_db.at(app.title_id).state
                : compat::UNKNOWN;
            const auto app_time_it = app_times.find(app_launch_path(app.path));
            const auto last_time = app_time_it != app_times.end() ? app_time_it->second.last_time_used : std::time_t{};
            const bool custom_config = config::has_custom_config(emuenv.config_path, app.path);
            bool skip_app = false;

            if (!is_sys_app) {
                // Filter app by region and type
                if (app_filter(app.title_id))
                    skip_app = true;

                // Filter commercial app by compatibility
                if ((app_region != HOMEBREW) && (app_compat_state != ALL_COMPAT_STATE) && (app_compat_state != compat_state))
                    skip_app = true;
            }

            // Search app by title or title id
            if (!gui.app_search_bar.PassFilter(app.title.c_str()) && !gui.app_search_bar.PassFilter(app.title_id.c_str()))
                skip_app = true;

            if (skip_app) {
                if (is_grid_view && is_child_opened && (&app == &apps_list.back())) {
                    ImGui::EndChild();
                    current_page++;
                    is_child_opened = false;
                }
                continue;
            }

            apps_list_filtered.push_back({ app.path, current_line, current_col });

            if (is_grid_view) {
                if ((apps_drawn % apps_per_page) == 0) {
                    const auto title = "##home_screen_page_" + std::to_string(current_page);
                    ImGui::SetNextWindowPos(ImVec2(floorf(APP_LIST_SCREEN_POS.x), floorf(APP_LIST_SCREEN_POS.y + (current_page * page_height))), ImGuiCond_Always);
                    ImGui::BeginChild(title.c_str(), SIZE_APP_LIST, ImGuiChildFlags_None, child_flags);
                    ++drawn_page_count;
                    is_child_opened = true;
                }
                max_cols = LINE_PATTERN[current_line];
            }

            auto INIT_ICON_POS = is_grid_view ? ImVec2((GRID_COLUMN_SIZE_DYNAMIC.at(max_cols).at(current_col) / 2.f) - (10.f * VIEWPORT_SCALE.x), current_line * (ICON_SIZE.y + ImGui::GetStyle().ItemSpacing.y + ImGui::GetFontSize())) : ImGui::GetCursorPos();
            for (uint32_t i = 0; i < current_col; ++i)
                INIT_ICON_POS.x += GRID_COLUMN_SIZE_DYNAMIC.at(max_cols).at(i);

            if (is_grid_view) {
                if (max_cols == 3) {
                    const auto padding = GRID_COLUMN_SIZE / 4.f;
                    if (current_col == 0) {
                        INIT_ICON_POS.x += padding;
                    } else if (current_col == 2) {
                        INIT_ICON_POS.x -= padding;
                    }
                }

                if (current_line == 0)
                    INIT_ICON_POS.y += ImGui::GetStyle().ItemSpacing.y;
            }

            const ImVec2 ICON_POS(is_grid_view ? ImVec2(INIT_ICON_POS.x - (ICON_SIZE.x / 2.f), INIT_ICON_POS.y) : ImVec2(INIT_ICON_POS.x + (5.f * VIEWPORT_SCALE.x), INIT_ICON_POS.y + (5.f * VIEWPORT_SCALE.y)));

            ImGui::PushID(app.path.c_str());
            const auto is_current_app = current_selected_app == app.path;
            const auto is_current_app_selected = is_current_app && gui.is_nav_button;

            if (is_grid_view)
                ImGui::SetCursorPos(ICON_POS);

            // Check if the current app is selected.
            if (is_grid_view ? ImGui::InvisibleButton("##icon", SELECTABLE_APP_SIZE) : ImGui::Selectable("##icon", is_current_app_selected, ImGuiSelectableFlags_SpanAllColumns, SELECTABLE_APP_SIZE)) {
                request_app_open(gui, emuenv.cfg.show_live_area_screen, app.path);
            }

            if (!gui.configuration_menu.custom_settings_dialog && (ImGui::IsItemHovered() || is_current_app_selected))
                emuenv.app_path = app.path;
            if (emuenv.app_path == app.path)
                draw_app_context_menu(gui, emuenv, app.path, session);
            const auto STITLE_SIZE = ImGui::CalcTextSize(app.stitle.c_str(), 0, false, ICON_SIZE.x + (42.f * VIEWPORT_SCALE.x));

            bool element_is_drawn = false;
            if (is_grid_view) {
                // Determine if the element is within the visible area of the window.
                const int32_t range = 2;
                const uint32_t start_page = std::max(static_cast<int32_t>(visible_page_index) - range, 0);
                const uint32_t end_page = std::min(visible_page_index + range, max_pages);

                element_is_drawn = (current_page >= start_page) && (current_page <= end_page);

                // When the app is selected
                if (is_current_app_selected && !is_page_animating && (current_page != current_page_index))
                    // If the app is at the top of the list and needs to scroll up
                    change_app_page(current_page);

                // Add the current app on current page to the visible apps list.
                if (element_is_drawn && (current_page == visible_page_index))
                    visible_apps.emplace_back(app.path);
            } else {
                const auto ITEM_RECT_MAX = ImGui::GetItemRectMax().y;

                // Get the min and full item rect max, depending on the view mode.
                const auto MIN_ITEM_RECT_MAX = ITEM_RECT_MAX - (5.f * VIEWPORT_SCALE.y);

                auto item_rect_min = ITEM_RECT_MAX - SELECTABLE_APP_SIZE.y;
                const auto MAX_LIST_POS = POS_APP_LIST.y + SIZE_APP_LIST.y;

                // Determine if the element is within the visible area of the window.
                element_is_drawn = (MIN_ITEM_RECT_MAX >= POS_APP_LIST.y) && (item_rect_min <= MAX_LIST_POS);

                if (is_current_app_selected) {
                    // If the app is at the top of the list and needs to scroll up
                    if (item_rect_min < POS_APP_LIST.y) {
                        target_scroll_y = current_pos - SIZE_APP_LIST.y - (POS_APP_LIST.y - item_rect_min) + SELECTABLE_APP_SIZE.y;
                        is_page_animating = true;
                    }
                    // If the app is at the bottom of the list and needs to scroll down
                    else if (ITEM_RECT_MAX > MAX_LIST_POS) {
                        target_scroll_y = current_pos + SIZE_APP_LIST.y + (ITEM_RECT_MAX - MAX_LIST_POS) - SELECTABLE_APP_SIZE.y;
                        is_page_animating = true;
                    }
                }

                // Add the current app index to the visible apps list.
                if (element_is_drawn && (item_rect_min >= POS_APP_LIST.y) && (ITEM_RECT_MAX <= MAX_LIST_POS))
                    visible_apps.emplace_back(app.path);
            }

            // Draw the app icons and custom config button only when they are within the visible area.
            if (element_is_drawn) {
                // Set the current selected app index to the current app index when the app is hovered.
                if (!gui.is_nav_button && ImGui::IsItemHovered())
                    current_selected_app = app.path;

                ImGui::SetCursorPos(ICON_POS);
                const ImVec2 ICON_POS_MIN(ImGui::GetCursorScreenPos());
                const ImVec2 ICON_POS_MAX(ICON_POS_MIN.x + ICON_SIZE.x, ICON_POS_MIN.y + ICON_SIZE.y);

                // Draw the app icon
                if (const auto app_icon = get_app_icon(gui, app.path)) {
                    // Current time for the animation
                    float time = ImGui::GetTime();
                    float tilt_angle = sinf(time * 2.0f) * 0.3f; // "Pitch effect (sinusoidal)"

                    // Calculate the horizontal offset based on the current column
                    float dx = (float)current_col - (max_cols - 1) / 2.0f;
                    float tilt = dx * tilt_angle;

                    // Zoom effect "relief"
                    float scale = 1.0f + tilt * 0.1f; // Intensité du zoom

                    // Horizontal compression for "tilted" effect
                    float stretch_x = 1.0f - fabsf(tilt) * 0.3f; // Compression horizontale en fonction du tilt

                    const float SHAKE_DURATION = 2.f;
                    const float SHAKE_DAMPING = 2.0f;

                    const float SHAKE_FREQ = 25.f;
                    const float SHAKE_INTENSITY = 0.06f;

                    float skew = 0.0f;
                    if (current_shake != ShakeType::None) {
                        const float elapsed = ImGui::GetTime() - current_shake_start_time;

                        if (elapsed < SHAKE_DURATION)
                            skew = sinf(elapsed * SHAKE_FREQ) * expf(-elapsed * SHAKE_DAMPING) * SHAKE_INTENSITY;
                        else
                            current_shake = ShakeType::None;
                    }

                    // Apply skew offset
                    const float offset_x = (current_shake == ShakeType::Horizontal) ? skew * ICON_SIZE.x : 0.0f;
                    const float offset_y = (current_shake == ShakeType::Vertical) ? skew * ICON_SIZE.y : 0.0f;

                    const float rotation_angle = sinf(time * 2.0f) * tilt_angle;
                    const ImVec2 rotation_offset(0.0f, rotation_angle * 10.0f);

                    ImVec2 center = ImVec2(ICON_POS_MIN.x + (ICON_SIZE.x / 2.f) + offset_x, ICON_POS_MIN.y + (ICON_SIZE.y / 2.f) + offset_y);
                    ImVec2 half_size = ImVec2((ICON_SIZE.x * scale * stretch_x) / 2.f, (ICON_SIZE.y * scale) / 2.f);

                    ImVec2 min_pos = ImVec2(center.x - half_size.x + rotation_offset.x, center.y - half_size.y + rotation_offset.y);
                    ImVec2 max_pos = ImVec2(center.x + half_size.x + rotation_offset.x, center.y + half_size.y + rotation_offset.y);

                    DRAW_LIST->AddImageRounded(
                        *app_icon,
                        min_pos,
                        max_pos,
                        ImVec2(0.0f, 0.0f),
                        ImVec2(1.0f, 1.0f),
                        IM_COL32_WHITE,
                        ICON_SIZE.x * VIEWPORT_SCALE.x,
                        ImDrawFlags_RoundCornersAll);
                } else {
                    DRAW_LIST->AddRectFilled(ICON_POS_MIN, ICON_POS_MAX, IM_COL32_WHITE, ICON_SIZE.x, ImDrawFlags_RoundCornersAll);
                }

                if (is_current_app) {
                    // Draw the animated circle when an app is selected
                    const float speed = 20.f * ImGui::GetIO().DeltaTime;
                    animated_circle_pos.x = std::lerp(animated_circle_pos.x, ICON_POS_MIN.x, speed);
                    animated_circle_pos.y = std::lerp(animated_circle_pos.y, ICON_POS_MIN.y, speed);

                    const auto COLOR_PULSE_FILL = get_selectable_color_pulse(120.f);
                    const auto COLOR_PULSE_BORDER = get_selectable_color_pulse();

                    // Draw the animated circle
                    const ImVec2 center(animated_circle_pos.x + ICON_SIZE.x / 2.f, animated_circle_pos.y + ICON_SIZE.y / 2.f);
                    const float radius = ICON_SIZE.x / 2.f;
                    const ImVec2 ANIMATED_CIRLCLE_POS_MAX(animated_circle_pos.x + ICON_SIZE.x, animated_circle_pos.y + ICON_SIZE.y);

                    DRAW_LIST->AddRectFilled(animated_circle_pos, ANIMATED_CIRLCLE_POS_MAX, COLOR_PULSE_FILL,
                        ICON_SIZE.x, ImDrawFlags_RoundCornersAll);
                    DRAW_LIST->AddCircle(center, radius, COLOR_PULSE_BORDER, 0, 5.f * VIEWPORT_SCALE.x);
                }

                // Draw the custom config button
                if (custom_config) {
                    if (is_grid_view)
                        ImGui::SetCursorPos(ICON_POS);
                    ImGui::SetCursorPosY(ICON_POS.y + ICON_SIZE.y - (ImGui::GetFontSize() * 2) - (7.8f * viewport.manual_dpi_scale));
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                    ImGui::Button("CC", ImVec2(40.f * VIEWPORT_SCALE.x, 0.f));
                    ImGui::PopStyleColor();
                }
            } else if (!gui.is_nav_button && (current_selected_app == app.path)) {
                // When the app is selected but not visible, reset the current selected app index.
                current_selected_app.clear();
            }

            if (!is_grid_view)
                ImGui::NextColumn();

            // Draw the compatibility badge for commercial apps when they are within the visible area.
            if (element_is_drawn && (app.title_id.starts_with("PCS") || app.title_id == "NPXS10007")) {
                const auto compat_state_vec4 = compatibility_color(compat_state);
                const ImU32 compat_state_color = IM_COL32((int)(compat_state_vec4.x * 255.0f), (int)(compat_state_vec4.y * 255.0f), (int)(compat_state_vec4.z * 255.0f), (int)(compat_state_vec4.w * 255.0f));
                const auto current_pos = ImGui::GetCursorPos();
                const auto grid_compat_padding = 4.f * VIEWPORT_SCALE.x;
                const auto compat_state_pos = emuenv.cfg.apps_list_grid ? ImVec2(ICON_POS.x + full_compat_radius + grid_compat_padding, INIT_ICON_POS.y + full_compat_radius + grid_compat_padding) : ImVec2(current_pos.x + ((ImGui::GetColumnWidth() - column_padding_size) / 2.f), current_pos.y + (ICON_SIZE.y / 2.f));
                ImGui::SetCursorPos(compat_state_pos);
                const ImVec2 COMPAT_CENTER_POS(ImGui::GetCursorScreenPos());
                DRAW_LIST->AddCircleFilled(COMPAT_CENTER_POS, full_compat_radius, IM_COL32(0, 0, 0, 255));
                DRAW_LIST->AddCircleFilled(COMPAT_CENTER_POS, compat_radius, compat_state_color);
            }

            if (!is_grid_view) {
                ImGui::NextColumn();
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_Text, !gui.theme_backgrounds_font_color.empty() && emuenv.app.user_list.users[emuenv.io.user_id].use_theme_bg ? gui.theme_backgrounds_font_color[gui.current_theme_bg] : GUI_COLOR_TEXT);
                ImGui::Selectable(app.title_id.c_str(), false, ImGuiSelectableFlags_None, list_selectable_size);
                ImGui::NextColumn();
                ImGui::Selectable(app.app_ver == "0.00" ? "-" : app.app_ver.c_str(), false, ImGuiSelectableFlags_None, list_selectable_size);
                ImGui::NextColumn();
                ImGui::Selectable(app.category.c_str(), false, ImGuiSelectableFlags_None, list_selectable_size);
                ImGui::NextColumn();
                if (last_time) {
                    tm date_tm = {};
                    SAFE_LOCALTIME(&last_time, &date_tm);
                    auto LAST_TIME = get_date_time(gui, emuenv, date_tm);
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 1.f));
                    ImGui::Selectable(LAST_TIME[DateTime::DATE_MINI].c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, ICON_SIZE.y / 2.f));
                    ImGui::PopStyleVar();
                    const auto CLOCK_STR = emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR ? fmt::format("{} {}", LAST_TIME[DateTime::CLOCK], LAST_TIME[DateTime::DAY_MOMENT]) : LAST_TIME[DateTime::CLOCK];
                    const auto HALF_CLOCK_SIZE = ImGui::CalcTextSize(CLOCK_STR.c_str()).x / 2.f;
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - (10.f * VIEWPORT_SCALE.x) + (ImGui::GetColumnWidth() / 2.f) - HALF_CLOCK_SIZE);
                    ImGui::Text("%s", CLOCK_STR.c_str());
                } else
                    ImGui::Selectable(!gui.lang.app_context.info["never"].empty() ? gui.lang.app_context.info["never"].c_str() : "Never", false, ImGuiSelectableFlags_None, ImVec2(0.f, ICON_SIZE.y));
                ImGui::NextColumn();
                ImGui::Selectable(app.title.c_str(), false, ImGuiSelectableFlags_None, list_selectable_size);
                ImGui::NextColumn();
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();
            } else if (element_is_drawn)
                draw_ellipsis_text(app.stitle, GRID_COLUMN_SIZE - (51.f * VIEWPORT_SCALE.x), ImVec2(INIT_ICON_POS.x, ICON_POS.y + SELECTABLE_APP_SIZE.y + ImGui::GetStyle().ItemSpacing.y + ImGui::GetFontSize()), ImVec2(0.f, -1.f), !gui.theme_backgrounds_font_color.empty() && emuenv.app.user_list.users[emuenv.io.user_id].use_theme_bg ? gui.theme_backgrounds_font_color[gui.current_theme_bg] : GUI_COLOR_TEXT);
            ImGui::PopID();
            if (is_grid_view) {
                apps_drawn++;
                if (((apps_drawn % apps_per_page) == 0) || (&app == &apps_list.back())) {
                    ImGui::EndChild();
                    current_page++;
                    is_child_opened = false;
                }
                ++current_col;
                if (current_col >= max_cols) {
                    current_col = 0;
                    current_line = (current_line + 1) % line_pattern_size;
                }
            }
        }
    };

    std::vector<gui::App> all_apps;
    if (emuenv.cfg.display_system_apps)
        all_apps.insert(all_apps.end(), gui.app_selector.sys_apps.begin(), gui.app_selector.sys_apps.end());
    const auto user_apps = app::get_apps(emuenv);
    std::ranges::copy_if(user_apps, std::back_inserter(all_apps), [&](const gui::App &app) {
        return emuenv.cfg.display_system_apps || !app.path.starts_with("vs0:");
    });

    display_app(all_apps);

    if (is_grid_view) {
        max_pages = drawn_page_count > 0 ? drawn_page_count - 1 : 0;
        if (current_page_index > max_pages)
            current_page_index = max_pages;
    }

    ImGui::PopStyleColor();
    ImGui::Columns(1);
    ImGui::SetWindowFontScale(1.f);
    if (!is_grid_view)
        ImGui::ScrollWhenDragging();
    ImGui::EndChild();

    // When visible apps list is not empty, set first visible app
    if (!visible_apps.empty())
        first_visible_app = visible_apps.front();

    const auto SELECTABLE_SIZE = ImVec2(50.f * VIEWPORT_SCALE.x, 60.f * VIEWPORT_SCALE.y);

    const auto window_draw_list = ImGui::GetWindowDrawList();

    // Set arrow position of width
    const auto ARROW_WIDTH_POS = VIEWPORT_SIZE.x - (30.f * VIEWPORT_SCALE.x);
    const auto ARROW_DRAW_WIDTH_POS = SCREEN_POS.x + ARROW_WIDTH_POS;
    const auto ARROW_SELECT_WIDTH_POS = ARROW_WIDTH_POS - (SELECTABLE_SIZE.x / 2.f);

    // Set arrow position of up
    if (current_pos > 0) {
        const auto ARROW_UP_HEIGHT_POS = 110.f * VIEWPORT_SCALE.y;
        const auto ARROW_UP_CENTER = ImVec2(ARROW_DRAW_WIDTH_POS, VIEWPORT_POS.y + ARROW_UP_HEIGHT_POS);
        window_draw_list->AddTriangleFilled(
            ImVec2(ARROW_UP_CENTER.x - (20.f * VIEWPORT_SCALE.x), ARROW_UP_CENTER.y + (16.f * VIEWPORT_SCALE.y)),
            ImVec2(ARROW_UP_CENTER.x, ARROW_UP_CENTER.y - (16.f * VIEWPORT_SCALE.y)),
            ImVec2(ARROW_UP_CENTER.x + (20.f * VIEWPORT_SCALE.x), ARROW_UP_CENTER.y + (16.f * VIEWPORT_SCALE.y)), ARROW_COLOR);
        ImGui::SetCursorPos(ImVec2(ARROW_SELECT_WIDTH_POS, ARROW_UP_HEIGHT_POS - SELECTABLE_SIZE.y));
        if (!pending_app_open_requested && (ImGui::InvisibleButton("##upp", SELECTABLE_SIZE) || (!ImGui::GetIO().WantTextInput && ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_leftstick_up)))) {
            gui.is_nav_button = false;
            current_selected_app.clear();
            if (is_grid_view)
                change_app_page(current_page_index - 1);
            else {
                target_scroll_y = current_pos - page_height;
                is_page_animating = true;
            }
        }
    }

    // Set arrow position of middle
    if (!gui.live_area_current_open_apps_list.empty()) {
        const auto ARROW_CENTER_HEIGHT_POS = VIEWPORT_SIZE.y - (250.f * VIEWPORT_SCALE.y);
        const auto ARROW_CENTER = ImVec2(ARROW_DRAW_WIDTH_POS, VIEWPORT_POS.y + ARROW_CENTER_HEIGHT_POS);
        window_draw_list->AddTriangleFilled(
            ImVec2(ARROW_CENTER.x - (16.f * VIEWPORT_SCALE.x), ARROW_CENTER.y - (20.f * VIEWPORT_SCALE.y)),
            ImVec2(ARROW_CENTER.x + (16.f * VIEWPORT_SCALE.x), ARROW_CENTER.y),
            ImVec2(ARROW_CENTER.x - (16.f * VIEWPORT_SCALE.x), ARROW_CENTER.y + (20.f * VIEWPORT_SCALE.y)), ARROW_COLOR);
        ImGui::SetCursorPos(ImVec2(ARROW_SELECT_WIDTH_POS, ARROW_CENTER_HEIGHT_POS - SELECTABLE_SIZE.y));
        if (!pending_app_open_requested && !gui.vita_area.app_close && ImGui::InvisibleButton("##right", SELECTABLE_SIZE) && !ImGui::GetIO().WantTextInput) {
            last_time["start"] = 0;
            ++gui.live_area_app_current_open;
            target_scroll_x = (gui.live_area_app_current_open + 1) * WINDOW_SIZE.x;
            ++live_area_scroll_animation_nonce;
            is_live_area_animating = true;
        }
    }

    // Set arrow position of down
    if (current_pos < max_pos) {
        const auto ARROW_DOWN_HEIGHT_POS = VIEWPORT_SIZE.y - (30.f * VIEWPORT_SCALE.y);
        const ImVec2 ARROW_DOWN_CENTER(ARROW_DRAW_WIDTH_POS, VIEWPORT_POS.y + ARROW_DOWN_HEIGHT_POS);
        window_draw_list->AddTriangleFilled(
            ImVec2(ARROW_DOWN_CENTER.x + (20.f * VIEWPORT_SCALE.x), ARROW_DOWN_CENTER.y - (16.f * VIEWPORT_SCALE.y)),
            ImVec2(ARROW_DOWN_CENTER.x, ARROW_DOWN_CENTER.y + (16.f * VIEWPORT_SCALE.y)),
            ImVec2(ARROW_DOWN_CENTER.x - (20.f * VIEWPORT_SCALE.x), ARROW_DOWN_CENTER.y - (16.f * VIEWPORT_SCALE.y)), ARROW_COLOR);
        ImGui::SetCursorPos(ImVec2(ARROW_SELECT_WIDTH_POS, ARROW_DOWN_HEIGHT_POS - SELECTABLE_SIZE.y));
        if (!pending_app_open_requested && (ImGui::InvisibleButton("##down", SELECTABLE_SIZE) || (!ImGui::GetIO().WantTextInput && ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_leftstick_down)))) {
            gui.is_nav_button = false;
            current_selected_app.clear();
            if (is_grid_view)
                change_app_page(current_page_index + 1);
            else {
                target_scroll_y = current_pos + page_height;
                is_page_animating = true;
            }
        }
    }
    ImGui::EndChild();

    for (uint32_t i = 0; i < gui.live_area_current_open_apps_list.size(); i++) {
        ImGui::SetNextWindowPos(ImVec2(SCREEN_POS.x + (WINDOW_SIZE.x * (i + 1)), SCREEN_POS.y), ImGuiCond_Always);
        draw_live_area_screen(gui, emuenv, session, static_cast<int32_t>(i));
    }

    if (is_live_area_animating) {
        if (!(is_live_area_animating = set_scroll_animation(scroll_x, target_scroll_x, fmt::format("##live_area_{}_{}", gui.live_area_app_current_open, live_area_scroll_animation_nonce), set_current_scroll_x))) {
            current_shake = ShakeType::Horizontal;
            current_shake_start_time = ImGui::GetTime();
        }
    }

    // Recalibrate scroll position if display size changed
    const float correct_scroll_x = (gui.live_area_app_current_open + 1) * WINDOW_SIZE.x;
    if (fabs(scroll_x - correct_scroll_x) > 1.0f) {
        if (!is_live_area_animating || fabs(target_scroll_x - correct_scroll_x) > 1.0f)
            ++live_area_scroll_animation_nonce;
        target_scroll_x = correct_scroll_x;
        is_live_area_animating = true;
    }

    if (!is_live_area_animating && !gui.live_area_app_closing.empty()) {
        const auto closing_app = get_live_area_current_open_apps_list_index(gui, gui.live_area_app_closing);
        if (closing_app != gui.live_area_current_open_apps_list.end())
            gui.live_area_current_open_apps_list.erase(closing_app);
        gui.live_area_app_closing.clear();
    }

    if (pending_app_open_requested) {
        if (pending_app_open_controls_please_wait)
            gui.vita_area.please_wait = true;

        if (is_page_animating || is_live_area_animating || drag_scroll_active)
            pending_app_open_wait_frame_displayed = false;
        else if (!pending_app_open_wait_frame_displayed)
            pending_app_open_wait_frame_displayed = true;
        else {
            const auto app_path = pending_app_open_path;
            const auto live_area = pending_app_open_live_area;

            pending_app_open_requested = false;
            pending_app_open_live_area = false;
            pending_app_open_wait_frame_displayed = false;
            pending_app_open_controls_please_wait = false;
            pending_app_open_path.clear();
            gui.vita_area.please_wait = false;

            pre_load_app(gui, emuenv, live_area, app_path, session);
        }
    } else if (pending_app_open_controls_please_wait) {
        pending_app_open_controls_please_wait = false;
        gui.vita_area.please_wait = false;
    }

    ImGui::End();
}

} // namespace gui
