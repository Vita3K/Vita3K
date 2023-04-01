// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <config/functions.h>

#include <compat/functions.h>
#include <config/state.h>
#include <display/state.h>
#include <gui/functions.h>
#include <io/state.h>
#include <kernel/state.h>

#include <io/VitaIoDevice.h>

#include <util/log.h>
#include <util/safe_time.h>
#include <util/string_utils.h>

using namespace std::string_literals;

namespace gui {

void init_user_apps(GuiState &gui, EmuEnvState &emuenv) {
    gui.apps_background.clear();
    gui.apps_list_opened.clear();
    gui.app_selector.user_apps_icon.clear();
    gui.current_app_selected = -1;
    gui.live_area_contents.clear();
    gui.live_items.clear();
    if (gui.app_selector.icon_async_loader)
        gui.app_selector.icon_async_loader->quit = true;

    std::thread init_apps([&gui, &emuenv]() {
        auto app_list_size = gui.app_selector.user_apps.size();
        gui.app_selector.user_apps.clear();
        get_user_apps_title(gui, emuenv);

        if (gui.app_selector.user_apps.empty())
            return false;

        gui.app_selector.is_app_list_sorted = false;
        init_last_time_apps(gui, emuenv);
        load_and_update_compat_user_apps(gui, emuenv);

        init_apps_icon(gui, emuenv, gui.app_selector.user_apps);

        if (app_list_size == gui.app_selector.user_apps.size())
            return false;

        std::string change_app_list = "new application(s) added";
        if (app_list_size > gui.app_selector.user_apps.size()) {
            change_app_list = "application(s) removed";
            app_list_size -= gui.app_selector.user_apps.size();
        } else
            app_list_size = gui.app_selector.user_apps.size() - app_list_size;

        LOG_INFO("{} {}", app_list_size, change_app_list);

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
        if (!gui.compat.compat_db_loaded)
            gui.compat.compat_db_loaded = compat::load_compat_app_db(gui, emuenv);
        if (compat::update_compat_app_db(gui, emuenv) && gui.users[emuenv.io.user_id].sort_apps_type == COMPAT)
            gui.app_selector.is_app_list_sorted = false;
    });
    load_and_update_compat_user_apps_thread.detach();
}

std::vector<std::string>::iterator get_app_open_list_index(GuiState &gui, const std::string app_path) {
    return std::find(gui.apps_list_opened.begin(), gui.apps_list_opened.end(), app_path);
}

void update_apps_list_opened(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    if ((get_app_open_list_index(gui, app_path) != gui.apps_list_opened.end()) && (gui.apps_list_opened.front() != app_path))
        gui.apps_list_opened.erase(get_app_open_list_index(gui, app_path));
    if ((get_app_open_list_index(gui, app_path) == gui.apps_list_opened.end()))
        gui.apps_list_opened.insert(gui.apps_list_opened.begin(), app_path);
    gui.current_app_selected = 0;
    if (gui.apps_list_opened.size() > 6) {
        const auto last_app = gui.apps_list_opened.back() == emuenv.io.app_path ? gui.apps_list_opened[gui.apps_list_opened.size() - 2] : gui.apps_list_opened.back();
        gui.live_area_contents.erase(last_app);
        gui.live_items.erase(last_app);
        gui.apps_list_opened.erase(get_app_open_list_index(gui, last_app));
    }
}

static std::map<std::string, uint64_t> last_time;

void open_live_area(GuiState &gui, EmuEnvState &emuenv, const std::string app_path) {
    update_apps_list_opened(gui, emuenv, app_path);
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
    if (app_path.find("NPXS") == std::string::npos) {
        if (emuenv.io.app_path != app_path) {
            if (!emuenv.io.app_path.empty())
                gui.vita_area.app_close = true;
            else {
                gui.vita_area.home_screen = false;
                gui.vita_area.live_area_screen = false;
                emuenv.io.app_path = app_path;
            }
        } else {
            gui.vita_area.home_screen = false;
            gui.vita_area.live_area_screen = false;
            gui.vita_area.information_bar = false;
        }
    } else {
        gui.vita_area.home_screen = false;
        gui.vita_area.live_area_screen = false;
        init_app_background(gui, emuenv, app_path);
        update_last_time_app_used(gui, emuenv, app_path);

        if (app_path == "NPXS10008") {
            init_trophy_collection(gui, emuenv);
            gui.vita_area.trophy_collection = true;
        } else if (app_path == "NPXS10015")
            gui.vita_area.settings = true;
        else {
            init_content_manager(gui, emuenv);
            gui.vita_area.content_manager = true;
        }
    }
}

void draw_app_close(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 display_size(emuenv.viewport_size.x, emuenv.viewport_size.y);
    const auto RES_SCALE = ImVec2(display_size.x / emuenv.res_width_dpi_scale, display_size.y / emuenv.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.dpi_scale, RES_SCALE.y * emuenv.dpi_scale);

    const auto WINDOW_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);
    const auto BUTTON_SIZE = ImVec2(320.f * SCALE.x, 46.f * SCALE.y);

