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

#include "private.h"

#include <config/functions.h>

#include <gui/functions.h>

#include <io/VitaIoDevice.h>

#include <util/log.h>
#include <util/safe_time.h>
#include <util/string_utils.h>

using namespace std::string_literals;

namespace gui {

void init_user_apps(GuiState &gui, HostState &host) {
    gui.apps_background.clear();
    gui.apps_list_opened.clear();
    gui.app_selector.user_apps_icon.clear();
    gui.current_app_selected = -1;
    gui.live_area_contents.clear();
    gui.live_items.clear();
    if (gui.app_selector.icon_async_loader)
        gui.app_selector.icon_async_loader->quit = true;

    std::thread init_apps([&gui, &host]() {
        auto app_list_size = gui.app_selector.user_apps.size();
        gui.app_selector.user_apps.clear();
        get_user_apps_title(gui, host);
        save_apps_cache(gui, host);

        if (gui.app_selector.user_apps.empty())
            return false;

        gui.app_selector.is_app_list_sorted = false;
        init_last_time_apps(gui, host);

        init_apps_icon(gui, host, gui.app_selector.user_apps);

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

void init_last_time_apps(GuiState &gui, HostState &host) {
    const auto last_time_apps = [&](std::vector<gui::App> &apps_list) {
        for (auto &app : apps_list) {
            const auto TIME_APP_INDEX = std::find_if(gui.time_apps[host.io.user_id].begin(), gui.time_apps[host.io.user_id].end(), [&](const TimeApp &t) {
                return t.app == app.path;
            });

            app.last_time = (TIME_APP_INDEX != gui.time_apps[host.io.user_id].end()) ? TIME_APP_INDEX->last_time_used : 0;
        }
    };

    last_time_apps(gui.app_selector.sys_apps);
    last_time_apps(gui.app_selector.user_apps);
    gui.app_selector.is_app_list_sorted = false;
}

std::vector<std::string>::iterator get_app_open_list_index(GuiState &gui, const std::string &app_path) {
    return std::find(gui.apps_list_opened.begin(), gui.apps_list_opened.end(), app_path);
}

void update_apps_list_opened(GuiState &gui, HostState &host, const std::string &app_path) {
    if ((get_app_open_list_index(gui, app_path) != gui.apps_list_opened.end()) && (gui.apps_list_opened.front() != app_path))
        gui.apps_list_opened.erase(get_app_open_list_index(gui, app_path));
    if ((get_app_open_list_index(gui, app_path) == gui.apps_list_opened.end()))
        gui.apps_list_opened.insert(gui.apps_list_opened.begin(), app_path);
    gui.current_app_selected = 0;
    if (gui.apps_list_opened.size() > 6) {
        const auto last_app = gui.apps_list_opened.back() == host.io.app_path ? gui.apps_list_opened[gui.apps_list_opened.size() - 2] : gui.apps_list_opened.back();
        gui.live_area_contents.erase(last_app);
        gui.live_items.erase(last_app);
        gui.apps_list_opened.erase(get_app_open_list_index(gui, last_app));
    }
}

static std::map<std::string, uint64_t> last_time;

void pre_load_app(GuiState &gui, HostState &host, bool live_area, const std::string &app_path) {
    if (live_area) {
        update_apps_list_opened(gui, host, app_path);
        last_time["home"] = 0;
        init_live_area(gui, host);
        gui.live_area.app_selector = false;
        gui.live_area.information_bar = true;
        gui.live_area.live_area_screen = true;
    } else
        pre_run_app(gui, host, app_path);
}

void pre_run_app(GuiState &gui, HostState &host, const std::string &app_path) {
    if (app_path.find("NPXS") == std::string::npos) {
        if (host.io.app_path != app_path) {
            if (!host.io.app_path.empty())
                gui.live_area.app_close = true;
            else {
                gui.live_area.information_bar = false;
                gui.live_area.app_selector = false;
                gui.live_area.live_area_screen = false;
                host.io.app_path = app_path;
            }
        } else {
            gui.live_area.live_area_screen = false;
            gui.live_area.information_bar = false;
        }
    } else {
        gui.live_area.app_selector = false;
        gui.live_area.live_area_screen = false;
        init_app_background(gui, host, app_path);
        update_last_time_app_used(gui, host, app_path);

        if (app_path == "NPXS10008") {
            init_trophy_collection(gui, host);
            gui.live_area.trophy_collection = true;
        } else if (app_path == "NPXS10015")
            gui.live_area.settings = true;
        else {
            init_content_manager(gui, host);
            gui.live_area.content_manager = true;
        }
    }
}

void draw_app_close(GuiState &gui, HostState &host) {
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);

    const auto WINDOW_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);
    const auto BUTTON_SIZE = ImVec2(320.f * SCALE.x, 46.f * SCALE.y);

    auto common = host.common_dialog.lang.common;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
    ImGui::Begin("##app_close", &gui.live_area.app_close, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
    ImGui::BeginChild("##app_close_child", WINDOW_SIZE, true, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);

    const auto ICON_SIZE = ImVec2(64.f * SCALE.x, 64.f * SCALE.y);

    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
    ImGui::SetCursorPos(ImVec2(50.f * SCALE.x, 108.f * SCALE.y));
    const auto warn_app_close = !gui.lang.game_data["app_close"].empty() ? gui.lang.game_data["app_close"].c_str() : "The following application will close.";
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", warn_app_close);
    if (gui.app_selector.user_apps_icon.find(host.io.app_path) != gui.app_selector.user_apps_icon.end()) {
        const auto ICON_POS_SCALE = ImVec2(152.f * SCALE.x, (display_size.y / 2.f) - (ICON_SIZE.y / 2.f) - (10.f * SCALE.y));
        const auto ICON_SIZE_SCALE = ImVec2(ICON_POS_SCALE.x + ICON_SIZE.x, ICON_POS_SCALE.y + ICON_SIZE.y);
        ImGui::GetWindowDrawList()->AddImageRounded(get_app_icon(gui, host.io.app_path)->second, ICON_POS_SCALE, ICON_SIZE_SCALE, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, 40.f * SCALE.x, ImDrawCornerFlags_All);
    }
    ImGui::SetCursorPos(ImVec2(ICON_SIZE.x + (72.f * SCALE.x), (WINDOW_SIZE.y / 2.f) - ImGui::CalcTextSize(host.current_app_title.c_str()).y + (4.f * SCALE.y)));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", host.current_app_title.c_str());
    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * SCALE.x)), WINDOW_SIZE.y - BUTTON_SIZE.y - (24.0f * SCALE.y)));
    if (ImGui::Button(!common["cancel"].empty() ? common["cancel"].c_str() : "Cancel", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_circle))
        gui.live_area.app_close = false;
    ImGui::SameLine(0, 20.f * SCALE.x);
    if (ImGui::Button("OK", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross)) {
        const auto app_path = gui.apps_list_opened[gui.current_app_selected];
        update_time_app_used(gui, host, host.io.app_path);
        host.kernel.exit_delete_all_threads();
        host.load_app_path = app_path;
        host.load_exec = true;
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

static AppRegion app_region = ALL;
static bool app_filter(const std::string &app) {
    const auto filter_app_region = [&](const std::vector<std::string> &app_region) {
        const auto app_region_index = std::find_if(app_region.begin(), app_region.end(), [&](const std::string &a) {
            return app.find(a) != std::string::npos;
        });
        return app_region_index == app_region.end();
    };

    switch (app_region) {
    case USA:
        if (filter_app_region({ "PCSA", "PCSE" }))
            return true;
        break;
    case EURO:
        if (filter_app_region({ "PCSF", "PCSB" }))
            return true;
        break;
    case JAPAN:
        if (filter_app_region({ "PCSC", "PCSG" }))
            return true;
        break;
    case ASIA:
        if (filter_app_region({ "PCSD", "PCSH" }))
            return true;
        break;
    case COMMERCIAL:
        if (filter_app_region({ "PCS" }))
            return true;
        break;
    case HOMEBREW:
        if (!filter_app_region({ "PCS" }))
            return true;
        break;
    }
    return false;
}

static void sort_app_list(GuiState &gui, HostState &host, const SortType &type) {
    auto &sorted = gui.app_selector.app_list_sorted[type];
    if (gui.app_selector.is_app_list_sorted) {
        for (auto &state : gui.app_selector.app_list_sorted)
            state.second = (state.first == type) ? static_cast<gui::SortState>(std::max<int>(1, (state.second + 1) % 3)) : NOT_SORTED;

        gui.users[host.io.user_id].sort_apps_type = type;
        gui.users[host.io.user_id].sort_apps_state = sorted;

        save_user(gui, host, host.io.user_id);
    } else {
        sorted = gui.users[host.io.user_id].sort_apps_state;
        gui.app_selector.is_app_list_sorted = true;
    }

    std::sort(gui.app_selector.user_apps.begin(), gui.app_selector.user_apps.end(), [&sorted, &type](const App &lhs, const App &rhs) {
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
    switch (type) {
    case APP_VER: label = "Ver"; break;
    case CATEGORY: label = "Cat"; break;
    case LAST_TIME: label = "Last Time"; break;
    case TITLE: label = "Title"; break;
    case TITLE_ID: label = "Title ID"; break;
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

void draw_app_selector(GuiState &gui, HostState &host) {
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);
    const auto INFORMATION_BAR_HEIGHT = 32.f * SCALE.y;
    const auto MENUBAR_BG_HEIGHT = (!host.cfg.show_info_bar && host.display.imgui_render) || !gui.live_area.information_bar ? 22.f * SCALE.x : INFORMATION_BAR_HEIGHT;

    const auto is_background = (gui.users[host.io.user_id].use_theme_bg && !gui.theme_backgrounds.empty()) || !gui.user_backgrounds.empty();

    ImGui::SetNextWindowPos(ImVec2(0, INFORMATION_BAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y - INFORMATION_BAR_HEIGHT), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::SetNextWindowBgAlpha(is_background ? host.cfg.background_alpha : 0.f);
    ImGui::Begin("##app_selector", &gui.live_area.app_selector, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetWindowFontScale(1.1f * RES_SCALE.x);
    if (!host.display.imgui_render || ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
        gui.live_area.information_bar = true;

    if (!gui.file_menu.archive_install_dialog && !gui.file_menu.firmware_install_dialog && !gui.file_menu.pkg_install_dialog && !gui.configuration_menu.custom_settings_dialog && !gui.configuration_menu.settings_dialog && !gui.controls_menu.controls_dialog) {
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || ImGui::IsAnyItemActive() || ImGui::IsAnyItemHovered())
            last_time["start"] = 0;
        else {
            if (last_time["start"] == 0)
                last_time["start"] = current_time();

            while (last_time["start"] + host.cfg.delay_start < current_time()) {
                last_time["start"] += host.cfg.delay_start;
                last_time["home"] = 0;
                gui.live_area.app_selector = false;
                gui.live_area.information_bar = true;
                gui.live_area.start_screen = true;
            }
        }
    }

    if (!gui.live_area.start_screen && !gui.live_area.live_area_screen && (!gui.theme_backgrounds.empty() || !gui.user_backgrounds.empty())) {
        if (last_time["home"] == 0)
            last_time["home"] = current_time();

        while (last_time["home"] + host.cfg.delay_background < current_time()) {
            last_time["home"] += host.cfg.delay_background;

            if (gui.users[host.io.user_id].use_theme_bg) {
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
        ImGui::GetBackgroundDrawList()->AddImage((gui.users[host.io.user_id].use_theme_bg && !gui.theme_backgrounds.empty()) ? gui.theme_backgrounds[gui.current_theme_bg] : gui.user_backgrounds[gui.users[host.io.user_id].backgrounds[gui.current_user_bg]],
            ImVec2(0.f, MENUBAR_BG_HEIGHT), display_size);
    else
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, MENUBAR_BG_HEIGHT), display_size, IM_COL32(11.f, 90.f, 252.f, 160.f), 0.f, ImDrawFlags_RoundCornersAll);

    const float icon_size = static_cast<float>(host.cfg.icon_size) * SCALE.x;
    const float column_icon_size = icon_size + (20.f * SCALE.x);

    switch (gui.app_selector.state) {
    case SELECT_APP:
        // Sort Apps list when is not sorted
        if (!gui.app_selector.is_app_list_sorted)
            sort_app_list(gui, host, gui.users[host.io.user_id].sort_apps_type);

        const auto title_id_label = get_label_name(gui, TITLE_ID);
        float title_id_size = (ImGui::CalcTextSize(title_id_label.c_str()).x) + (60.f * SCALE.x);
        const auto app_ver_label = get_label_name(gui, APP_VER);
        float app_ver_size = (ImGui::CalcTextSize(app_ver_label.c_str()).x) + (30.f * SCALE.x);
        const auto category_label = get_label_name(gui, CATEGORY);
        float category_size = (ImGui::CalcTextSize(category_label.c_str()).x) + (30.f * SCALE.x);
        const auto title_label = get_label_name(gui, TITLE);
        const auto last_time_label = get_label_name(gui, LAST_TIME);
        float last_time_size = (ImGui::CalcTextSize(last_time_label.c_str()).x) + (30.f * SCALE.x);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
        if (!host.cfg.apps_list_grid) {
            ImGui::Columns(6);
            ImGui::SetColumnWidth(0, column_icon_size);
            if (ImGui::Button("Filter"))
                ImGui::OpenPopup("app_filter");
            ImGui::NextColumn();
            ImGui::SetColumnWidth(1, title_id_size);
            if (ImGui::Button(title_id_label.c_str()))
                sort_app_list(gui, host, TITLE_ID);
            ImGui::NextColumn();
            ImGui::SetColumnWidth(2, app_ver_size);
            if (ImGui::Button(app_ver_label.c_str()))
                sort_app_list(gui, host, APP_VER);
            ImGui::NextColumn();
            ImGui::SetColumnWidth(3, category_size);
            if (ImGui::Button(category_label.c_str()))
                sort_app_list(gui, host, CATEGORY);
            ImGui::NextColumn();
            ImGui::SetColumnWidth(4, last_time_size);
            if (ImGui::Button(last_time_label.c_str()))
                sort_app_list(gui, host, LAST_TIME);
            ImGui::NextColumn();
            if (ImGui::Button(title_label.c_str()))
                sort_app_list(gui, host, TITLE);
        } else {
            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, 90 * SCALE.x);
            if (ImGui::Button("Filter"))
                ImGui::OpenPopup("app_filter");
            ImGui::NextColumn();
            if (ImGui::Button("Sort Apps By"))
                ImGui::OpenPopup("sort_apps");
            if (ImGui::BeginPopup("sort_apps")) {
                ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
                if (ImGui::MenuItem(app_ver_label.c_str(), nullptr, gui.app_selector.app_list_sorted[APP_VER] != NOT_SORTED))
                    sort_app_list(gui, host, APP_VER);
                if (ImGui::MenuItem(category_label.c_str(), nullptr, gui.app_selector.app_list_sorted[CATEGORY] != NOT_SORTED))
                    sort_app_list(gui, host, CATEGORY);
                if (ImGui::MenuItem(last_time_label.c_str(), nullptr, gui.app_selector.app_list_sorted[LAST_TIME] != NOT_SORTED))
                    sort_app_list(gui, host, LAST_TIME);
                if (ImGui::MenuItem(title_label.c_str(), nullptr, gui.app_selector.app_list_sorted[TITLE] != NOT_SORTED))
                    sort_app_list(gui, host, TITLE);
                if (ImGui::MenuItem(title_id_label.c_str(), nullptr, gui.app_selector.app_list_sorted[TITLE_ID] != NOT_SORTED))
                    sort_app_list(gui, host, TITLE_ID);
                ImGui::PopStyleColor();
                ImGui::EndPopup();
            }
        }
        if (ImGui::BeginPopup("app_filter")) {
            ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
            if (ImGui::MenuItem("All", nullptr, app_region == ALL))
                app_region = ALL;
            if (ImGui::BeginMenu("By Region")) {
                ImGui::SetWindowFontScale(1.1f * RES_SCALE.x);
                if (ImGui::MenuItem("USA", nullptr, app_region == USA))
                    app_region = USA;
                if (ImGui::MenuItem("Europe", nullptr, app_region == EURO))
                    app_region = EURO;
                if (ImGui::MenuItem("Japan", nullptr, app_region == JAPAN))
                    app_region = JAPAN;
                if (ImGui::MenuItem("Asia", nullptr, app_region == ASIA))
                    app_region = ASIA;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("By Type")) {
                if (ImGui::MenuItem("Commercial", nullptr, app_region == COMMERCIAL))
                    app_region = COMMERCIAL;
                if (ImGui::MenuItem("Homebrew", nullptr, app_region == HOMEBREW))
                    app_region = HOMEBREW;
                ImGui::EndMenu();
            }
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
        ImGui::PopStyleColor();
        const auto search_bar_size = 120.f * SCALE.x;
        ImGui::SameLine(ImGui::GetColumnWidth() - ImGui::CalcTextSize("Refresh").x - search_bar_size - (78 * SCALE.x));
        if (ImGui::Button("Refresh"))
            init_user_apps(gui, host);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_SEARCH_BAR_TEXT);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, GUI_COLOR_SEARCH_BAR_BG);
        gui.app_search_bar.Draw("##app_search_bar", search_bar_size);
        ImGui::PopStyleColor(2);
        ImGui::NextColumn();
        ImGui::Columns(1);
        ImGui::Separator();
        const auto POS_APP_LIST = ImVec2(60.f * SCALE.x, 48.f * SCALE.y + INFORMATION_BAR_HEIGHT);
        const auto SIZE_APP_LIST = ImVec2((host.cfg.apps_list_grid ? 840.f : 900.f) * SCALE.x, display_size.y - POS_APP_LIST.y);
        ImGui::SetNextWindowPos(host.cfg.apps_list_grid ? POS_APP_LIST : ImVec2(1.f, POS_APP_LIST.y), ImGuiCond_Always);
        ImGui::BeginChild("##apps_list", SIZE_APP_LIST, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

        // Set Scroll Pos
        current_scroll_pos = ImGui::GetScrollY();
        max_scroll_pos = ImGui::GetScrollMaxY();
        if (scroll_type != 0) {
            const float scroll_move = (scroll_type == -1 ? 340.f : -340.f) * SCALE.y;
            ImGui::SetScrollY(ImGui::GetScrollY() + scroll_move);
            scroll_type = 0;
        }

        const auto GRID_ICON_SIZE = ImVec2(128.f * SCALE.x, 128.f * SCALE.y);
        const auto GRID_COLUMN_SIZE = GRID_ICON_SIZE.x + (80.f * SCALE.x);
        if (!host.cfg.apps_list_grid) {
            ImGui::Columns(6, nullptr, true);
            ImGui::SetColumnWidth(0, column_icon_size);
            ImGui::SetColumnWidth(1, title_id_size);
            ImGui::SetColumnWidth(2, app_ver_size);
            ImGui::SetColumnWidth(3, category_size);
            ImGui::SetColumnWidth(4, last_time_size);
        } else {
            ImGui::Columns(4, nullptr, false);
            ImGui::SetColumnWidth(0, GRID_COLUMN_SIZE);
            ImGui::SetColumnWidth(1, GRID_COLUMN_SIZE);
            ImGui::SetColumnWidth(2, GRID_COLUMN_SIZE);
            ImGui::SetColumnWidth(3, GRID_COLUMN_SIZE);
        }
        ImGui::SetWindowFontScale(0.9f);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
        const auto display_app = [&](const std::vector<gui::App> &apps_list, std::map<std::string, ImGui_Texture> &apps_icon) {
            for (const auto &app : apps_list) {
                bool selected = false;
                if (app_region != ALL) {
                    if ((app.title_id.find("NPXS") == std::string::npos) && app_filter(app.title_id))
                        continue;
                }
                if (!gui.app_search_bar.PassFilter(app.title.c_str()) && !gui.app_search_bar.PassFilter(app.title_id.c_str()))
                    continue;
                const auto POS_ICON = ImGui::GetCursorPosY();
                const auto GRID_INIT_POS = ImGui::GetCursorPosX() + (GRID_COLUMN_SIZE / 2.f) - (10.f * SCALE.x);
                const auto ICON_SIZE = host.cfg.apps_list_grid ? GRID_ICON_SIZE : ImVec2(icon_size, icon_size);
                if (apps_icon.find(app.path) != apps_icon.end()) {
                    if (host.cfg.apps_list_grid)
                        ImGui::SetCursorPosX(GRID_INIT_POS - (GRID_ICON_SIZE.x / 2.f));
                    ImGui::Image(apps_icon[app.path], ICON_SIZE);
                }
                ImGui::SetCursorPosY(POS_ICON);
                if (host.cfg.apps_list_grid)
                    ImGui::SetCursorPosX(GRID_INIT_POS - (GRID_ICON_SIZE.x / 2.f));
                ImGui::PushID(app.path.c_str());
                ImGui::Selectable("##icon", &selected, host.cfg.apps_list_grid ? ImGuiSelectableFlags_None : ImGuiSelectableFlags_SpanAllColumns, host.cfg.apps_list_grid ? GRID_ICON_SIZE : ImVec2(0.f, icon_size));
                if (!gui.configuration_menu.custom_settings_dialog && ImGui::IsItemHovered())
                    host.app_path = app.path;
                if (host.app_path == app.path)
                    draw_app_context_menu(gui, host, app.path);
                const auto IS_CUSTOM_CONFIG = fs::exists(fs::path(host.base_path) / "config" / fmt::format("config_{}.xml", app.path));
                if (IS_CUSTOM_CONFIG) {
                    if (host.cfg.apps_list_grid)
                        ImGui::SetCursorPosX(GRID_INIT_POS - (GRID_ICON_SIZE.x / 2.f));
                    ImGui::SetCursorPosY(POS_ICON + ICON_SIZE.y - ImGui::GetFontSize() - (7.8f * host.dpi_scale));
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                    ImGui::Button("CC", ImVec2(40.f * SCALE.x, 0.f));
                    ImGui::PopStyleColor();
                }
                if (!host.cfg.apps_list_grid) {
                    ImGui::NextColumn();
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
                    ImGui::PushStyleColor(ImGuiCol_Text, !gui.theme_backgrounds_font_color.empty() && gui.users[host.io.user_id].use_theme_bg ? gui.theme_backgrounds_font_color[gui.current_theme_bg] : GUI_COLOR_TEXT);
                    ImGui::Selectable(app.title_id.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, icon_size));
                    ImGui::NextColumn();
                    ImGui::Selectable(app.app_ver.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, icon_size));
                    ImGui::NextColumn();
                    ImGui::Selectable(app.category.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, icon_size));
                    ImGui::NextColumn();
                    if (app.last_time) {
                        tm date_tm = {};
                        SAFE_LOCALTIME(&app.last_time, &date_tm);
                        auto LAST_TIME = get_date_time(gui, host, date_tm);
                        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 1.f));
                        ImGui::Selectable(LAST_TIME["date"].c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, icon_size / 2.f));
                        ImGui::PopStyleVar();
                        const auto CLOCK_STR = gui.users[host.io.user_id].clock_12_hour ? fmt::format("{} {}", LAST_TIME["clock"], LAST_TIME["day-moment"]) : LAST_TIME["clock"];
                        const auto HALF_CLOCK_SIZE = ImGui::CalcTextSize(CLOCK_STR.c_str()).x / 2.f;
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - (10.f * SCALE.x) + (ImGui::GetColumnWidth() / 2.f) - HALF_CLOCK_SIZE);
                        ImGui::Text("%s", CLOCK_STR.c_str());
                    } else
                        ImGui::Selectable(!gui.lang.app_context["never"].empty() ? gui.lang.app_context["never"].c_str() : "Never", false, ImGuiSelectableFlags_None, ImVec2(0.f, icon_size));
                    ImGui::NextColumn();
                    ImGui::Selectable(app.title.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, icon_size));
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar();
                } else {
                    ImGui::SetCursorPosX(GRID_INIT_POS - (ImGui::CalcTextSize(app.stitle.c_str(), 0, false, GRID_ICON_SIZE.x + (32.f * SCALE.x)).x / 2.f));
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (GRID_COLUMN_SIZE - (48.f * SCALE.x)));
                    ImGui::TextColored(!gui.theme_backgrounds_font_color.empty() && gui.users[host.io.user_id].use_theme_bg ? gui.theme_backgrounds_font_color[gui.current_theme_bg] : GUI_COLOR_TEXT, "%s", app.stitle.c_str());
                    ImGui::PopTextWrapPos();
                }
                ImGui::NextColumn();
                ImGui::PopID();
                if (selected)
                    pre_load_app(gui, host, host.cfg.show_live_area_screen, app.path);
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
        break;
    }

    const auto SELECTABLE_SIZE = ImVec2(50.f * SCALE.x, 60.f * SCALE.y);

    if (current_scroll_pos > 0) {
        const auto ARROW_UPP_CENTER = ImVec2(display_size.x - (30.f * SCALE.x), 110.f * SCALE.y);
        ImGui::GetWindowDrawList()->AddTriangleFilled(
            ImVec2(ARROW_UPP_CENTER.x - (20.f * SCALE.x), ARROW_UPP_CENTER.y + (16.f * SCALE.y)),
            ImVec2(ARROW_UPP_CENTER.x, ARROW_UPP_CENTER.y - (16.f * SCALE.y)),
            ImVec2(ARROW_UPP_CENTER.x + (20.f * SCALE.x), ARROW_UPP_CENTER.y + (16.f * SCALE.y)), ARROW_COLOR);
        ImGui::SetCursorPos(ImVec2(ARROW_UPP_CENTER.x - (SELECTABLE_SIZE.x / 2.f), ARROW_UPP_CENTER.y - SELECTABLE_SIZE.y));
        if ((ImGui::Selectable("##upp", false, ImGuiSelectableFlags_None, SELECTABLE_SIZE))
            || ImGui::IsKeyPressed(host.cfg.keyboard_leftstick_up) || ImGui::IsKeyPressed(host.cfg.keyboard_button_up))
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
            || ImGui::IsKeyPressed(host.cfg.keyboard_button_r1) || ImGui::IsKeyPressed(host.cfg.keyboard_leftstick_right)) {
            last_time["start"] = 0;
            ++gui.current_app_selected;
            gui.live_area.app_selector = false;
            gui.live_area.live_area_screen = true;
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
            || ImGui::IsKeyPressed(host.cfg.keyboard_leftstick_down) || ImGui::IsKeyPressed(host.cfg.keyboard_button_down))
            scroll_type = -1;
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace gui
