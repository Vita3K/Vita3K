// Vita3K emulator project
// Copyright (C) 2020 Vita3K team
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

#include <gui/functions.h>

#include <util/log.h>
#include <util/string_utils.h>

using namespace std::string_literals;

namespace gui {

bool refresh_app_list(GuiState &gui, HostState &host) {
    auto app_list_size = gui.app_selector.apps.size();

    gui.apps_background.clear();
    gui.app_selector.apps.clear();
    gui.app_selector.icons.clear();
    gui.live_area_contents.clear();
    gui.live_items.clear();

    get_apps_title(gui, host);

    if (gui.app_selector.apps.empty())
        return false;

    init_icons(gui, host);

    std::sort(gui.app_selector.apps.begin(), gui.app_selector.apps.end(), [](const App &lhs, const App &rhs) {
        return string_utils::toupper(lhs.title) < string_utils::toupper(rhs.title);
    });

    std::string change_app_list = "new application(s) added";
    if (app_list_size == gui.app_selector.apps.size())
        return false;

    if (app_list_size > gui.app_selector.apps.size()) {
        change_app_list = "application(s) removed";
        app_list_size -= gui.app_selector.apps.size();
    } else
        app_list_size = gui.app_selector.apps.size() - app_list_size;

    LOG_INFO("{} {}", app_list_size, change_app_list);

    return true;
}

inline uint64_t current_time() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

static std::map<std::string, uint64_t> last_time;
static auto MENUBAR_HEIGHT = 22.f;

void draw_app_selector(GuiState &gui, HostState &host) {
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    const auto scal = ImVec2(display_size.x / 960.0f, display_size.y / 544.0f);

    ImGui::SetNextWindowPos(ImVec2(0, MENUBAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(display_size.x, display_size.y - MENUBAR_HEIGHT), ImGuiCond_Always);
    if (!gui.theme_backgrounds.empty() || !gui.user_backgrounds.empty())
        ImGui::SetNextWindowBgAlpha(host.cfg.background_alpha);
    ImGui::Begin("##app_selector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

    if (gui.start_background && !gui.file_menu.pkg_install_dialog) {
        if (ImGui::IsAnyWindowHovered() || ImGui::IsAnyItemActive() || ImGui::IsAnyItemHovered())
            last_time["start"] = 0;
        else {
            if (last_time["start"] == 0)
                last_time["start"] = current_time();

            while (last_time["start"] + host.cfg.delay_start < current_time()) {
                last_time["start"] += host.cfg.delay_start;
                gui.theme.start_screen = true;
            }
        }
    }

    if ((!gui.theme_backgrounds.empty() || !gui.user_backgrounds.empty()) && ImGui::IsWindowHovered()) {
        if (last_time["home"] == 0)
            last_time["home"] = current_time();

        while (last_time["home"] + host.cfg.delay_background < current_time()) {
            last_time["home"] += host.cfg.delay_background;

            if (host.cfg.use_theme_background) {
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

    if (host.cfg.use_theme_background && !gui.theme_backgrounds.empty())
        ImGui::GetBackgroundDrawList()->AddImage(gui.theme_backgrounds[gui.current_theme_bg], ImVec2(0.f, MENUBAR_HEIGHT), display_size);
    else if (!gui.user_backgrounds.empty())
        ImGui::GetBackgroundDrawList()->AddImage(gui.user_backgrounds[host.cfg.user_backgrounds[gui.current_user_bg]], ImVec2(0.f, MENUBAR_HEIGHT), display_size);

    if (gui.delete_app_icon) {
        if (gui.app_selector.icons.find(host.io.title_id) != gui.app_selector.icons.end())
            gui.app_selector.icons.erase(host.io.title_id);
        gui.delete_app_icon = false;
    }

    const float icon_size = static_cast<float>(host.cfg.icon_size);

    switch (gui.app_selector.state) {
    case SELECT_APP:
        ImGui::SetWindowFontScale(1.1f);
        std::string title_id_label = "Title ID";
        float title_id_size = (ImGui::CalcTextSize(title_id_label.c_str()).x + 50.f) * scal.x;
        std::string app_ver_label = "Version";
        float app_ver_size = (ImGui::CalcTextSize(app_ver_label.c_str()).x + 30.f) * scal.x;
        std::string cateogry_label = "Category";
        float cateogry_size = (ImGui::CalcTextSize(cateogry_label.c_str()).x + 30.f) * scal.x;
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
        if (!host.cfg.apps_list_grid) {
            ImGui::Columns(5);
            ImGui::SetColumnWidth(0, icon_size + /* padding */ 20.f);
            ImGui::NextColumn();
            switch (gui.app_selector.title_id_sort_state) {
            case ASCENDANT:
                title_id_label += " >";
                break;
            case DESCENDANT:
                title_id_label += " <";
                break;
            }
            ImGui::SetColumnWidth(1, title_id_size);
            if (ImGui::Button(title_id_label.c_str())) {
                gui.app_selector.title_id_sort_state = static_cast<gui::SortState>(std::max<int>(1, (gui.app_selector.title_id_sort_state + 1) % 3));
                gui.app_selector.app_ver_sort_state = NOT_SORTED;
                gui.app_selector.category_sort_state = NOT_SORTED;
                gui.app_selector.title_sort_state = NOT_SORTED;
                switch (gui.app_selector.title_id_sort_state) {
                case ASCENDANT:
                    std::sort(gui.app_selector.apps.begin(), gui.app_selector.apps.end(), [](const App &lhs, const App &rhs) {
                        return lhs.title_id < rhs.title_id;
                    });
                    break;
                case DESCENDANT:
                    std::sort(gui.app_selector.apps.begin(), gui.app_selector.apps.end(), [](const App &lhs, const App &rhs) {
                        return lhs.title_id > rhs.title_id;
                    });
                    break;
                default:
                    break;
                }
            }
            ImGui::NextColumn();
            switch (gui.app_selector.app_ver_sort_state) {
            case ASCENDANT:
                app_ver_label += " >";
                app_ver_size += ImGui::CalcTextSize(" >").x;
                break;
            case DESCENDANT:
                app_ver_label += " <";
                app_ver_size += ImGui::CalcTextSize(" <").x;
                break;
            }
            ImGui::SetColumnWidth(2, app_ver_size);
            if (ImGui::Button(app_ver_label.c_str())) {
                gui.app_selector.title_id_sort_state = NOT_SORTED;
                gui.app_selector.app_ver_sort_state = static_cast<gui::SortState>(std::max<int>(1, (gui.app_selector.app_ver_sort_state + 1) % 3));
                gui.app_selector.category_sort_state = NOT_SORTED;
                gui.app_selector.title_sort_state = NOT_SORTED;
                switch (gui.app_selector.app_ver_sort_state) {
                case ASCENDANT:
                    std::sort(gui.app_selector.apps.begin(), gui.app_selector.apps.end(), [](const App &lhs, const App &rhs) {
                        return lhs.app_ver < rhs.app_ver;
                    });
                    break;
                case DESCENDANT:
                    std::sort(gui.app_selector.apps.begin(), gui.app_selector.apps.end(), [](const App &lhs, const App &rhs) {
                        return lhs.app_ver > rhs.app_ver;
                    });
                    break;
                default:
                    break;
                }
            }
            ImGui::NextColumn();
            switch (gui.app_selector.category_sort_state) {
            case ASCENDANT:
                cateogry_label += " >";
                cateogry_size += ImGui::CalcTextSize(" >").x;
                break;
            case DESCENDANT:
                cateogry_label += " <";
                cateogry_size += ImGui::CalcTextSize(" <").x;
                break;
            }
            ImGui::SetColumnWidth(3, cateogry_size);
            if (ImGui::Button(cateogry_label.c_str())) {
                gui.app_selector.title_id_sort_state = NOT_SORTED;
                gui.app_selector.app_ver_sort_state = NOT_SORTED;
                gui.app_selector.category_sort_state = static_cast<gui::SortState>(std::max<int>(1, (gui.app_selector.category_sort_state + 1) % 3));
                gui.app_selector.title_sort_state = NOT_SORTED;
                switch (gui.app_selector.category_sort_state) {
                case ASCENDANT:
                    std::sort(gui.app_selector.apps.begin(), gui.app_selector.apps.end(), [](const App &lhs, const App &rhs) {
                        return lhs.category < rhs.category;
                    });
                    break;
                case DESCENDANT:
                    std::sort(gui.app_selector.apps.begin(), gui.app_selector.apps.end(), [](const App &lhs, const App &rhs) {
                        return lhs.category > rhs.category;
                    });
                    break;
                default:
                    break;
                }
            }
            ImGui::NextColumn();
        }
        std::string title_label = "Title";
        switch (gui.app_selector.title_sort_state) {
        case ASCENDANT:
            title_label += " >";
            break;
        case DESCENDANT:
            title_label += " <";
            break;
        default:
            break;
        }
        if (ImGui::Button(title_label.c_str()) || !gui.app_selector.is_app_list_sorted) {
            gui.app_selector.title_id_sort_state = NOT_SORTED;
            gui.app_selector.app_ver_sort_state = NOT_SORTED;
            gui.app_selector.category_sort_state = NOT_SORTED;
            gui.app_selector.title_sort_state = static_cast<gui::SortState>(std::max<int>(1, (gui.app_selector.title_sort_state + 1) % 3));
            gui.app_selector.is_app_list_sorted = true;
            switch (gui.app_selector.title_sort_state) {
            case ASCENDANT:
                std::sort(gui.app_selector.apps.begin(), gui.app_selector.apps.end(), [](const App &lhs, const App &rhs) {
                    return string_utils::toupper(lhs.title) < string_utils::toupper(rhs.title);
                });
                break;
            case DESCENDANT:
                std::sort(gui.app_selector.apps.begin(), gui.app_selector.apps.end(), [](const App &lhs, const App &rhs) {
                    return string_utils::toupper(lhs.title) > string_utils::toupper(rhs.title);
                });
                break;
            default:
                break;
            }
        }
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_SEARCH_BAR_TEXT);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, GUI_COLOR_SEARCH_BAR_BG);
        ImGui::SameLine(ImGui::GetColumnWidth() - (ImGui::CalcTextSize("Refresh").x + ImGui::GetStyle().DisplayWindowPadding.x + 270));
        if (ImGui::Button("Refresh"))
            refresh_app_list(gui, host);
        ImGui::PopStyleColor(3);
        ImGui::SameLine(ImGui::GetColumnWidth() - (ImGui::CalcTextSize("Search").x + ImGui::GetStyle().DisplayWindowPadding.x + 180));
        ImGui::TextColored(GUI_COLOR_TEXT, "Search");
        ImGui::SameLine();
        gui.app_search_bar.Draw("##app_search_bar", 180.f);
        if (!host.cfg.apps_list_grid) {
            ImGui::NextColumn();
            ImGui::Columns(1);
        }
        ImGui::Separator();
        static const auto POS_APP_LIST = ImVec2(54.f, 70.f);
        ImGui::SetNextWindowPos(host.cfg.apps_list_grid ? POS_APP_LIST : ImVec2(1.f, 68.f), ImGuiCond_Always);
        ImGui::BeginChild("##apps_list", ImVec2(host.cfg.apps_list_grid ? display_size.x - POS_APP_LIST.x : display_size.x, display_size.y - POS_APP_LIST.y), false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        const auto GRID_ICON_SIZE = ImVec2(128.f * scal.x, 128.f * scal.y);
        if (!host.cfg.apps_list_grid) {
            ImGui::Columns(5, nullptr, true);
            ImGui::SetColumnWidth(0, icon_size + /* padding */ 20.f);
            ImGui::SetColumnWidth(1, title_id_size);
            ImGui::SetColumnWidth(2, app_ver_size);
            ImGui::SetColumnWidth(3, cateogry_size);
        } else {
            ImGui::Columns(4, nullptr, false);
            const auto COLUMN_SIZE = GRID_ICON_SIZE.x + (80.f * scal.x);
            ImGui::SetColumnWidth(0, COLUMN_SIZE);
            ImGui::SetColumnWidth(1, COLUMN_SIZE);
            ImGui::SetColumnWidth(2, COLUMN_SIZE);
            ImGui::SetColumnWidth(3, COLUMN_SIZE);
        }
        ImGui::SetWindowFontScale(!gui.live_area_font_data.empty() ? 0.82f * scal.x : 1.f);
        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
        for (const auto &app : gui.app_selector.apps) {
            bool selected = false;
            if (!gui.app_search_bar.PassFilter(app.title.c_str()) && !gui.app_search_bar.PassFilter(app.title_id.c_str()))
                continue;
            if (!fs::exists(fs::path(host.pref_path) / "ux0/app" / app.title_id)) {
                host.io.title_id = app.title_id;
                LOG_ERROR("Application not found: {} [{}], deleting the entry for it.", app.title_id, app.title);
                delete_app(gui, host);
            }
            const auto POS_ICON = ImGui::GetCursorPosY();
            if (gui.app_selector.icons.find(app.title_id) != gui.app_selector.icons.end()) {
                if (host.cfg.apps_list_grid)
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ((ImGui::GetColumnWidth() / 2.f) - (GRID_ICON_SIZE.x / 2.f) - 10.f));
                ImGui::Image(gui.app_selector.icons[app.title_id], host.cfg.apps_list_grid ? GRID_ICON_SIZE : ImVec2(icon_size, icon_size));
            }
            const auto POS_STITLE = ImVec2(ImGui::GetCursorPosX() + (30.f * scal.x), ImGui::GetCursorPosY());
            ImGui::SetCursorPosY(POS_ICON);
            if (host.cfg.apps_list_grid)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ((ImGui::GetColumnWidth() / 2.f) - (GRID_ICON_SIZE.x / 2.f) - 10.f));
            else
                ImGui::SetCursorPosY(POS_ICON);
            ImGui::PushID(app.title_id.c_str());
            ImGui::Selectable("##icon", &selected, host.cfg.apps_list_grid ? ImGuiSelectableFlags_None : ImGuiSelectableFlags_SpanAllColumns, host.cfg.apps_list_grid ? GRID_ICON_SIZE : ImVec2(0.f, icon_size));
            ImGui::PopID();
            if (ImGui::IsItemHovered()) {
                host.app_version = app.app_ver;
                host.app_short_title = app.stitle;
                host.app_title = app.title;
                host.io.title_id = app.title_id;
            }
            if (host.io.title_id == app.title_id)
                draw_app_context_menu(gui, host);
            if (!host.cfg.apps_list_grid) {
                ImGui::NextColumn();
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                ImGui::Selectable(app.title_id.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, icon_size));
                ImGui::NextColumn();
                ImGui::Selectable(app.app_ver.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, icon_size));
                ImGui::NextColumn();
                ImGui::Selectable(app.category.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, icon_size));
                ImGui::NextColumn();
                ImGui::Selectable(app.title.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, icon_size));
                ImGui::PopStyleVar();
                ImGui::NextColumn();
            } else {
                ImGui::SetCursorPos(ImVec2(POS_STITLE.x + ((GRID_ICON_SIZE.x / 2.f) - (ImGui::CalcTextSize(app.stitle.c_str(), 0, false, GRID_ICON_SIZE.x).x / 2.f)), POS_STITLE.y));
                ImGui::PushTextWrapPos(POS_STITLE.x + GRID_ICON_SIZE.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", app.title.c_str());
                ImGui::PopTextWrapPos();
                ImGui::NextColumn();
            }
            if (selected) {
                if (host.cfg.show_live_area_screen) {
                    init_live_area(gui, host);
                    gui.live_area.live_area_screen = true;
                } else
                    gui.app_selector.selected_title_id = app.title_id;
            }
        }
        ImGui::PopStyleColor();
        ImGui::Columns(1);
        ImGui::EndChild();
        break;
    }
    ImGui::End();
}

} // namespace gui
