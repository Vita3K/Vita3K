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

#include <boost/algorithm/string/trim.hpp>

#include <gui/functions.h>

#include <host/functions.h>

#include <io/VitaIoDevice.h>
#include <io/functions.h>

namespace gui {

void get_app_info(GuiState &gui, HostState &host, const std::string &title_id) {
    const auto APP_PATH{ fs::path(host.pref_path) / "ux0/app" / title_id };
    gui.app_selector.app_info = {};

    if (fs::exists(APP_PATH) && !fs::is_empty(APP_PATH)) {
        gui.app_selector.app_info.trophy = fs::exists(APP_PATH / "sce_sys/trophy") ? "Eligible" : "Ineligible";

        const auto last_writed = fs::last_write_time(APP_PATH);
        const auto updated = std::localtime(&last_writed);
        gui.app_selector.app_info.updated = fmt::format("{}/{}/{} {:0>2d}:{:0>2d}", updated->tm_mday, updated->tm_mon + 1, updated->tm_year + 1900, updated->tm_hour, updated->tm_min);
    }
}

size_t get_app_size(HostState &host, const std::string &title_id) {
    const auto APP_PATH{ fs::path(host.pref_path) / "ux0/app" / title_id };
    size_t app_size = 0;
    if (fs::exists(APP_PATH) && !fs::is_empty(APP_PATH)) {
        for (const auto &app : fs::recursive_directory_iterator(APP_PATH)) {
            if (fs::is_regular_file(app.path()))
                app_size += fs::file_size(app.path());
        }
    }
    const auto DLC_PATH{ fs::path(host.pref_path) / "ux0/addcont" / title_id };
    if (fs::exists(DLC_PATH) && !fs::is_empty(DLC_PATH)) {
        for (const auto &dlc : fs::recursive_directory_iterator(DLC_PATH)) {
            if (fs::is_regular_file(dlc.path()))
                app_size += fs::file_size(dlc.path());
        }
    }

    return app_size;
}

std::string get_unit_size(const size_t &size) {
    std::string size_str;
    if (size >= GB(10))
        size_str = std::to_string(size / GB(1)) + " GB";
    else if (size >= MB(10))
        size_str = std::to_string(size / MB(1)) + " MB";
    else if (size >= KB(1))
        size_str = std::to_string(size / KB(1)) + " KB";
    else
        size_str = std::to_string(size) + " B";

    return size_str;
}

struct SaveData {
    std::string title;
    std::string title_id;
    size_t size;
    std::string date;
};

static std::vector<SaveData> save_data_list;

static void get_save_data_list(GuiState &gui, HostState &host) {
    host.io.user_id = fmt::format("{:0>2d}", host.cfg.user_id);
    save_data_list.clear();

    fs::path SAVE_PATH{ fs::path{ host.pref_path } / "ux0/user" / host.io.user_id / "savedata" };
    if (!fs::exists(SAVE_PATH))
        return;

    for (const auto &save : fs::directory_iterator(SAVE_PATH)) {
        const auto title_id = save.path().stem().generic_string();
        if (fs::is_directory(save.path()) && !fs::is_empty(save.path()) && get_app_index(gui, title_id) != gui.app_selector.user_apps.end()) {
            const auto last_writed = fs::last_write_time(save);
            const auto updated = std::localtime(&last_writed);
            const auto date = fmt::format("{}/{}/{} {:0>2d}:{:0>2d}", updated->tm_mday, updated->tm_mon + 1, updated->tm_year + 1900, updated->tm_hour, updated->tm_min);

            size_t size = 0;
            for (const auto &save_path : fs::recursive_directory_iterator(save)) {
                if (fs::is_regular_file(save_path.path()))
                    size += fs::file_size(save_path.path());
            }

            save_data_list.push_back({ get_app_index(gui, title_id)->title, title_id, size, date });
        }
    }
    std::sort(save_data_list.begin(), save_data_list.end(), [](const SaveData &sa, const SaveData &sb) {
        return sa.title < sb.title;
    });
}

static std::map<std::string, size_t> apps_size;
static std::map<std::string, std::string> space;

void get_contents_size(GuiState &gui, HostState &host) {
    space.clear();

    size_t free_size = 0;
    free_size = fs::space(host.pref_path).free;
    space["free"] = get_unit_size(free_size);

    size_t apps_list_size = 0;
    for (const auto &app : gui.app_selector.user_apps) {
        apps_size[app.title_id] = get_app_size(host, app.title_id);
        apps_list_size += apps_size[app.title_id];
    }
    space["app"] = apps_list_size ? get_unit_size(apps_list_size) : "-";

    get_save_data_list(gui, host);
    size_t save_data_list_size = 0;
    for (const auto &save : save_data_list) {
        save_data_list_size += save.size;
    }
    space["savedata"] = save_data_list_size ? get_unit_size(save_data_list_size) : "-";

    const auto THEME_PATH{ fs::path(host.pref_path) / "ux0/theme" };
    size_t themes_list_size = 0;
    if (fs::exists(THEME_PATH) && !fs::is_empty(THEME_PATH)) {
        for (const auto &themes_list : fs::directory_iterator(THEME_PATH)) {
            for (const auto &theme : fs::recursive_directory_iterator(themes_list)) {
                if (fs::is_regular_file(theme.path()))
                    themes_list_size += fs::file_size(theme.path());
            }
        }
    }
    space["themes"] = themes_list_size ? get_unit_size(themes_list_size) : "-";
}

static std::map<std::string, bool> contents_selected;
static std::string app_selected, size_selected_contents, menu, title;

static bool get_size_selected_contents(GuiState &gui, HostState &host) {
    size_selected_contents.clear();
    size_t contents_size = 0;
    for (const auto &content : contents_selected) {
        if (content.second) {
            if (menu == "app")
                contents_size += apps_size[content.first];
            else {
                const auto save_index = std::find_if(save_data_list.begin(), save_data_list.end(), [&](const SaveData &s) {
                    return s.title_id == content.first;
                });
                contents_size += save_index->size;
            }
        }
    }
    size_selected_contents = get_unit_size(contents_size);

    return contents_size;
}

struct Dlc {
    std::string name;
    std::string size;
    std::string date;
};

static std::map<std::string, Dlc> dlc_info;

static void get_content_info(GuiState &gui, HostState &host) {
    const auto APP_PATH{ fs::path(host.pref_path) / "ux0/app" / app_selected };
    if (fs::exists(APP_PATH) && !fs::is_empty(APP_PATH)) {
        size_t app_size = 0;
        for (const auto &app : fs::recursive_directory_iterator(APP_PATH)) {
            if (fs::is_regular_file(app.path()))
                app_size += fs::file_size(app.path());
        }
        gui.app_selector.app_info.size = app_size;
    }

    dlc_info.clear();
    const auto DLC_PATH{ fs::path(host.pref_path) / "ux0/addcont" / app_selected };
    if (fs::exists(DLC_PATH) && !fs::is_empty(DLC_PATH)) {
        for (const auto &dlc : fs::directory_iterator(DLC_PATH)) {
            const auto content_id = dlc.path().stem().string();

            const auto last_writed = fs::last_write_time(dlc);
            const auto updated = std::localtime(&last_writed);

            dlc_info[content_id].date = fmt::format("{}/{}/{} {:0>2d}:{:0>2d}", updated->tm_mday, updated->tm_mon + 1, updated->tm_year + 1900, updated->tm_hour, updated->tm_min);

            size_t dlc_size = 0;
            for (const auto &content : fs::recursive_directory_iterator(dlc)) {
                if (fs::is_regular_file(content.path()))
                    dlc_size += fs::file_size(content.path());
            }
            dlc_info[content_id].size = get_unit_size(dlc_size);

            const auto content_path{ fs::path("addcont") / app_selected / content_id };
            vfs::FileBuffer params;
            if (vfs::read_file(VitaIoDevice::ux0, params, host.pref_path, content_path.string() + "/sce_sys/param.sfo")) {
                SfoFile sfo_handle;
                sfo::load(sfo_handle, params);
                if (!sfo::get_data_by_key(dlc_info[content_id].name, sfo_handle, fmt::format("TITLE_{:0>2d}", host.cfg.sys_lang)))
                    sfo::get_data_by_key(dlc_info[content_id].name, sfo_handle, "TITLE");
            }
            boost::trim(dlc_info[content_id].name);
        }
    }
}

static bool popup, content_delete, set_scroll_pos;
static float scroll_pos;
static ImGuiTextFilter search_bar;

void draw_content_manager(GuiState &gui, HostState &host) {
    draw_information_bar(gui);

    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto SCAL = ImVec2(display_size.x / 960.0f, display_size.y / 544.0f);
    const auto MENUBAR_HEIGHT = 32.f * SCAL.y;

    const auto SIZE_ICON_LIST = ImVec2(60.f * SCAL.x, 60.f * SCAL.y);
    const auto SIZE_ICON_DETAIL = ImVec2(70.f * SCAL.x, 70.f * SCAL.y);

    const auto BUTTON_SIZE = ImVec2(310.f * SCAL.x, 46.f * SCAL.y);

    const auto WINDOW_SIZE = ImVec2(display_size.x, display_size.y - MENUBAR_HEIGHT);
    const auto SIZE_LIST = ImVec2(820 * SCAL.x, 378.f * SCAL.y);
    const auto SIZE_INFO = ImVec2(780 * SCAL.x, 402.f * SCAL.y);

    const auto POPUP_SIZE = ImVec2(756.0f * SCAL.x, 436.0f * SCAL.y);

    ImGui::SetNextWindowPos(ImVec2(0, MENUBAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);

    const auto is_founded = gui.apps_background.find(host.io.title_id) != gui.apps_background.end();
    if (!is_founded)
        ImGui::SetNextWindowBgAlpha(0.999f);
    ImGui::Begin("##content_manager", &gui.live_area.content_manager, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    if (is_founded)
        ImGui::GetWindowDrawList()->AddImage(gui.apps_background[host.io.title_id], ImVec2(0.f, MENUBAR_HEIGHT), display_size);

    ImGui::SetWindowFontScale(1.5f * SCAL.x);

    if (menu == "info") {
        ImGui::SetCursorPos(ImVec2(90.f * SCAL.x, 10.f * SCAL.y));
        ImGui::Image(gui.app_selector.icons[app_selected], SIZE_ICON_DETAIL);
        const auto CALC_NAME = ImGui::CalcTextSize(get_app_index(gui, app_selected)->title.c_str(), nullptr, false, SIZE_INFO.x - SIZE_ICON_DETAIL.x).y / 2.f;
        ImGui::SetCursorPos(ImVec2((110.f * SCAL.x) + SIZE_ICON_DETAIL.x, (SIZE_ICON_DETAIL.y / 2.f) - CALC_NAME + (10.f * SCAL.y)));
        ImGui::PushTextWrapPos(SIZE_INFO.x);
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", get_app_index(gui, app_selected)->title.c_str());
        ImGui::PopTextWrapPos();
    } else {
        const auto content_str = ImGui::CalcTextSize(title.c_str(), 0, false, SIZE_LIST.x);
        ImGui::PushTextWrapPos(((display_size.x - SIZE_LIST.x) / 2.f) + SIZE_LIST.x);
        ImGui::SetCursorPos(ImVec2((display_size.x / 2.f) - (content_str.x / 2.f), (32.f * SCAL.y) - (content_str.y / 2.f)));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", title.c_str());
        ImGui::PopTextWrapPos();
        if (!menu.empty()) {
            // Search Bar
            const auto search_size = ImGui::CalcTextSize("Search");
            ImGui::SetCursorPos(ImVec2(20.f * SCAL.y, (32.f * SCAL.y) - (search_size.y / 2.f)));
            ImGui::TextColored(GUI_COLOR_TEXT, "Search");
            ImGui::SameLine();
            search_bar.Draw("##search_bar", 200);

            // Free Space
            const auto scal_font = 19.2f / ImGui::GetFontSize();
            ImGui::GetWindowDrawList()->AddText(gui.live_area_font, 19.2f * SCAL.x, ImVec2((display_size.x - ((ImGui::CalcTextSize("Free Space").x * scal_font)) * SCAL.x) - (15.f * SCAL.x), 42.f * SCAL.y),
                4294967295, "Free Space");
            ImGui::GetWindowDrawList()->AddText(gui.live_area_font, 19.2f * SCAL.x, ImVec2((display_size.x - ((ImGui::CalcTextSize(space["free"].c_str()).x * scal_font)) * SCAL.x) - (15.f * SCAL.x), 68.f * SCAL.y),
                4294967295, space["free"].c_str());
        }
        ImGui::SetCursorPosY(64.0f * SCAL.y);
        ImGui::Separator();
    }

    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, (menu == "info" ? 130.f : 102.0f) * SCAL.y), ImGuiCond_Always, ImVec2(0.5f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
    ImGui::BeginChild("##content_manager_child", menu == "info" ? SIZE_INFO : SIZE_LIST, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    if (menu.empty()) {
        title = "Content Manager";
        ImGui::SetWindowFontScale(1.2f);
        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, 630.f * SCAL.x);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
        const auto SIZE_SELECT = 80.f * SCAL.y;
        if (ImGui::Selectable("Applications", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT)))
            menu = "app";
        ImGui::NextColumn();
        ImGui::SetWindowFontScale(0.8f);
        ImGui::Selectable(space["app"].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT));
        ImGui::NextColumn();
        ImGui::Separator();
        ImGui::SetWindowFontScale(1.2f);
        if (ImGui::Selectable("Saved Data", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT)))
            menu = "save";
        ImGui::NextColumn();
        ImGui::SetWindowFontScale(0.8f);
        ImGui::Selectable(space["savedata"].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT));
        ImGui::NextColumn();
        ImGui::Separator();
        ImGui::SetWindowFontScale(1.2f);
        if (ImGui::Selectable("Themes", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT))) {
            host.io.title_id = "NPXS10015";
            pre_run_app(gui, host);
        }
        ImGui::NextColumn();
        ImGui::SetWindowFontScale(0.8f);
        ImGui::Selectable(space["themes"].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT));
        ImGui::NextColumn();
        ImGui::PopStyleVar();
        ImGui::Separator();
        ImGui::SetWindowFontScale(1.2f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (25.f * SCAL.y));
        ImGui::TextColored(GUI_COLOR_TEXT, "Free Space");
        ImGui::NextColumn();
        ImGui::SetWindowFontScale(0.8f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (25.f * SCAL.y));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", space["free"].c_str());
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (25.f * SCAL.y));
        ImGui::SetWindowFontScale(1.2f);
        ImGui::Separator();
        ImGui::NextColumn();
        ImGui::Columns(1);
    } else {
        if (content_delete) {
            for (const auto &content : contents_selected) {
                if (content.second) {
                    if (menu == "app") {
                        const auto APP_PATH{ fs::path(host.pref_path) / "ux0/app" / content.first };
                        if (fs::exists(APP_PATH) && !fs::is_empty(APP_PATH))
                            fs::remove_all(APP_PATH);
                        const auto DLC_PATH{ fs::path(host.pref_path) / "ux0/addcont" / content.first };
                        if (fs::exists(DLC_PATH) && !fs::is_empty(DLC_PATH))
                            fs::remove_all(DLC_PATH);

                        gui.app_selector.user_apps.erase(get_app_index(gui, content.first));
                        gui.app_selector.icons.erase(content.first);
                    }
                    const auto SAVE_PATH{ fs::path(host.pref_path) / "ux0/user" / host.io.user_id / "savedata" / content.first };
                    if (fs::exists(SAVE_PATH) && !fs::is_empty(SAVE_PATH))
                        fs::remove_all(SAVE_PATH);
                }
            }
            get_contents_size(gui, host);
            contents_selected.clear();
            content_delete = false;
        }
        if (popup) {
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
            ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
            ImGui::Begin("##app_delete", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            ImGui::SetNextWindowBgAlpha(0.999f);
            ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f);
            ImGui::BeginChild("##app_delete_child", POPUP_SIZE, true, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
            ImGui::SetWindowFontScale(1.6f * SCAL.x);
            ImGui::SetCursorPos(ImVec2(52.f * SCAL.x, 80.f * SCAL.y));
            ImGui::PushTextWrapPos(POPUP_SIZE.x);
            ImGui::TextColored(GUI_COLOR_TEXT, menu == "app" ? "The selected applications and all related data, including saved data, will be deleted." : "The selected saved data items wii be deleted");
            ImGui::PopTextWrapPos();
            ImGui::SetCursorPos(ImVec2(106.f * SCAL.x, ImGui::GetCursorPosY() + (76.f * SCAL.y)));
            ImGui::TextColored(GUI_COLOR_TEXT, "Data to be Deleted: %s", size_selected_contents.c_str());
            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2) - (BUTTON_SIZE.x + (10.f * SCAL.x)), POPUP_SIZE.y - BUTTON_SIZE.y - (22.0f * SCAL.y)));
            if (ImGui::Button("Cancel", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_circle)) {
                popup = false;
            }
            ImGui::SameLine(0, 20.f * SCAL.x);
            if (ImGui::Button("Ok", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross)) {
                content_delete = true;
                popup = false;
            }
            ImGui::PopStyleVar(2);
            ImGui::EndChild();
            ImGui::End();
        }

        // Apps Menu
        if (menu == "app") {
            title = "Applications";
            if (gui.app_selector.user_apps.empty()) {
                ImGui::SetWindowFontScale(1.2f);
                const auto calc_text = ImGui::CalcTextSize("There are no content items.");
                ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) - (calc_text.x / 2.f), (SIZE_LIST.y / 2.f) - (calc_text.y / 2.f)));
                ImGui::TextColored(GUI_COLOR_TEXT, "There are no content items.");
            } else {
                // Set Scroll Pos
                if (set_scroll_pos) {
                    ImGui::SetScrollY(scroll_pos);
                    set_scroll_pos = false;
                }

                ImGui::Columns(4, nullptr, false);
                ImGui::SetColumnWidth(0, 60 * SCAL.x);
                ImGui::SetColumnWidth(1, 75 * SCAL.x);
                ImGui::SetColumnWidth(2, 580.f * SCAL.x);
                for (const auto &app : gui.app_selector.user_apps) {
                    if (!search_bar.PassFilter(app.title.c_str()) && !search_bar.PassFilter(app.stitle.c_str()) && !search_bar.PassFilter(app.title_id.c_str()))
                        continue;
                    ImGui::PushID(app.title_id.c_str());
                    ImGui::SetWindowFontScale(1.32f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (15.f * SCAL.y));
                    ImGui::Checkbox("##selected", &contents_selected[app.title_id]);
                    ImGui::NextColumn();
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (8.f * SCAL.y));
                    ImGui::Image(gui.app_selector.icons[app.title_id], SIZE_ICON_LIST);
                    ImGui::NextColumn();
                    const auto Title_POS = ImGui::GetCursorPosY();
                    ImGui::SetWindowFontScale(1.1f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (4.f * SCAL.y));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", app.title.c_str());
                    ImGui::SetCursorPosY(Title_POS + (46.f * SCAL.y));
                    ImGui::SetWindowFontScale(0.8f);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", get_unit_size(apps_size[app.title_id]).c_str());
                    ImGui::NextColumn();
                    ImGui::SetWindowFontScale(1.2f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (15.f * SCAL.y));
                    if (ImGui::Button("i", ImVec2(45.f * SCAL.x, 45.f * SCAL.y))) {
                        scroll_pos = ImGui::GetScrollY();
                        ImGui::SetScrollY(0.f);
                        app_selected = app.title_id;
                        get_app_info(gui, host, app_selected);
                        get_content_info(gui, host);
                        menu = "info";
                    }
                    ImGui::PopID();
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (15.f * SCAL.y));
                    ImGui::Separator();
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);
            }
            // Save Data Menu
        } else if (menu == "save") {
            title = "Saved Data";
            if (save_data_list.empty()) {
                ImGui::SetWindowFontScale(1.2f);
                const auto calc_text = ImGui::CalcTextSize("There is no saved data.");
                ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) - (calc_text.x / 2.f), (SIZE_LIST.y / 2.f) - (calc_text.y / 2.f)));
                ImGui::TextColored(GUI_COLOR_TEXT, "There is no saved data.");
            } else {
                ImGui::Columns(3, nullptr, false);
                ImGui::SetColumnWidth(0, 60 * SCAL.x);
                ImGui::SetColumnWidth(1, 75 * SCAL.x);
                for (const auto &save : save_data_list) {
                    if (!search_bar.PassFilter(save.title.c_str()) && !search_bar.PassFilter(save.title_id.c_str()))
                        continue;
                    ImGui::PushID(save.title_id.c_str());
                    ImGui::SetWindowFontScale(1.32f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (15.f * SCAL.y));
                    ImGui::Checkbox("##selected", &contents_selected[save.title_id]);
                    ImGui::NextColumn();
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (8.f * SCAL.y));
                    ImGui::Image(gui.app_selector.icons[save.title_id], SIZE_ICON_LIST);
                    ImGui::NextColumn();
                    const auto Title_POS = ImGui::GetCursorPosY();
                    ImGui::SetWindowFontScale(1.1f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (4.f * SCAL.y));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", save.title.c_str());
                    ImGui::SetWindowFontScale(0.8f);
                    ImGui::SetCursorPosY(Title_POS + (46.f * SCAL.y));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", save.date.c_str(), get_unit_size(save.size).c_str());
                    ImGui::PopID();
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (6.f * SCAL.y));
                    ImGui::Separator();
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);
            }
        } else if (menu == "info") {
            // Information
            ImGui::SetWindowFontScale(1.f);
            ImGui::TextColored(GUI_COLOR_TEXT, "Trophy Earning");
            ImGui::SameLine(280.f * SCAL.x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", gui.app_selector.app_info.trophy.c_str());
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (35.f * SCAL.y));
            ImGui::TextColored(GUI_COLOR_TEXT, "Parental Controls");
            ImGui::SameLine(280.f * SCAL.x);
            ImGui::TextColored(GUI_COLOR_TEXT, "Level %d", *reinterpret_cast<const uint16_t *>(get_app_index(gui, app_selected)->parental_level.c_str()));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (35.f * SCAL.y));
            ImGui::TextColored(GUI_COLOR_TEXT, "Updated");
            ImGui::SameLine(280.f * SCAL.x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", gui.app_selector.app_info.updated.c_str());
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (35.f * SCAL.y));
            ImGui::TextColored(GUI_COLOR_TEXT, "Size");
            ImGui::SameLine(280.f * SCAL.x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", get_unit_size(gui.app_selector.app_info.size).c_str());
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (35.f * SCAL.y));
            ImGui::TextColored(GUI_COLOR_TEXT, "Version");
            ImGui::SameLine(280.f * SCAL.x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", get_app_index(gui, app_selected)->app_ver.c_str());
            for (const auto &dlc : dlc_info) {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (40.f * SCAL.y));
                ImGui::Separator();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (40.f * SCAL.y));
                ImGui::SetWindowFontScale(1.2f);
                ImGui::TextColored(GUI_COLOR_TEXT, "+");
                ImGui::SameLine(0, 30.f * SCAL.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", dlc.second.name.c_str());
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (35.f * SCAL.y));
                ImGui::SetWindowFontScale(1.f);
                ImGui::TextColored(GUI_COLOR_TEXT, "Updated");
                ImGui::SameLine(280.f * SCAL.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", dlc.second.date.c_str());
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (35.f * SCAL.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "Size");
                ImGui::SameLine(280.f * SCAL.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", dlc.second.size.c_str());
            }
        }
    }

    ImGui::EndChild();

    ImGui::SetWindowFontScale(1.2f * SCAL.x);
    ImGui::SetCursorPos(ImVec2(10.f, display_size.y - (88.f * SCAL.y)));
    const auto is_empty = ((menu == "app") && gui.app_selector.user_apps.empty()) || ((menu == "save") && save_data_list.empty());
    if (menu.empty() || (menu == "info") || is_empty) {
        if (ImGui::Button("Back", ImVec2(64.f * SCAL.x, 40.f * SCAL.y))) {
            if (!menu.empty()) {
                if (menu == "info") {
                    menu = "app";
                    set_scroll_pos = true;
                } else
                    menu.clear();
            } else
                gui.live_area.content_manager = false;
        }
    } else {
        ImGui::SetWindowFontScale(1.5f * SCAL.x);
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0.f, 482.f * SCAL.y), display_size, IM_COL32(39.f, 42.f, 49.f, 255.f), 0.f, ImDrawCornerFlags_All);
        if (ImGui::Button("Cancel", ImVec2(202.f * SCAL.x, 44.f * SCAL.y))) {
            if (!menu.empty()) {
                menu.clear();
                contents_selected.clear();
            }
        }
        const auto state = std::find_if(contents_selected.begin(), contents_selected.end(), [&](const auto &c) {
            return !c.second;
        }) != contents_selected.end();
        ImGui::SetCursorPos(ImVec2(display_size.x - (450.f * SCAL.x), display_size.y - (88.f * SCAL.y)));
        if (ImGui::Button(state ? "Select All" : "Clear All", ImVec2(224.f * SCAL.x, 44.f * SCAL.y))) {
            for (auto &content : contents_selected) {
                if (state)
                    content.second = true;
                else
                    content.second = false;
            }
        }
        const auto is_enable = std::find_if(contents_selected.begin(), contents_selected.end(), [&](const auto &cs) {
            return cs.second;
        }) != contents_selected.end();
        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
        if (is_enable ? ImGui::Button("Delete", ImVec2(202.f * SCAL.x, 44.f * SCAL.y)) && get_size_selected_contents(gui, host) : ImGui::Selectable("Delete", false, ImGuiSelectableFlags_Disabled, ImVec2(194.f * SCAL.x, 36.f * SCAL.y)))
            popup = true;
        ImGui::PopStyleVar();
    }
    ImGui::PopStyleVar();
    ImGui::End();
}

} // namespace gui