    auto common = emuenv.common_dialog.lang.common;

    ImGui::SetNextWindowPos(ImVec2(emuenv.viewport_pos.x, emuenv.viewport_pos.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
    ImGui::Begin("##app_close", &gui.vita_area.app_close, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextWindowPos(ImVec2(emuenv.viewport_pos.x + (display_size.x / 2.f) - (WINDOW_SIZE.x / 2.f), emuenv.viewport_pos.y + (display_size.y / 2.f) - (WINDOW_SIZE.y / 2.f)), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
    ImGui::BeginChild("##app_close_child", WINDOW_SIZE, true, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);

    const auto ICON_SIZE = ImVec2(64.f * SCALE.x, 64.f * SCALE.y);

    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
    ImGui::SetCursorPos(ImVec2(50.f * SCALE.x, 108.f * SCALE.y));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", gui.lang.game_data["app_close"].c_str());
    if (gui.app_selector.user_apps_icon.contains(emuenv.io.app_path)) {
        const auto ICON_POS_SCALE = ImVec2(50.f * SCALE.x, (WINDOW_SIZE.y / 2.f) - (ICON_SIZE.y / 2.f) - (10.f * SCALE.y));
        const auto ICON_SIZE_SCALE = ImVec2(ICON_POS_SCALE.x + ICON_SIZE.x, ICON_POS_SCALE.y + ICON_SIZE.y);
        ImGui::SetCursorPos(ICON_POS_SCALE);
        const auto POS_MIN = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddImageRounded(get_app_icon(gui, emuenv.io.app_path)->second, POS_MIN, ImVec2(POS_MIN.x + ICON_SIZE.x, POS_MIN.y + ICON_SIZE.y), ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, ICON_SIZE.x, ImDrawCornerFlags_All);
    }
    ImGui::SetCursorPos(ImVec2(ICON_SIZE.x + (72.f * SCALE.x), (WINDOW_SIZE.y / 2.f) - ImGui::CalcTextSize(emuenv.current_app_title.c_str()).y + (4.f * SCALE.y)));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", emuenv.current_app_title.c_str());
    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * SCALE.x)), WINDOW_SIZE.y - BUTTON_SIZE.y - (24.0f * SCALE.y)));
    if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(emuenv.cfg.keyboard_button_circle))
        gui.vita_area.app_close = false;
    ImGui::SameLine(0, 20.f * SCALE.x);
    if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(emuenv.cfg.keyboard_button_cross)) {
        const auto app_path = gui.vita_area.live_area_screen ? gui.apps_list_opened[gui.current_app_selected] : emuenv.app_path;
        update_time_app_used(gui, emuenv, emuenv.io.app_path);
        emuenv.kernel.exit_delete_all_threads();
        emuenv.load_app_path = app_path;
        emuenv.load_exec = true;
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
}

