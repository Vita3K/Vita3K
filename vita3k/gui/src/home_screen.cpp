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

#include "private.h"

#include <app/functions.h>
#include <compat/functions.h>
#include <config/state.h>
#include <ctrl/ctrl.h>
#include <dialog/state.h>
#include <display/state.h>
#include <gui/functions.h>
#include <io/state.h>
#include <kernel/state.h>

#include <util/log.h>
#include <util/safe_time.h>
#include <util/string_utils.h>
#include <util/vector_utils.h>

using namespace std::string_literals;

namespace gui {

void init_user_apps(GuiState &gui, EmuEnvState &emuenv) {
    gui.apps_background.clear();
    gui.app_selector.user_apps_icon.clear();
    gui.live_area_app_current_open = -1;
    gui.live_area_current_open_apps_list.clear();
    gui.live_area_contents.clear();
    gui.live_items.clear();
    if (gui.app_selector.icon_async_loader)
        gui.app_selector.icon_async_loader->quit = true;

    std::thread init_apps([&gui, &emuenv]() {
        auto apps_list_size = gui.app_selector.user_apps.size();
        gui.app_selector.user_apps.clear();
        get_user_apps_title(gui, emuenv);

        if (gui.app_selector.user_apps.empty())
            return false;

        gui.app_selector.is_app_list_sorted = false;
        init_last_time_apps(gui, emuenv);
        load_and_update_compat_user_apps(gui, emuenv);

        const auto new_apps_list_size = gui.app_selector.user_apps.size();
        init_apps_icon(gui, emuenv, gui.app_selector.user_apps);

        if (apps_list_size == new_apps_list_size)
            return false;

        std::string change_app_list = "new application(s) added";
        if (apps_list_size > new_apps_list_size) {
            change_app_list = "application(s) removed";
            apps_list_size -= new_apps_list_size;
        } else
            apps_list_size = new_apps_list_size - apps_list_size;

        LOG_INFO("{} {}", apps_list_size, change_app_list);

        return true;
    });
    init_apps.detach();
}

void init_last_time_apps(GuiState &gui, EmuEnvState &emuenv) {
    const auto last_time_apps = [&](std::vector<gui::App> &apps_list) {
        for (auto &app : apps_list) {
            const auto TIME_APP_INDEX = std::find_if(gui.time_apps[emuenv.io.user_id].begin(), gui.time_apps[emuenv.io.user_id].end(), [&](const TimeApp &t) {
                return t.app == app.path;
            });

            app.last_time = (TIME_APP_INDEX != gui.time_apps[emuenv.io.user_id].end()) ? TIME_APP_INDEX->last_time_used : 0;
        }
    };

    last_time_apps(gui.app_selector.sys_apps);
    last_time_apps(gui.app_selector.user_apps);
    gui.app_selector.is_app_list_sorted = false;
}

void load_and_update_compat_user_apps(GuiState &gui, EmuEnvState &emuenv) {
    std::thread load_and_update_compat_user_apps_thread([&gui, &emuenv]() {
        if (!gui.compat.compat_db_loaded && compat::load_app_compat_db(gui, emuenv))
            gui.compat.compat_db_loaded = true;
        else if (compat::update_app_compat_db(gui, emuenv) && !emuenv.io.user_id.empty() && gui.users[emuenv.io.user_id].sort_apps_type == COMPAT)
            gui.app_selector.is_app_list_sorted = false;
    });
    load_and_update_compat_user_apps_thread.detach();
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
        const auto &last_app = gui.live_area_current_open_apps_list.back() == emuenv.io.app_path ? gui.live_area_current_open_apps_list[gui.live_area_current_open_apps_list.size() - 2] : gui.live_area_current_open_apps_list.back();
        gui.live_area_contents.erase(last_app);
        gui.live_items.erase(last_app);
        gui.live_area_current_open_apps_list.erase(get_live_area_current_open_apps_list_index(gui, last_app));
    }
}

static std::map<std::string, uint64_t> last_time;

void open_live_area(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    update_live_area_current_open_apps_list(gui, emuenv, app_path);
    last_time["home"] = 0;
    init_live_area(gui, emuenv, app_path);
    gui.vita_area.home_screen = false;
    gui.vita_area.information_bar = true;
    gui.vita_area.live_area_screen = true;
}

void pre_load_app(GuiState &gui, EmuEnvState &emuenv, bool live_area, const std::string &app_path) {
    if (app_path == "NPXS10003") {
        update_last_time_app_used(gui, emuenv, app_path);
        open_path("https://Vita3k.org");
    } else {
        if (live_area)
            open_live_area(gui, emuenv, app_path);
        else
            pre_run_app(gui, emuenv, app_path);
    }
}