inline uint64_t current_time() {
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
        if (!filter_app({ "PCS" }))
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
        case CATEGORY:
            switch (sorted) {
            case ASCENDANT:
                return lhs.category < rhs.category;
            case DESCENDANT:
                return lhs.category > rhs.category;
            }
        case COMPAT:
            switch (sorted) {
            case ASCENDANT:
                return lhs.compat < rhs.compat;
            case DESCENDANT:
                return lhs.compat > rhs.compat;
            }
        case LAST_TIME:
            switch (sorted) {
            case ASCENDANT:
                return lhs.last_time > rhs.last_time;
            case DESCENDANT:
                return lhs.last_time < rhs.last_time;
            }
        case TITLE:
            switch (sorted) {
            case ASCENDANT:
                return string_utils::toupper(lhs.title) < string_utils::toupper(rhs.title);
            case DESCENDANT:
                return string_utils::toupper(lhs.title) > string_utils::toupper(rhs.title);
            }
        case TITLE_ID:
            switch (sorted) {
            case ASCENDANT:
                return lhs.title_id < rhs.title_id;
            case DESCENDANT:
                return lhs.title_id > rhs.title_id;
            }
        }
        return false;
    });
}

static std::string get_label_name(GuiState &gui, const SortType &type) {
    std::string label;
    auto lang = gui.lang.home_screen;
    switch (type) {
    case APP_VER: label = lang["ver"]; break;
    case CATEGORY: label = lang["cat"]; break;
    case COMPAT: label = lang["comp"]; break;
    case LAST_TIME: label = lang["last_time"]; break;
    case TITLE: label = gui.lang.app_context["name"]; break;
    case TITLE_ID: label = gui.lang.app_context["title_id"]; break;
    }

    switch (gui.app_selector.app_list_sorted[type]) {
    case ASCENDANT: label += " >"; break;
    case DESCENDANT: label += " <"; break;
    default: break;
    }

    return label;
}

static const ImU32 ARROW_COLOR = 4294967295; // White
static float scroll_type, current_scroll_pos, max_scroll_pos;