void pre_run_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    switch_bgm_state(true);
    const auto is_sys = app_path.starts_with("NPXS") && (app_path != "NPXS10007");
    if (!is_sys) {
        if (emuenv.io.app_path != app_path) {
            if (!emuenv.io.app_path.empty())
                gui.vita_area.app_close = true;
            else
                emuenv.io.app_path = app_path;
        } else {
            gui.vita_area.home_screen = false;
            gui.vita_area.live_area_screen = false;
            gui.vita_area.information_bar = false;
            app::switch_state(emuenv, false);
        }
    } else {
        emuenv.app_path = app_path;
        gui.vita_area.home_screen = false;
        gui.vita_area.live_area_screen = false;
        init_app_background(gui, emuenv, app_path);
        update_last_time_app_used(gui, emuenv, app_path);

        if (app_path == "NPXS10008") {
            init_trophy_collection(gui, emuenv);
            gui.vita_area.trophy_collection = true;
        } else if (app_path == "NPXS10015") {
            if (gui.vita_area.content_manager) {
                emuenv.app_path = "NPXS10026";
                update_time_app_used(gui, emuenv, "NPXS10026");
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
        update_time_app_used(gui, emuenv, "NPXS10026");
    } else if (gui.vita_area.manual) {
        gui.vita_area.manual = false;

        // Free manual textures from memory when manual is closed
        for (auto &manual : gui.manuals)
            manual = {};
        gui.manuals.clear();
    } else if (gui.vita_area.settings) {
        gui.vita_area.settings = false;
        update_time_app_used(gui, emuenv, "NPXS10015");
        if (emuenv.app_path == "NPXS10026") {
            pre_run_app(gui, emuenv, "NPXS10026");
            return;
        }
    } else {
        gui.vita_area.trophy_collection = false;
        update_time_app_used(gui, emuenv, "NPXS10008");
    }

    if ((gui::get_live_area_current_open_apps_list_index(gui, emuenv.app_path) != gui.live_area_current_open_apps_list.end()) && (gui.live_area_current_open_apps_list[gui.live_area_app_current_open] == emuenv.app_path)) {
        gui.vita_area.live_area_screen = true;
        gui.vita_area.information_bar = true;
    } else {
        if (emuenv.cfg.show_info_bar)
            gui.vita_area.information_bar = true;
        gui.vita_area.home_screen = true;
    }

    switch_bgm_state(false);
}

void close_and_run_new_app(EmuEnvState &emuenv, const std::string &app_path) {
    emuenv.kernel.exit_delete_all_threads();
    emuenv.load_app_path = app_path;
    emuenv.load_exec = true;
}

void draw_app_close(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 display_size(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

    const auto WINDOW_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);
    const auto BUTTON_SIZE = ImVec2(320.f * SCALE.x, 46.f * SCALE.y);

    auto &common = emuenv.common_dialog.lang.common;

    ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
    ImGui::Begin("##app_close", &gui.vita_area.app_close, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x + (display_size.x / 2.f) - (WINDOW_SIZE.x / 2.f), emuenv.logical_viewport_pos.y + (display_size.y / 2.f) - (WINDOW_SIZE.y / 2.f)), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
    ImGui::BeginChild("##app_close_child", WINDOW_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);

    const auto ICON_SIZE = ImVec2(64.f * SCALE.x, 64.f * SCALE.y);

    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
    ImGui::SetCursorPos(ImVec2(50.f * SCALE.x, 108.f * SCALE.y));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", gui.lang.game_data["app_close"].c_str());
    if (gui.app_selector.user_apps_icon.contains(emuenv.io.app_path)) {
        const auto ICON_POS_SCALE = ImVec2(50.f * SCALE.x, (WINDOW_SIZE.y / 2.f) - (ICON_SIZE.y / 2.f) - (10.f * SCALE.y));
        ImGui::SetCursorPos(ICON_POS_SCALE);
        const auto POS_MIN = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddImageRounded(get_app_icon(gui, emuenv.io.app_path)->second, POS_MIN, ImVec2(POS_MIN.x + ICON_SIZE.x, POS_MIN.y + ICON_SIZE.y), ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, ICON_SIZE.x, ImDrawFlags_RoundCornersAll);
    }
    ImGui::SetCursorPos(ImVec2(ICON_SIZE.x + (72.f * SCALE.x), (WINDOW_SIZE.y / 2.f) - ImGui::CalcTextSize(emuenv.current_app_title.c_str()).y + (4.f * SCALE.y)));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", emuenv.current_app_title.c_str());
    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * SCALE.x)), WINDOW_SIZE.y - BUTTON_SIZE.y - (24.0f * SCALE.y)));
    if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE))
        gui.vita_area.app_close = false;
    ImGui::SameLine(0, 20.f * SCALE.x);
    if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE)) {
        const auto &app_path = gui.vita_area.live_area_screen ? gui.live_area_current_open_apps_list[gui.live_area_app_current_open] : emuenv.app_path;
        close_and_run_new_app(emuenv, app_path);
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
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
        const auto app_index = std::find_if(app_region.begin(), app_region.end(), [&](const std::string &a) {
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

        gui.users[emuenv.io.user_id].sort_apps_type = type;
        gui.users[emuenv.io.user_id].sort_apps_state = sorted;

        save_user(gui, emuenv, emuenv.io.user_id);
    } else {
        sorted = gui.users[emuenv.io.user_id].sort_apps_state;
        gui.app_selector.is_app_list_sorted = true;
    }

    std::sort(gui.app_selector.user_apps.begin(), gui.app_selector.user_apps.end(), [&](const App &lhs, const App &rhs) {
        switch (type) {
        case APP_VER:
            switch (sorted) {
            case ASCENDANT:
                return lhs.app_ver < rhs.app_ver;
            case DESCENDANT:
                return lhs.app_ver > rhs.app_ver;
            }
            break;
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
                return lhs.compat < rhs.compat;
            case DESCENDANT:
                return lhs.compat > rhs.compat;
            }
            break;
        case LAST_TIME:
            switch (sorted) {
            case ASCENDANT:
                return lhs.last_time > rhs.last_time;
            case DESCENDANT:
                return lhs.last_time < rhs.last_time;
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

static std::string first_visible_app, current_selected_app;

static std::vector<std::string> apps_list_filtered;
void browse_home_apps_list(GuiState &gui, EmuEnvState &emuenv, const uint32_t button) {
    if (apps_list_filtered.empty())
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
    const int32_t current_selected_app_index = vector_utils::find_index(apps_list_filtered, current_selected_app);
    if (current_selected_app_index == -1) {
        current_selected_app = first_visible_app;
        return;
    }

    const auto prev_filtered_app = apps_list_filtered[std::max(current_selected_app_index - 1, 0)];
    const auto next_filtered_app = apps_list_filtered[std::min(current_selected_app_index + 1, apps_list_filtered_size)];

    const auto switch_to_first_live_area_open_app = [&gui]() {
        if (!gui.live_area_current_open_apps_list.empty()) {
            ++gui.live_area_app_current_open;
            gui.vita_area.home_screen = false;
            gui.vita_area.live_area_screen = true;
        }
    };

    switch (button) {
    case SCE_CTRL_UP:
        if (emuenv.cfg.apps_list_grid) {
            if (current_selected_app_index >= 4)
                current_selected_app = apps_list_filtered[current_selected_app_index - 4];
        } else
            current_selected_app = prev_filtered_app;
        break;
    case SCE_CTRL_RIGHT:
        if (emuenv.cfg.apps_list_grid) {
            if (((current_selected_app_index + 1) % 4 == 0) || (current_selected_app_index == apps_list_filtered_size))
                switch_to_first_live_area_open_app();
            else
                current_selected_app = next_filtered_app;
        } else
            switch_to_first_live_area_open_app();
        break;
    case SCE_CTRL_DOWN:
        if (emuenv.cfg.apps_list_grid) {
            if ((current_selected_app_index + 4) <= apps_list_filtered_size)
                current_selected_app = apps_list_filtered[current_selected_app_index + 4];
        } else
            current_selected_app = next_filtered_app;
        break;
    case SCE_CTRL_LEFT:
        if (emuenv.cfg.apps_list_grid && (current_selected_app_index % 4 != 0))
            current_selected_app = prev_filtered_app;
        break;
    case SCE_CTRL_R1:
        gui.live_area_app_current_open = std::min(gui.live_area_app_current_open + 1, static_cast<int32_t>(gui.live_area_current_open_apps_list.size() - 1));
        gui.vita_area.live_area_screen = gui.live_area_app_current_open >= 0;
        gui.vita_area.home_screen = !gui.vita_area.live_area_screen;
        break;
    case SCE_CTRL_CIRCLE:
        if (emuenv.cfg.sys_button == 0)
            pre_load_app(gui, emuenv, emuenv.cfg.show_live_area_screen, current_selected_app);
        break;
    case SCE_CTRL_CROSS:
        if (emuenv.cfg.sys_button == 1)
            pre_load_app(gui, emuenv, emuenv.cfg.show_live_area_screen, current_selected_app);
        break;
    default: break;
    }
}

static std::string selected_app;
void select_app(GuiState &gui, const std::string &title_id) {
    // Find the app in the user apps list
    auto app_it = std::find_if(gui.app_selector.user_apps.begin(), gui.app_selector.user_apps.end(), [&](const App &app) {
        return app.title_id == title_id;
    });

    // Check if the app was found
    if (app_it != gui.app_selector.user_apps.end()) {
        // Set the selected app
        current_selected_app = selected_app = title_id;
        gui.is_nav_button = true;
    } else
        LOG_ERROR("App with title id {} not found", title_id);
}

void draw_home_screen(GuiState &gui, EmuEnvState &emuenv) {
    constexpr ImU32 ARROW_COLOR = 0xFFFFFFFF; // White
    static int scroll_type;

    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 VIEWPORT_RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 VIEWPORT_SCALE(VIEWPORT_RES_SCALE.x * emuenv.manual_dpi_scale, VIEWPORT_RES_SCALE.y * emuenv.manual_dpi_scale);
    const auto INFORMATION_BAR_HEIGHT = 32.f * VIEWPORT_SCALE.y;

    // Clear apps list filtered
    apps_list_filtered.clear();

    ImGui::SetNextWindowPos(ImVec2(VIEWPORT_POS.x, VIEWPORT_POS.y + INFORMATION_BAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INFORMATION_BAR_HEIGHT), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::SetNextWindowBgAlpha(emuenv.cfg.background_alpha);
    auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;
    if (gui.is_nav_button)
        flags |= ImGuiWindowFlags_NoMouseInputs;
    ImGui::Begin("##home_screen", &gui.vita_area.home_screen, flags);
    if (!emuenv.display.imgui_render || ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
        gui.vita_area.information_bar = true;

    const auto config_dialog = gui.configuration_menu.custom_settings_dialog || gui.configuration_menu.settings_dialog || gui.controls_menu.controls_dialog;
    const auto install_dialog = gui.file_menu.archive_install_dialog || gui.file_menu.firmware_install_dialog || gui.file_menu.pkg_install_dialog;
    if (!config_dialog && !install_dialog && !gui.vita_area.app_close && !gui.vita_area.app_information && !gui.help_menu.about_dialog && !gui.help_menu.welcome_dialog && !gui.is_nav_button) {
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
                switch_bgm_state(true);
            }
        }
    }

    if (!gui.vita_area.start_screen && !gui.vita_area.live_area_screen && (!gui.theme_backgrounds.empty() || !gui.user_backgrounds.empty())) {
        if (last_time["home"] == 0)
            last_time["home"] = current_time();

        while (last_time["home"] + emuenv.cfg.delay_background < current_time()) {
            last_time["home"] += emuenv.cfg.delay_background;

            if (gui.users[emuenv.io.user_id].use_theme_bg)
                gui.current_theme_bg = (gui.current_theme_bg + 1) % gui.theme_backgrounds.size();
            else if (gui.user_backgrounds.size() > 1)
                gui.current_user_bg = (gui.current_user_bg + 1) % gui.user_backgrounds.size();
        }
    }

    draw_background(gui, emuenv);

    // Size of the icon depending view mode
    const ImVec2 ICON_SIZE(emuenv.cfg.apps_list_grid ? ImVec2(128.f * VIEWPORT_SCALE.x, 128.f * VIEWPORT_SCALE.y) : ImVec2(emuenv.cfg.icon_size * VIEWPORT_SCALE.x, emuenv.cfg.icon_size * VIEWPORT_SCALE.x));

    // Size of column padding
    const float column_padding_size = 20.f * VIEWPORT_SCALE.x;

    // Size of the icon part
    const float column_icon_size = ICON_SIZE.x + column_padding_size + (5.f * VIEWPORT_SCALE.x);

    // Size of the compatibility part
    const auto compat_radius = 12.f * VIEWPORT_SCALE.x;
    const auto full_compat_radius = 3.f * VIEWPORT_SCALE.x + compat_radius;

    auto &lang = gui.lang.home_screen;

    ImGui::SetWindowFontScale(0.9f * VIEWPORT_RES_SCALE.x);

    // Sort Apps list when is not sorted
    if (!gui.app_selector.is_app_list_sorted)
        sort_app_list(gui, emuenv, gui.users[emuenv.io.user_id].sort_apps_type);

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
    ImGui::SetCursorPosY(4.f * VIEWPORT_SCALE.y);
    if (!emuenv.cfg.apps_list_grid) {
        ImGui::Columns(7);
        ImGui::SetColumnWidth(0, column_icon_size);
        if (ImGui::Button(lang["filter"].c_str()))
            ImGui::OpenPopup("app_filter");
        ImGui::NextColumn();
        ImGui::SetColumnWidth(1, compat_size);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - (column_padding_size / 4.f));
        if (ImGui::Button(compat_label.c_str()))
            sort_app_list(gui, emuenv, COMPAT);
        ImGui::NextColumn();
        ImGui::SetColumnWidth(2, title_id_size);
        if (ImGui::Button(title_id_label.c_str()))
            sort_app_list(gui, emuenv, TITLE_ID);
        ImGui::NextColumn();
        ImGui::SetColumnWidth(3, app_ver_size);
        if (ImGui::Button(app_ver_label.c_str()))
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
        if (ImGui::Button(lang["filter"].c_str()))
            ImGui::OpenPopup("app_filter");
        ImGui::NextColumn();
        if (ImGui::Button(lang["sort_app"].c_str()))
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
            ImGui::SetWindowFontScale(1.1f * VIEWPORT_RES_SCALE.x);
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
            ImGui::SetWindowFontScale(1.1f * VIEWPORT_RES_SCALE.x);
            if (ImGui::MenuItem(lang["commercial"].c_str(), nullptr, app_region == COMMERCIAL))
                app_region = COMMERCIAL;
            if (ImGui::MenuItem(lang["homebrew"].c_str(), nullptr, app_region == HOMEBREW))
                app_region = HOMEBREW;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(lang["by_compatibility_state"].c_str())) {
            ImGui::SetWindowFontScale(1.1f * VIEWPORT_RES_SCALE.x);
            if (ImGui::MenuItem(lang["all"].c_str(), nullptr, app_compat_state == ALL_COMPAT_STATE))
                app_compat_state = ALL_COMPAT_STATE;
            auto &lang_compat = gui.lang.compatibility.states;
            for (int32_t i = compat::UNKNOWN; i <= compat::PLAYABLE; i++) {
                const auto compat_state = static_cast<compat::CompatibilityState>(i);
                ImGui::PushStyleColor(ImGuiCol_Text, gui.compat.compat_color[compat_state]);
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
    if (ImGui::Button(lang["refresh"].c_str()))
        init_user_apps(gui, emuenv);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_SEARCH_BAR_TEXT);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, GUI_COLOR_SEARCH_BAR_BG);
    gui.app_search_bar.Draw("##app_search_bar", search_bar_size);
    ImGui::PopStyleColor(2);
    ImGui::NextColumn();
    ImGui::Columns(1);
    ImGui::Separator();
    const auto MARGIN_HEIGHT = INFORMATION_BAR_HEIGHT + (34.f * VIEWPORT_SCALE.y);
    const auto POS_APP_LIST = ImVec2(VIEWPORT_POS.x + (60.f * VIEWPORT_SCALE.x), VIEWPORT_POS.y + MARGIN_HEIGHT);
    const auto SIZE_APP_LIST = ImVec2(emuenv.cfg.apps_list_grid ? 840.f * VIEWPORT_SCALE.x : 900.f * VIEWPORT_SCALE.x, VIEWPORT_SIZE.y - MARGIN_HEIGHT);
    auto child_flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
    if (gui.is_nav_button)
        child_flags |= ImGuiWindowFlags_NoMouseInputs;
    ImGui::SetNextWindowPos(emuenv.cfg.apps_list_grid ? POS_APP_LIST : ImVec2(emuenv.logical_viewport_pos.x, VIEWPORT_POS.y + MARGIN_HEIGHT), ImGuiCond_Always);
    ImGui::BeginChild("##apps_list", SIZE_APP_LIST, ImGuiChildFlags_None, child_flags);

    // Get Scroll Pos
    float current_scroll_pos = ImGui::GetScrollY();
    float max_scroll_pos = ImGui::GetScrollMaxY();

    // Set Scroll Pos
    if (scroll_type != 0) {
        const float scroll_move = scroll_type == -1 ? SIZE_APP_LIST.y : -SIZE_APP_LIST.y;
        ImGui::SetScrollY(ImGui::GetScrollY() + scroll_move);
        scroll_type = 0;
    }

    const auto GRID_COLUMN_SIZE = ICON_SIZE.x + (80.f * VIEWPORT_SCALE.x);
    const ImVec2 list_selectable_size(0.f, ICON_SIZE.y + (10.f * VIEWPORT_SCALE.y));
    const ImVec2 SELECTABLE_APP_SIZE = emuenv.cfg.apps_list_grid ? ICON_SIZE : list_selectable_size;

    if (!emuenv.cfg.apps_list_grid) {
        ImGui::Columns(7, nullptr, true);
        ImGui::SetColumnWidth(0, column_icon_size);
        ImGui::SetColumnWidth(1, compat_size);
        ImGui::SetColumnWidth(2, title_id_size);
        ImGui::SetColumnWidth(3, app_ver_size);
        ImGui::SetColumnWidth(4, category_size);
        ImGui::SetColumnWidth(5, last_time_size);
    } else {
        ImGui::Columns(4, nullptr, false);
        ImGui::SetColumnWidth(0, GRID_COLUMN_SIZE);
        ImGui::SetColumnWidth(1, GRID_COLUMN_SIZE);
        ImGui::SetColumnWidth(2, GRID_COLUMN_SIZE);
        ImGui::SetColumnWidth(3, GRID_COLUMN_SIZE);
    }

    ImGui::SetWindowFontScale(1.1f);
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);

    std::vector<std::string> visible_apps{};

    const auto display_app = [&](const std::vector<gui::App> &apps_list, std::map<std::string, ImGui_Texture> &apps_icon) {
        for (const auto &app : apps_list) {
            const auto is_sys = app.path.starts_with("NPXS") && (app.path != "NPXS10007");

            if (!is_sys) {
                // Filter app by region and type
                if (app_filter(app.title_id))
                    continue;

                // Filter commercial app by compatibility
                if ((app_region != HOMEBREW) && (app_compat_state != ALL_COMPAT_STATE) && (app_compat_state != app.compat))
                    continue;
            }

            // Search app by title or title id
            if (!gui.app_search_bar.PassFilter(app.title.c_str()) && !gui.app_search_bar.PassFilter(app.title_id.c_str()))
                continue;

            const auto POS_ICON = ImGui::GetCursorPos();
            const auto GRID_INIT_POS = POS_ICON.x + (GRID_COLUMN_SIZE / 2.f) - (10.f * VIEWPORT_SCALE.x);

            const auto GRID_ICON_POS = GRID_INIT_POS - (ICON_SIZE.x / 2.f);
            ImGui::PushID(app.path.c_str());

            if (emuenv.cfg.apps_list_grid)
                ImGui::SetCursorPosX(GRID_ICON_POS);

            // Get the current app index off the apps list.
            apps_list_filtered.push_back(app.path);

            // Check if the current app is selected.
            const auto is_current_app_selected = gui.is_nav_button && (current_selected_app == app.path);
            const auto icon_flags = emuenv.cfg.apps_list_grid ? ImGuiSelectableFlags_None : ImGuiSelectableFlags_SpanAllColumns;
            if (ImGui::Selectable("##icon", is_current_app_selected || (selected_app == app.path), icon_flags, SELECTABLE_APP_SIZE)) {
                selected_app.clear();
                pre_load_app(gui, emuenv, emuenv.cfg.show_live_area_screen, app.path);
            }

            if (!gui.configuration_menu.custom_settings_dialog && (ImGui::IsItemHovered() || is_current_app_selected))
                emuenv.app_path = app.path;
            if (emuenv.app_path == app.path)
                draw_app_context_menu(gui, emuenv, app.path);
            const auto STITLE_SIZE = ImGui::CalcTextSize(app.stitle.c_str(), 0, false, ICON_SIZE.x + (42.f * VIEWPORT_SCALE.x));
            const auto item_rect_max = ImGui::GetItemRectMax().y;

            // Get the min and full item rect max, depending on the view mode.
            const auto MIN_ITEM_RECT_MAX = emuenv.cfg.apps_list_grid ? item_rect_max : item_rect_max - (5.f * VIEWPORT_SCALE.y);
            const auto FULL_ITEM_RECT_MAX = emuenv.cfg.apps_list_grid ? item_rect_max + ImGui::GetStyle().ItemSpacing.y + STITLE_SIZE.y : item_rect_max;

            auto item_rect_min = item_rect_max - SELECTABLE_APP_SIZE.y;
            const auto MAX_LIST_POS = POS_APP_LIST.y + SIZE_APP_LIST.y;

            // Determine if the element is within the visible area of the window.
            const auto element_is_within_visible_area = (MIN_ITEM_RECT_MAX >= POS_APP_LIST.y) && (item_rect_min <= MAX_LIST_POS);

            // When the app is selected.
            if (is_current_app_selected) {
                // Scroll to the app position when it is not visible.
                if (item_rect_min < POS_APP_LIST.y)
                    ImGui::SetScrollHereY(0.f);
                else if (FULL_ITEM_RECT_MAX > MAX_LIST_POS) {
                    if (emuenv.cfg.apps_list_grid)
                        ImGui::SetScrollY(ImGui::GetScrollY() + (FULL_ITEM_RECT_MAX - MAX_LIST_POS));
                    else
                        ImGui::SetScrollHereY(1.f);
                }
            }

            ImGui::SetCursorPos(POS_ICON);

            // Draw the app icons and custom config button only when they are within the visible area.
            if (element_is_within_visible_area) {
                // Add the current app index to the visible apps list.
                visible_apps.push_back(app.path);

                // Set the current selected app index to the current app index when the app is hovered.
                if (!gui.is_nav_button && ImGui::IsItemHovered())
                    current_selected_app = app.path;

                // Draw the app icon
                if (apps_icon.contains(app.path)) {
                    if (emuenv.cfg.apps_list_grid)
                        ImGui::SetCursorPosX(GRID_ICON_POS);
                    else
                        ImGui::SetCursorPos(ImVec2(POS_ICON.x + (5.f * VIEWPORT_SCALE.x), POS_ICON.y + (5.f * VIEWPORT_SCALE.y)));
                    const auto POS_MIN = ImGui::GetCursorScreenPos();
                    const ImVec2 POS_MAX(POS_MIN.x + ICON_SIZE.x, POS_MIN.y + ICON_SIZE.y);
                    ImGui::GetWindowDrawList()->AddImageRounded(apps_icon[app.path], POS_MIN, POS_MAX, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, ICON_SIZE.x * VIEWPORT_SCALE.x, ImDrawFlags_RoundCornersAll);
                }

                // Draw the custom config button
                const auto IS_CUSTOM_CONFIG = fs::exists(emuenv.config_path / "config" / fmt::format("config_{}.xml", app.path));
                if (IS_CUSTOM_CONFIG) {
                    if (emuenv.cfg.apps_list_grid)
                        ImGui::SetCursorPosX(GRID_ICON_POS);
                    ImGui::SetCursorPosY(POS_ICON.y + ICON_SIZE.y - ImGui::GetFontSize() - (7.8f * emuenv.manual_dpi_scale));
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                    ImGui::Button("CC", ImVec2(40.f * VIEWPORT_SCALE.x, 0.f));
                    ImGui::PopStyleColor();
                }
            } else if (!gui.is_nav_button && (current_selected_app == app.path)) {
                // When the app is selected but not visible, reset the current selected app index.
                current_selected_app.clear();
            }

            if (!emuenv.cfg.apps_list_grid)
                ImGui::NextColumn();

            // Draw the compatibility badge for commercial apps when they are within the visible area.
            if (element_is_within_visible_area && (app.title_id.starts_with("PCS") || (app.title_id == "NPXS10007"))) {
                const auto compat_state = (gui.compat.compat_db_loaded && gui.compat.app_compat_db.contains(app.title_id)) ? gui.compat.app_compat_db[app.title_id].state : compat::UNKNOWN;
                const auto &compat_state_vec4 = gui.compat.compat_color[compat_state];
                const ImU32 compat_state_color = IM_COL32((int)(compat_state_vec4.x * 255.0f), (int)(compat_state_vec4.y * 255.0f), (int)(compat_state_vec4.z * 255.0f), (int)(compat_state_vec4.w * 255.0f));
                const auto current_pos = ImGui::GetCursorPos();
                const auto grid_compat_padding = 4.f * VIEWPORT_SCALE.x;
                const auto compat_state_pos = emuenv.cfg.apps_list_grid ? ImVec2(GRID_ICON_POS + full_compat_radius + grid_compat_padding, POS_ICON.y + full_compat_radius + grid_compat_padding) : ImVec2(current_pos.x + ((ImGui::GetColumnWidth() - column_padding_size) / 2.f), current_pos.y + (ICON_SIZE.y / 2.f));
                ImGui::SetCursorPos(compat_state_pos);
                ImGui::GetWindowDrawList()->AddCircleFilled(ImGui::GetCursorScreenPos(), full_compat_radius, IM_COL32(0, 0, 0, 255));
                ImGui::GetWindowDrawList()->AddCircleFilled(ImGui::GetCursorScreenPos(), compat_radius, compat_state_color);
            }

            if (!emuenv.cfg.apps_list_grid) {
                ImGui::NextColumn();
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_Text, !gui.theme_backgrounds_font_color.empty() && gui.users[emuenv.io.user_id].use_theme_bg ? gui.theme_backgrounds_font_color[gui.current_theme_bg] : GUI_COLOR_TEXT);
                ImGui::Selectable(app.title_id.c_str(), false, ImGuiSelectableFlags_None, list_selectable_size);
                ImGui::NextColumn();
                ImGui::Selectable(app.app_ver.c_str(), false, ImGuiSelectableFlags_None, list_selectable_size);
                ImGui::NextColumn();
                ImGui::Selectable(app.category.c_str(), false, ImGuiSelectableFlags_None, list_selectable_size);
                ImGui::NextColumn();
                if (app.last_time) {
                    tm date_tm = {};
                    SAFE_LOCALTIME(&app.last_time, &date_tm);
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
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();
            } else {
                ImGui::SetCursorPos(ImVec2(GRID_INIT_POS - (STITLE_SIZE.x / 2.f), POS_ICON.y + SELECTABLE_APP_SIZE.y + ImGui::GetStyle().ItemSpacing.y));
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (GRID_COLUMN_SIZE - (38.f * VIEWPORT_SCALE.x)));
                ImGui::TextColored(!gui.theme_backgrounds_font_color.empty() && gui.users[emuenv.io.user_id].use_theme_bg ? gui.theme_backgrounds_font_color[gui.current_theme_bg] : GUI_COLOR_TEXT, "%s", app.stitle.c_str());
                ImGui::PopTextWrapPos();
            }
            ImGui::NextColumn();
            ImGui::PopID();
        }
    };

    // Draw System Applications
    if (emuenv.cfg.display_system_apps)
        display_app(gui.app_selector.sys_apps, gui.app_selector.sys_apps_icon);

    // Draw User Applications
    display_app(gui.app_selector.user_apps, gui.app_selector.user_apps_icon);

    ImGui::PopStyleColor();
    ImGui::Columns(1);
    ImGui::SetWindowFontScale(1.f);
    ImGui::ScrollWhenDragging();
    ImGui::EndChild();

    // When visible apps list is not empty, set first visible app
    if (!visible_apps.empty())
        first_visible_app = visible_apps.front();

    const auto SELECTABLE_SIZE = ImVec2(50.f * VIEWPORT_SCALE.x, 60.f * VIEWPORT_SCALE.y);

    const auto window_draw_list = ImGui::GetWindowDrawList();

    // Set arrow position of width
    const auto ARROW_WIDTH_POS = VIEWPORT_SIZE.x - (30.f * VIEWPORT_SCALE.x);
    const auto ARROW_DRAW_WIDTH_POS = VIEWPORT_POS.x + ARROW_WIDTH_POS;
    const auto ARROW_SELECT_WIDTH_POS = ARROW_WIDTH_POS - (SELECTABLE_SIZE.x / 2.f);

    // Set arrow position of up
    if (current_scroll_pos > 0) {
        const auto ARROW_UP_HEIGHT_POS = 110.f * VIEWPORT_SCALE.y;
        const auto ARROW_UP_CENTER = ImVec2(ARROW_DRAW_WIDTH_POS, VIEWPORT_POS.y + ARROW_UP_HEIGHT_POS);
        window_draw_list->AddTriangleFilled(
            ImVec2(ARROW_UP_CENTER.x - (20.f * VIEWPORT_SCALE.x), ARROW_UP_CENTER.y + (16.f * VIEWPORT_SCALE.y)),
            ImVec2(ARROW_UP_CENTER.x, ARROW_UP_CENTER.y - (16.f * VIEWPORT_SCALE.y)),
            ImVec2(ARROW_UP_CENTER.x + (20.f * VIEWPORT_SCALE.x), ARROW_UP_CENTER.y + (16.f * VIEWPORT_SCALE.y)), ARROW_COLOR);
        ImGui::SetCursorPos(ImVec2(ARROW_SELECT_WIDTH_POS, ARROW_UP_HEIGHT_POS - SELECTABLE_SIZE.y));
        if (ImGui::Selectable("##upp", false, ImGuiSelectableFlags_None, SELECTABLE_SIZE)
            || (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_leftstick_up)))) {
            gui.is_nav_button = false;
            scroll_type = 1;
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
        if (!gui.vita_area.app_close && (ImGui::Selectable("##right", false, ImGuiSelectableFlags_None, SELECTABLE_SIZE) || (!ImGui::GetIO().WantTextInput && ImGui::IsKeyReleased(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_r1))))) {
            last_time["start"] = 0;
            ++gui.live_area_app_current_open;
            gui.vita_area.home_screen = false;
            gui.vita_area.live_area_screen = true;
        }
    }

    // Set arrow position of down
    if (current_scroll_pos < max_scroll_pos) {
        const auto ARROW_DOWN_HEIGHT_POS = VIEWPORT_SIZE.y - (30.f * VIEWPORT_SCALE.y);
        const ImVec2 ARROW_DOWN_CENTER(ARROW_DRAW_WIDTH_POS, VIEWPORT_POS.y + ARROW_DOWN_HEIGHT_POS);
        window_draw_list->AddTriangleFilled(
            ImVec2(ARROW_DOWN_CENTER.x + (20.f * VIEWPORT_SCALE.x), ARROW_DOWN_CENTER.y - (16.f * VIEWPORT_SCALE.y)),
            ImVec2(ARROW_DOWN_CENTER.x, ARROW_DOWN_CENTER.y + (16.f * VIEWPORT_SCALE.y)),
            ImVec2(ARROW_DOWN_CENTER.x - (20.f * VIEWPORT_SCALE.x), ARROW_DOWN_CENTER.y - (16.f * VIEWPORT_SCALE.y)), ARROW_COLOR);
        ImGui::SetCursorPos(ImVec2(ARROW_SELECT_WIDTH_POS, ARROW_DOWN_HEIGHT_POS - SELECTABLE_SIZE.y));
        if (ImGui::Selectable("##down", false, ImGuiSelectableFlags_None, SELECTABLE_SIZE)
            || (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_leftstick_down)))) {
            gui.is_nav_button = false;
            scroll_type = -1;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace gui