void draw_home_screen(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / emuenv.res_width_dpi_scale, display_size.y / emuenv.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.dpi_scale, RES_SCALE.y * emuenv.dpi_scale);
    const auto INFORMATION_BAR_HEIGHT = 32.f * SCALE.y;
    const auto MENUBAR_BG_HEIGHT = (!emuenv.cfg.show_info_bar && emuenv.display.imgui_render) || !gui.vita_area.information_bar ? 22.f * SCALE.x : INFORMATION_BAR_HEIGHT;

    const auto is_background = (gui.users[emuenv.io.user_id].use_theme_bg && !gui.theme_backgrounds.empty()) || !gui.user_backgrounds.empty();

    ImGui::SetNextWindowPos(ImVec2(0, INFORMATION_BAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y - INFORMATION_BAR_HEIGHT), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::SetNextWindowBgAlpha(is_background ? emuenv.cfg.background_alpha : 0.f);
    ImGui::Begin("##home_screen", &gui.vita_area.home_screen, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);
    if (!emuenv.display.imgui_render || ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
        gui.vita_area.information_bar = true;

    const auto config_dialog = gui.configuration_menu.custom_settings_dialog || gui.configuration_menu.settings_dialog || gui.controls_menu.controls_dialog;
    const auto install_dialog = gui.file_menu.archive_install_dialog || gui.file_menu.firmware_install_dialog || gui.file_menu.pkg_install_dialog;
    if (!config_dialog && !install_dialog && !gui.vita_area.app_close && !gui.vita_area.app_information && !gui.help_menu.about_dialog && !gui.help_menu.welcome_dialog) {
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
            }
        }
    }

    if (!gui.vita_area.start_screen && !gui.vita_area.live_area_screen && (!gui.theme_backgrounds.empty() || !gui.user_backgrounds.empty())) {
        if (last_time["home"] == 0)
            last_time["home"] = current_time();

        while (last_time["home"] + emuenv.cfg.delay_background < current_time()) {
            last_time["home"] += emuenv.cfg.delay_background;

            if (gui.users[emuenv.io.user_id].use_theme_bg) {
                if (gui.current_theme_bg < uint64_t(gui.theme_backgrounds.size() - 1))
                    ++gui.current_theme_bg;
                else
                    gui.current_theme_bg = 0;
            } else {
                if (gui.user_backgrounds.size() > 1) {
                    if (gui.current_user_bg < uint64_t(gui.user_backgrounds.size() - 1))
                        ++gui.current_user_bg;
                    else
                        gui.current_user_bg = 0;
                }
            }
        }
    }

    if (is_background)
        ImGui::GetBackgroundDrawList()->AddImage((gui.users[emuenv.io.user_id].use_theme_bg && !gui.theme_backgrounds.empty()) ? gui.theme_backgrounds[gui.current_theme_bg] : gui.user_backgrounds[gui.users[emuenv.io.user_id].backgrounds[gui.current_user_bg]],
            ImVec2(0.f, MENUBAR_BG_HEIGHT), display_size);
    else
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, MENUBAR_BG_HEIGHT), display_size, IM_COL32(11.f, 90.f, 252.f, 160.f), 0.f, ImDrawFlags_RoundCornersAll);

    const ImVec2 VIEWPORT_RES_SCALE(emuenv.viewport_size.x / emuenv.res_width_dpi_scale, emuenv.viewport_size.y / emuenv.res_height_dpi_scale);
    const ImVec2 VIEWPORT_SCALE(VIEWPORT_RES_SCALE.x * emuenv.dpi_scale, VIEWPORT_RES_SCALE.y * emuenv.dpi_scale);

    // Size of the icon depending view mode
    const ImVec2 ICON_SIZE(emuenv.cfg.apps_list_grid ? ImVec2(128.f * VIEWPORT_SCALE.x, 128.f * VIEWPORT_SCALE.y) : ImVec2(emuenv.cfg.icon_size * SCALE.x, emuenv.cfg.icon_size * SCALE.x));

    // Size of column padding
    const float column_padding_size = 20.f * SCALE.x;

    // Size of the icon part
    const float column_icon_size = ICON_SIZE.x + column_padding_size + (5.f * SCALE.x);

    // Size of the compatibility part
    const auto compat_radius = 12.f * (emuenv.cfg.apps_list_grid ? VIEWPORT_SCALE.x : SCALE.x);
    const auto full_compat_radius = (3.f * (emuenv.cfg.apps_list_grid ? VIEWPORT_SCALE.x : SCALE.x)) + compat_radius;

    auto lang = gui.lang.home_screen;

    ImGui::SetWindowFontScale(0.9f * VIEWPORT_RES_SCALE.x);

    // Sort Apps list when is not sorted
    if (!gui.app_selector.is_app_list_sorted)
        sort_app_list(gui, emuenv, gui.users[emuenv.io.user_id].sort_apps_type);

    const auto title_id_label = get_label_name(gui, TITLE_ID);
    const float title_id_size = (ImGui::CalcTextSize("PCSX12345").x + (40.f * SCALE.x));
    const auto app_ver_label = get_label_name(gui, APP_VER);
    const float app_ver_size = (ImGui::CalcTextSize(app_ver_label.c_str()).x) + (38.f * SCALE.x);
    const auto category_label = get_label_name(gui, CATEGORY);
    const float category_size = (ImGui::CalcTextSize(category_label.c_str()).x) + (38.f * SCALE.x);
    const auto compat_label = get_label_name(gui, COMPAT);
    const float compat_size = ImGui::CalcTextSize(compat_label.c_str()).x + (column_padding_size / 2.f) + (8 * SCALE.x);
    const auto title_label = get_label_name(gui, TITLE);
    const auto last_time_label = get_label_name(gui, LAST_TIME);
    const float last_time_size = (ImGui::CalcTextSize(last_time_label.c_str()).x) + (38.f * SCALE.x);
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
    ImGui::SetCursorPosY(4.f * SCALE.y);
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
        ImGui::SetColumnWidth(0, 90 * SCALE.x);
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
        ImGui::SetWindowFontScale(1.1f * VIEWPORT_RES_SCALE.x);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
        if (ImGui::MenuItem(lang["all"].c_str(), nullptr, app_region == ALL && app_compat_state == ALL_COMPAT_STATE))
            app_region = AppRegion::ALL, app_compat_state = ALL_COMPAT_STATE;
        if (ImGui::BeginMenu(lang["by_region"].c_str())) {
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
            if (ImGui::MenuItem(lang["commercial"].c_str(), nullptr, app_region == COMMERCIAL))
                app_region = COMMERCIAL;
            if (ImGui::MenuItem(lang["homebrew"].c_str(), nullptr, app_region == HOMEBREW))
                app_region = HOMEBREW;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(lang["by_compatibility_state"].c_str())) {
            if (ImGui::MenuItem(lang["all"].c_str(), nullptr, app_compat_state == ALL_COMPAT_STATE))
                app_compat_state = ALL_COMPAT_STATE;
            auto lang_compat = gui.lang.compatibility.states;
            if (ImGui::MenuItem(lang_compat[compat::UNKNOWN].c_str(), nullptr, app_compat_state == compat::UNKNOWN))
                app_compat_state = compat::UNKNOWN;
            if (ImGui::MenuItem(lang_compat[compat::NOTHING].c_str(), nullptr, app_compat_state == compat::NOTHING))
                app_compat_state = compat::NOTHING;
            if (ImGui::MenuItem(lang_compat[compat::BOOTABLE].c_str(), nullptr, app_compat_state == compat::BOOTABLE))
                app_compat_state = compat::BOOTABLE;
            if (ImGui::MenuItem(lang_compat[compat::INTRO].c_str(), nullptr, app_compat_state == compat::INTRO))
                app_compat_state = compat::INTRO;
            if (ImGui::MenuItem(lang_compat[compat::MENU].c_str(), nullptr, app_compat_state == compat::MENU))
                app_compat_state = compat::MENU;
            if (ImGui::MenuItem(lang_compat[compat::INGAME_LESS].c_str(), nullptr, app_compat_state == compat::INGAME_LESS))
                app_compat_state = compat::INGAME_LESS;
            if (ImGui::MenuItem(lang_compat[compat::INGAME_MORE].c_str(), nullptr, app_compat_state == compat::INGAME_MORE))
                app_compat_state = compat::INGAME_MORE;
            if (ImGui::MenuItem(lang_compat[compat::PLAYABLE].c_str(), nullptr, app_compat_state == compat::PLAYABLE))
                app_compat_state = compat::PLAYABLE;

            ImGui::EndMenu();
        }
        ImGui::PopStyleColor();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
    const auto search_bar_size = 120.f * SCALE.x;
    ImGui::SameLine(ImGui::GetColumnWidth() - ImGui::CalcTextSize(lang["refresh"].c_str()).x - search_bar_size - (78 * SCALE.x));
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
    const auto MARGIN_HEIGHT = INFORMATION_BAR_HEIGHT + (34.f * SCALE.y);
    const auto POS_APP_LIST = ImVec2(emuenv.viewport_pos.x + (60.f * VIEWPORT_SCALE.x), MARGIN_HEIGHT);
    const auto SIZE_APP_LIST = ImVec2(emuenv.cfg.apps_list_grid ? 840.f * VIEWPORT_SCALE.x : 900.f * SCALE.x, display_size.y - MARGIN_HEIGHT);
    ImGui::SetNextWindowPos(emuenv.cfg.apps_list_grid ? POS_APP_LIST : ImVec2(0, MARGIN_HEIGHT), ImGuiCond_Always);
    ImGui::BeginChild("##apps_list", SIZE_APP_LIST, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    // Set Scroll Pos
    current_scroll_pos = ImGui::GetScrollY();
    max_scroll_pos = ImGui::GetScrollMaxY();
    if (scroll_type != 0) {
        const float scroll_move = (scroll_type == -1 ? 340.f : -340.f) * SCALE.y;
        ImGui::SetScrollY(ImGui::GetScrollY() + scroll_move);
        scroll_type = 0;
    }

    const auto GRID_COLUMN_SIZE = ICON_SIZE.x + (80.f * VIEWPORT_SCALE.x);
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
    const auto display_app = [&](const std::vector<gui::App> &apps_list, std::map<std::string, ImGui_Texture> &apps_icon) {
        for (const auto &app : apps_list) {
            bool selected = false;
            const auto is_not_sys_app = app.title_id.find("NPXS") == std::string::npos;

            if (is_not_sys_app) {
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

            // Determine if the element is within the visible area of the window.
            ImVec2 item_rect_min = ImGui::GetItemRectMin();
            const float margin = 200.f * SCALE.y;
            const auto element_is_within_visible_area = (item_rect_min.y >= -margin) && (item_rect_min.y <= (ImGui::GetWindowPos().y + ImGui::GetWindowSize().y + margin));

            // Draw the app icons and custom config button only when they are within the visible area.
            if (element_is_within_visible_area) {
                if (apps_icon.contains(app.path)) {
                    if (emuenv.cfg.apps_list_grid)
                        ImGui::SetCursorPosX(GRID_ICON_POS);
                    else
                        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x + (5.f * SCALE.x), ImGui::GetCursorPos().y + (5.f * SCALE.y)));
                    const auto POS_MIN = ImGui::GetCursorScreenPos();
                    const ImVec2 POS_MAX(POS_MIN.x + ICON_SIZE.x, POS_MIN.y + ICON_SIZE.y);
                    ImGui::GetWindowDrawList()->AddImageRounded(apps_icon[app.path], POS_MIN, POS_MAX, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, ICON_SIZE.x * SCALE.x, ImDrawFlags_RoundCornersAll);
                }
                const auto IS_CUSTOM_CONFIG = fs::exists(fs::path(emuenv.base_path) / "config" / fmt::format("config_{}.xml", app.path));
                if (IS_CUSTOM_CONFIG) {
                    if (emuenv.cfg.apps_list_grid)
                        ImGui::SetCursorPosX(GRID_ICON_POS);
                    ImGui::SetCursorPosY(POS_ICON.y + ICON_SIZE.y - ImGui::GetFontSize() - (7.8f * emuenv.dpi_scale));
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                    ImGui::Button("CC", ImVec2(40.f * SCALE.x, 0.f));
                    ImGui::PopStyleColor();
                }
            }

            if (!emuenv.cfg.apps_list_grid)
                ImGui::NextColumn();

            // Draw the compatibility badge for commercial apps when they are within the visible area.
            if (element_is_within_visible_area && (app.title_id.find("PCS") != std::string::npos)) {
                const auto compat_state = (gui.compat.compat_db_loaded ? gui.compat.app_compat_db.contains(app.title_id) : false) ? gui.compat.app_compat_db[app.title_id].state : compat::UNKNOWN;
                const auto compat_state_vec4 = gui.compat.compat_color[compat_state];
                const ImU32 compat_state_color = IM_COL32((int)(compat_state_vec4.x * 255.0f), (int)(compat_state_vec4.y * 255.0f), (int)(compat_state_vec4.z * 255.0f), (int)(compat_state_vec4.w * 255.0f));
                const auto current_pos = ImGui::GetCursorPos();
                const auto grid_compat_padding = 4.f * VIEWPORT_SCALE.x;
                const auto compat_state_pos = emuenv.cfg.apps_list_grid ? ImVec2(GRID_ICON_POS + full_compat_radius + grid_compat_padding, POS_ICON.y + full_compat_radius + grid_compat_padding) : ImVec2(current_pos.x + ((ImGui::GetColumnWidth() - column_padding_size) / 2.f), current_pos.y + (ICON_SIZE.y / 2.f));
                ImGui::SetCursorPos(compat_state_pos);
                ImGui::GetWindowDrawList()->AddCircleFilled(ImGui::GetCursorScreenPos(), full_compat_radius, IM_COL32(0, 0, 0, 255));
                ImGui::GetWindowDrawList()->AddCircleFilled(ImGui::GetCursorScreenPos(), compat_radius, compat_state_color);
            }

            ImGui::SetCursorPosY(POS_ICON.y);
            if (emuenv.cfg.apps_list_grid)
                ImGui::SetCursorPosX(GRID_ICON_POS);
            const auto icon_flags = emuenv.cfg.apps_list_grid ? ImGuiSelectableFlags_None : ImGuiSelectableFlags_SpanAllColumns;
            const ImVec2 list_selectable_size(0.f, ICON_SIZE.y + (10.f * SCALE.y));
            ImGui::Selectable("##icon", &selected, icon_flags, emuenv.cfg.apps_list_grid ? ICON_SIZE : list_selectable_size);
            if (!gui.configuration_menu.custom_settings_dialog && ImGui::IsItemHovered())
                emuenv.app_path = app.path;
            if (emuenv.app_path == app.path)
                draw_app_context_menu(gui, emuenv, app.path);
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
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - (10.f * SCALE.x) + (ImGui::GetColumnWidth() / 2.f) - HALF_CLOCK_SIZE);
                    ImGui::Text("%s", CLOCK_STR.c_str());
                } else
                    ImGui::Selectable(!gui.lang.app_context["never"].empty() ? gui.lang.app_context["never"].c_str() : "Never", false, ImGuiSelectableFlags_None, ImVec2(0.f, ICON_SIZE.y));
                ImGui::NextColumn();
                ImGui::Selectable(app.title.c_str(), false, ImGuiSelectableFlags_None, list_selectable_size);
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();
            } else {
                ImGui::SetCursorPosX(GRID_INIT_POS - (ImGui::CalcTextSize(app.stitle.c_str(), 0, false, ICON_SIZE.x + (32.f * SCALE.x)).x / 2.f));
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (GRID_COLUMN_SIZE - (48.f * SCALE.x)));
                ImGui::TextColored(!gui.theme_backgrounds_font_color.empty() && gui.users[emuenv.io.user_id].use_theme_bg ? gui.theme_backgrounds_font_color[gui.current_theme_bg] : GUI_COLOR_TEXT, "%s", app.stitle.c_str());
                ImGui::PopTextWrapPos();
            }
            ImGui::NextColumn();
            if (selected)
                pre_load_app(gui, emuenv, emuenv.cfg.show_live_area_screen, app.path);
            ImGui::PopID();
        }
    };
    // System Applications
    display_app(gui.app_selector.sys_apps, gui.app_selector.sys_apps_icon);
    // User Applications
    display_app(gui.app_selector.user_apps, gui.app_selector.user_apps_icon);
    ImGui::PopStyleColor();
    ImGui::Columns(1);
    ImGui::SetWindowFontScale(1.f);
    ImGui::EndChild();

    const auto SELECTABLE_SIZE = ImVec2(50.f * SCALE.x, 60.f * SCALE.y);

    if (current_scroll_pos > 0) {
        const auto ARROW_UPP_CENTER = ImVec2(display_size.x - (30.f * SCALE.x), 110.f * SCALE.y);
        ImGui::GetWindowDrawList()->AddTriangleFilled(
            ImVec2(ARROW_UPP_CENTER.x - (20.f * SCALE.x), ARROW_UPP_CENTER.y + (16.f * SCALE.y)),
            ImVec2(ARROW_UPP_CENTER.x, ARROW_UPP_CENTER.y - (16.f * SCALE.y)),
            ImVec2(ARROW_UPP_CENTER.x + (20.f * SCALE.x), ARROW_UPP_CENTER.y + (16.f * SCALE.y)), ARROW_COLOR);
        ImGui::SetCursorPos(ImVec2(ARROW_UPP_CENTER.x - (SELECTABLE_SIZE.x / 2.f), ARROW_UPP_CENTER.y - SELECTABLE_SIZE.y));
        if ((ImGui::Selectable("##upp", false, ImGuiSelectableFlags_None, SELECTABLE_SIZE))
            || !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(emuenv.cfg.keyboard_leftstick_up) || ImGui::IsKeyPressed(emuenv.cfg.keyboard_button_up))
            scroll_type = 1;
    }
    if (!gui.apps_list_opened.empty()) {
        const auto ARROW_CENTER = ImVec2(display_size.x - (30.f * SCALE.x), display_size.y - (250.f * SCALE.y));
        ImGui::GetWindowDrawList()->AddTriangleFilled(
            ImVec2(ARROW_CENTER.x - (16.f * SCALE.x), ARROW_CENTER.y - (20.f * SCALE.y)),
            ImVec2(ARROW_CENTER.x + (16.f * SCALE.x), ARROW_CENTER.y),
            ImVec2(ARROW_CENTER.x - (16.f * SCALE.x), ARROW_CENTER.y + (20.f * SCALE.y)), ARROW_COLOR);
        ImGui::SetCursorPos(ImVec2(ARROW_CENTER.x - (SELECTABLE_SIZE.x / 2.f), ARROW_CENTER.y - SELECTABLE_SIZE.y));
        if ((ImGui::Selectable("##right", false, ImGuiSelectableFlags_None, SELECTABLE_SIZE))
            || !ImGui::GetIO().WantTextInput && (ImGui::IsKeyPressed(emuenv.cfg.keyboard_button_r1) || ImGui::IsKeyPressed(emuenv.cfg.keyboard_leftstick_right) || ImGui::IsKeyPressed(emuenv.cfg.keyboard_button_right))) {
            last_time["start"] = 0;
            ++gui.current_app_selected;
            gui.vita_area.home_screen = false;
            gui.vita_area.live_area_screen = true;
        }
    }
    if (current_scroll_pos < max_scroll_pos) {
        const auto ARROW_DOWN_CENTER = ImVec2(display_size.x - (30.f * SCALE.x), display_size.y - (30.f * SCALE.y));
        ImGui::GetWindowDrawList()->AddTriangleFilled(
            ImVec2(ARROW_DOWN_CENTER.x + (20.f * SCALE.x), ARROW_DOWN_CENTER.y - (16.f * SCALE.y)),
            ImVec2(ARROW_DOWN_CENTER.x, ARROW_DOWN_CENTER.y + (16.f * SCALE.y)),
            ImVec2(ARROW_DOWN_CENTER.x - (20.f * SCALE.x), ARROW_DOWN_CENTER.y - (16.f * SCALE.y)), ARROW_COLOR);
        ImGui::SetCursorPos(ImVec2(ARROW_DOWN_CENTER.x - (SELECTABLE_SIZE.x / 2.f), ARROW_DOWN_CENTER.y - SELECTABLE_SIZE.y));
        if ((ImGui::Selectable("##down", false, ImGuiSelectableFlags_None, SELECTABLE_SIZE))
            || !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(emuenv.cfg.keyboard_leftstick_down) || ImGui::IsKeyPressed(emuenv.cfg.keyboard_button_down))
            scroll_type = -1;
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace gui
