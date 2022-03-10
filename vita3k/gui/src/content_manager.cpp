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

#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <boost/range/numeric.hpp>

#include <gui/functions.h>

#include <host/functions.h>

#include <io/VitaIoDevice.h>
#include <io/functions.h>

#include <util/safe_time.h>

namespace gui {
namespace {
template <typename T>
auto get_recursive_directory_size(const T &path) {
    const auto &path_list = fs::recursive_directory_iterator(path);
    const auto pred = [](const auto acc, const auto &app) {
        if (fs::is_regular_file(app.path()))
            return acc + fs::file_size(app.path());
        return acc;
    };
    return boost::accumulate(path_list, boost::uintmax_t{}, pred);
}
} // namespace

void get_app_info(GuiState &gui, HostState &host, const std::string &app_path) {
    const auto APP_PATH{ fs::path(host.pref_path) / "ux0/app" / app_path };
    gui.app_selector.app_info = {};

    if (fs::exists(APP_PATH) && !fs::is_empty(APP_PATH)) {
        auto lang = gui.lang.app_context;
        gui.app_selector.app_info.trophy = fs::exists(APP_PATH / "sce_sys/trophy") ? lang["eligible"] : lang["ineligible"];

        const auto last_writen = fs::last_write_time(APP_PATH);
        SAFE_LOCALTIME(&last_writen, &gui.app_selector.app_info.updated);
    }
}

size_t get_app_size(GuiState &gui, HostState &host, const std::string &app_path) {
    const auto APP_PATH{ fs::path(host.pref_path) / "ux0/app" / app_path };
    boost::uintmax_t app_size = 0;
    if (fs::exists(APP_PATH) && !fs::is_empty(APP_PATH)) {
        app_size += get_recursive_directory_size(APP_PATH);
    }
    const auto ADDCONT_PATH{ fs::path(host.pref_path) / "ux0/addcont" / get_app_index(gui, app_path)->title_id };
    if (fs::exists(ADDCONT_PATH) && !fs::is_empty(ADDCONT_PATH)) {
        app_size += get_recursive_directory_size(ADDCONT_PATH);
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
    tm date;
};

static std::vector<SaveData> save_data_list;

static void get_save_data_list(GuiState &gui, HostState &host) {
    save_data_list.clear();

    fs::path SAVE_PATH{ fs::path{ host.pref_path } / "ux0/user" / host.io.user_id / "savedata" };
    if (!fs::exists(SAVE_PATH))
        return;

    for (const auto &save : fs::directory_iterator(SAVE_PATH)) {
        const auto title_id = save.path().stem().generic_string();
        if (fs::is_directory(save.path()) && !fs::is_empty(save.path()) && get_app_index(gui, title_id) != gui.app_selector.user_apps.end()) {
            tm updated_tm = {};

            const auto last_writen = fs::last_write_time(save);
            SAFE_LOCALTIME(&last_writen, &updated_tm);

            const auto size = get_recursive_directory_size(save);
            save_data_list.push_back({ get_app_index(gui, title_id)->title, title_id, size, updated_tm });
        }
    }
    std::sort(save_data_list.begin(), save_data_list.end(), [](const SaveData &sa, const SaveData &sb) {
        return sa.title < sb.title;
    });
}

static std::map<std::string, size_t> apps_size;
static std::map<std::string, std::string> space;

void init_content_manager(GuiState &gui, HostState &host) {
    space.clear();

    const auto free_size{ fs::space(host.pref_path).free };
    space["free"] = get_unit_size(free_size);

    const auto query_app = [&gui, &host] {
        const auto &directory_list = gui.app_selector.user_apps;
        const auto pred = [&](const auto acc, const auto &app) {
            apps_size[app.path] = get_app_size(gui, host, app.path);
            return acc + apps_size[app.path];
        };
        return boost::accumulate(directory_list, boost::uintmax_t{}, pred);
    };

    const auto query_savedata = [] {
        const auto &directory_list = save_data_list;
        const auto pred = [](const auto acc, const auto &save) { return acc + save.size; };
        return boost::accumulate(directory_list, boost::uintmax_t{}, pred);
    };

    const auto query_themes = [&host] {
        const auto THEME_PATH{ fs::path(host.pref_path) / "ux0/theme" };
        const auto &directory_list = fs::directory_iterator(THEME_PATH);
        const auto pred = [&](const auto acc, const auto &) { return acc + get_recursive_directory_size(THEME_PATH); };
        if (fs::exists(THEME_PATH) && !fs::is_empty(THEME_PATH)) {
            return boost::accumulate(directory_list, boost::uintmax_t{}, pred);
        }
        return boost::uintmax_t{};
    };

    const auto get_list_size_or_dash = [](const auto query) {
        const auto list_size = query();
        return list_size ? get_unit_size(list_size) : "-";
    };

    space["app"] = get_list_size_or_dash(query_app);
    get_save_data_list(gui, host);
    space["savedata"] = get_list_size_or_dash(query_savedata);
    space["themes"] = get_list_size_or_dash(query_themes);
}

static std::map<std::string, bool> contents_selected;
static std::string app_selected, size_selected_contents, menu, title;

static bool get_size_selected_contents(GuiState &gui, HostState &host) {
    size_selected_contents.clear();
    const auto pred = [](const auto acc, const auto &content) {
        if (content.second) {
            if (menu == "app")
                return acc + apps_size[content.first];
            else {
                const auto save_index = std::find_if(save_data_list.begin(), save_data_list.end(), [&](const SaveData &s) {
                    return s.title_id == content.first;
                });
                return acc + save_index->size;
            }
        }
        return acc;
    };
    const auto contents_size = boost::accumulate(contents_selected, size_t{}, pred);
    size_selected_contents = get_unit_size(contents_size);
    return contents_size;
}

struct AddCont {
    std::string name;
    std::string size;
    tm date;
};

static std::map<std::string, AddCont> addcont_info;

static void get_content_info(GuiState &gui, HostState &host) {
    const auto APP_PATH{ fs::path(host.pref_path) / "ux0/app" / app_selected };
    if (fs::exists(APP_PATH) && !fs::is_empty(APP_PATH)) {
        gui.app_selector.app_info.size = get_recursive_directory_size(APP_PATH);
    }

    addcont_info.clear();
    const auto ADDCONT_PATH{ fs::path(host.pref_path) / "ux0/addcont" / app_selected };
    if (fs::exists(ADDCONT_PATH) && !fs::is_empty(ADDCONT_PATH)) {
        for (const auto &addcont : fs::directory_iterator(ADDCONT_PATH)) {
            const auto content_id = addcont.path().stem().string();

            tm updated_tm = {};

            const auto last_writen = fs::last_write_time(addcont);
            SAFE_LOCALTIME(&last_writen, &addcont_info[content_id].date);

            const auto addcont_size = get_recursive_directory_size(addcont);
            addcont_info[content_id].size = get_unit_size(addcont_size);

            const auto content_path{ fs::path("addcont") / app_selected / content_id };
            vfs::FileBuffer params;
            if (vfs::read_file(VitaIoDevice::ux0, params, host.pref_path, content_path.string() + "/sce_sys/param.sfo")) {
                SfoFile sfo_handle;
                sfo::load(sfo_handle, params);
                if (!sfo::get_data_by_key(addcont_info[content_id].name, sfo_handle, fmt::format("TITLE_{:0>2d}", host.cfg.sys_lang)))
                    sfo::get_data_by_key(addcont_info[content_id].name, sfo_handle, "TITLE");
            }
            boost::trim(addcont_info[content_id].name);
        }
    }
}

static bool popup, content_delete, set_scroll_pos;
static float scroll_pos;
static ImGuiTextFilter search_bar;

void draw_content_manager(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);
    const auto INFORMATION_BAR_HEIGHT = 32.f * SCALE.y;

    const auto SIZE_ICON_LIST = ImVec2(60.f * SCALE.x, 60.f * SCALE.y);
    const auto SIZE_ICON_DETAIL = ImVec2(70.f * SCALE.x, 70.f * SCALE.y);

    const auto BUTTON_SIZE = ImVec2(310.f * SCALE.x, 46.f * SCALE.y);

    const auto WINDOW_SIZE = ImVec2(display_size.x, display_size.y - INFORMATION_BAR_HEIGHT);
    const auto SIZE_LIST = ImVec2(820 * SCALE.x, 378.f * SCALE.y);
    const auto SIZE_INFO = ImVec2(780 * SCALE.x, 402.f * SCALE.y);

    const auto POPUP_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);

    const auto is_background = gui.apps_background.find("NPXS10026") != gui.apps_background.end();
    const auto is_12_hour_format = host.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR;

    ImGui::SetNextWindowPos(ImVec2(0, INFORMATION_BAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);

    ImGui::Begin("##content_manager", &gui.live_area.content_manager, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    if (is_background)
        ImGui::GetBackgroundDrawList()->AddImage(gui.apps_background["NPXS10026"], ImVec2(0.f, 0.f), display_size);
    else
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, 0.f), display_size, IM_COL32(53.f, 54.f, 70.f, 255.f), 0.f, ImDrawCornerFlags_All);

    ImGui::SetWindowFontScale(1.5f * RES_SCALE.x);

    if (menu == "info") {
        ImGui::SetCursorPos(ImVec2(90.f * SCALE.x, 10.f * SCALE.y));
        ImGui::Image(gui.app_selector.user_apps_icon[app_selected], SIZE_ICON_DETAIL);
        const auto CALC_NAME = ImGui::CalcTextSize(get_app_index(gui, app_selected)->title.c_str(), nullptr, false, SIZE_INFO.x - SIZE_ICON_DETAIL.x).y / 2.f;
        ImGui::SetCursorPos(ImVec2((110.f * SCALE.x) + SIZE_ICON_DETAIL.x, (SIZE_ICON_DETAIL.y / 2.f) - CALC_NAME + (10.f * SCALE.y)));
        ImGui::PushTextWrapPos(SIZE_INFO.x);
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", get_app_index(gui, app_selected)->title.c_str());
        ImGui::PopTextWrapPos();
    } else {
        const auto content_str = ImGui::CalcTextSize(title.c_str(), 0, false, SIZE_LIST.x);
        ImGui::PushTextWrapPos(((display_size.x - SIZE_LIST.x) / 2.f) + SIZE_LIST.x);
        ImGui::SetCursorPos(ImVec2((display_size.x / 2.f) - (content_str.x / 2.f), (32.f * SCALE.y) - (content_str.y / 2.f)));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", title.c_str());
        ImGui::PopTextWrapPos();
        if (!menu.empty()) {
            if (((menu == "app") && !gui.app_selector.user_apps.empty()) || ((menu == "save") && !save_data_list.empty())) {
                // Search Bar
                const auto search_size = ImGui::CalcTextSize("Search");
                ImGui::SetCursorPos(ImVec2(20.f * SCALE.y, (32.f * SCALE.y) - (search_size.y / 2.f)));
                ImGui::TextColored(GUI_COLOR_TEXT, "Search");
                ImGui::SameLine();
                search_bar.Draw("##search_bar", 200 * SCALE.x);
            }
            // Free Space
            const auto scal_font = 19.2f / ImGui::GetFontSize();
            ImGui::GetWindowDrawList()->AddText(gui.vita_font, 19.2f * SCALE.x, ImVec2((display_size.x - ((ImGui::CalcTextSize("Free Space").x * scal_font)) * SCALE.x) - (15.f * SCALE.x), 42.f * SCALE.y),
                4294967295, "Free Space");
            ImGui::GetWindowDrawList()->AddText(gui.vita_font, 19.2f * SCALE.x, ImVec2((display_size.x - ((ImGui::CalcTextSize(space["free"].c_str()).x * scal_font)) * SCALE.x) - (15.f * SCALE.x), 68.f * SCALE.y),
                4294967295, space["free"].c_str());
        }
        ImGui::SetCursorPosY(64.0f * SCALE.y);
        ImGui::Separator();
    }

    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, (menu == "info" ? 130.f : 102.0f) * SCALE.y), ImGuiCond_Always, ImVec2(0.5f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
    ImGui::BeginChild("##content_manager_child", menu == "info" ? SIZE_INFO : SIZE_LIST, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    auto common = host.common_dialog.lang.common;

    if (menu.empty()) {
        title = "Content Manager";
        ImGui::SetWindowFontScale(1.2f);
        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, 630.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
        const auto SIZE_SELECT = 80.f * SCALE.y;
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
            host.app_path = "NPXS10026";
            gui.live_area.content_manager = false;
            pre_run_app(gui, host, "NPXS10015");
        }
        ImGui::NextColumn();
        ImGui::SetWindowFontScale(0.8f);
        ImGui::Selectable(space["themes"].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT));
        ImGui::NextColumn();
        ImGui::PopStyleVar();
        ImGui::Separator();
        ImGui::SetWindowFontScale(1.2f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (25.f * SCALE.y));
        ImGui::TextColored(GUI_COLOR_TEXT, "Free Space");
        ImGui::NextColumn();
        ImGui::SetWindowFontScale(0.8f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (25.f * SCALE.y));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", space["free"].c_str());
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (25.f * SCALE.y));
        ImGui::SetWindowFontScale(1.2f);
        ImGui::Separator();
        ImGui::NextColumn();
        ImGui::Columns(1);
    } else {
        if (content_delete) {
            for (const auto &content : contents_selected) {
                if (content.second) {
                    if (menu == "app") {
                        fs::remove_all(fs::path(host.pref_path) / "ux0/app" / content.first);
                        fs::remove_all(fs::path(host.pref_path) / "ux0/addcont" / content.first);
                        gui.app_selector.user_apps.erase(get_app_index(gui, content.first));
                        gui.app_selector.user_apps_icon.erase(content.first);
                    }
                    const auto SAVE_PATH{ fs::path(host.pref_path) / "ux0/user" / host.io.user_id / "savedata" / content.first };
                    fs::remove_all(SAVE_PATH);
                }
            }
            init_content_manager(gui, host);
            contents_selected.clear();
            content_delete = false;
        }
        if (popup) {
            ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
            ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
            ImGui::Begin("##app_delete", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            ImGui::SetNextWindowBgAlpha(0.999f);
            ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
            ImGui::BeginChild("##app_delete_child", POPUP_SIZE, true, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
            ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);
            ImGui::SetCursorPos(ImVec2(52.f * SCALE.x, 80.f * SCALE.y));
            ImGui::PushTextWrapPos(POPUP_SIZE.x);
            ImGui::TextColored(GUI_COLOR_TEXT, menu == "app" ? "The selected applications and all related data, including saved data, will be deleted." : "The selected saved data items will be deleted");
            ImGui::PopTextWrapPos();
            ImGui::SetCursorPos(ImVec2(106.f * SCALE.x, ImGui::GetCursorPosY() + (76.f * SCALE.y)));
            ImGui::TextColored(GUI_COLOR_TEXT, "Data to be Deleted: %s", size_selected_contents.c_str());
            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2) - (BUTTON_SIZE.x + (10.f * SCALE.x)), POPUP_SIZE.y - BUTTON_SIZE.y - (22.0f * SCALE.y)));
            if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_circle)) {
                popup = false;
            }
            ImGui::SameLine(0, 20.f * SCALE.x);
            if (ImGui::Button("OK", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross)) {
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
                ImGui::SetColumnWidth(0, 60 * SCALE.x);
                ImGui::SetColumnWidth(1, 75 * SCALE.x);
                ImGui::SetColumnWidth(2, 580.f * SCALE.x);
                for (const auto &app : gui.app_selector.user_apps) {
                    if (!search_bar.PassFilter(app.title.c_str()) && !search_bar.PassFilter(app.stitle.c_str()) && !search_bar.PassFilter(app.title_id.c_str()))
                        continue;
                    ImGui::PushID(app.path.c_str());
                    ImGui::SetWindowFontScale(1.32f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (15.f * SCALE.y));
                    ImGui::Checkbox("##selected", &contents_selected[app.path]);
                    ImGui::NextColumn();
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (8.f * SCALE.y));
                    ImGui::Image(gui.app_selector.user_apps_icon[app.path], SIZE_ICON_LIST);
                    ImGui::NextColumn();
                    const auto Title_POS = ImGui::GetCursorPosY();
                    ImGui::SetWindowFontScale(1.1f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (4.f * SCALE.y));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", app.title.c_str());
                    ImGui::SetCursorPosY(Title_POS + (46.f * SCALE.y));
                    ImGui::SetWindowFontScale(0.8f);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", get_unit_size(apps_size[app.path]).c_str());
                    ImGui::NextColumn();
                    ImGui::SetWindowFontScale(1.2f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (15.f * SCALE.y));
                    if (ImGui::Button("i", ImVec2(45.f * SCALE.x, 45.f * SCALE.y))) {
                        scroll_pos = ImGui::GetScrollY();
                        ImGui::SetScrollY(0.f);
                        app_selected = app.path;
                        get_app_info(gui, host, app_selected);
                        get_content_info(gui, host);
                        menu = "info";
                    }
                    ImGui::PopID();
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (15.f * SCALE.y));
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
                ImGui::SetColumnWidth(0, 60 * SCALE.x);
                ImGui::SetColumnWidth(1, 75 * SCALE.x);
                for (const auto &save : save_data_list) {
                    if (!search_bar.PassFilter(save.title.c_str()) && !search_bar.PassFilter(save.title_id.c_str()))
                        continue;
                    ImGui::PushID(save.title_id.c_str());
                    ImGui::SetWindowFontScale(1.32f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (15.f * SCALE.y));
                    ImGui::Checkbox("##selected", &contents_selected[save.title_id]);
                    ImGui::NextColumn();
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (8.f * SCALE.y));
                    ImGui::Image(gui.app_selector.user_apps_icon[save.title_id], SIZE_ICON_LIST);
                    ImGui::NextColumn();
                    const auto Title_POS = ImGui::GetCursorPosY();
                    ImGui::SetWindowFontScale(1.1f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (4.f * SCALE.y));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", save.title.c_str());
                    ImGui::SetWindowFontScale(0.8f);
                    ImGui::SetCursorPosY(Title_POS + (46.f * SCALE.y));
                    auto DATE_TIME = get_date_time(gui, host, save.date);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", DATE_TIME[DateTime::DATE_MINI].c_str(), DATE_TIME[DateTime::CLOCK].c_str());
                    if (is_12_hour_format) {
                        ImGui::SameLine();
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s", DATE_TIME[DateTime::DAY_MOMENT].c_str());
                    }
                    ImGui::PopID();
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (6.f * SCALE.y));
                    ImGui::Separator();
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);
            }
        } else if (menu == "info") {
            // Information
            const auto app_index = get_app_index(gui, app_selected);
            ImGui::SetWindowFontScale(1.f);
            ImGui::TextColored(GUI_COLOR_TEXT, "Trophy Earning");
            ImGui::SameLine(280.f * SCALE.x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", gui.app_selector.app_info.trophy.c_str());
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (35.f * SCALE.y));
            ImGui::TextColored(GUI_COLOR_TEXT, "Parental Controls");
            ImGui::SameLine(280.f * SCALE.x);
            ImGui::TextColored(GUI_COLOR_TEXT, "Level %d", *reinterpret_cast<const uint16_t *>(get_app_index(gui, app_selected)->parental_level.c_str()));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (35.f * SCALE.y));
            ImGui::TextColored(GUI_COLOR_TEXT, "Updated");
            ImGui::SameLine(280.f * SCALE.x);
            auto DATE_TIME = get_date_time(gui, host, gui.app_selector.app_info.updated);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", DATE_TIME[DateTime::DATE_MINI].c_str(), DATE_TIME[DateTime::CLOCK].c_str());
            if (is_12_hour_format) {
                ImGui::SameLine();
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", DATE_TIME[DateTime::DAY_MOMENT].c_str());
            }
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (35.f * SCALE.y));
            ImGui::TextColored(GUI_COLOR_TEXT, "Size");
            ImGui::SameLine(280.f * SCALE.x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", get_unit_size(gui.app_selector.app_info.size).c_str());
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (35.f * SCALE.y));
            ImGui::TextColored(GUI_COLOR_TEXT, "Version");
            ImGui::SameLine(280.f * SCALE.x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", get_app_index(gui, app_selected)->app_ver.c_str());
            for (const auto &addcont : addcont_info) {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (40.f * SCALE.y));
                ImGui::Separator();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (40.f * SCALE.y));
                ImGui::SetWindowFontScale(1.2f);
                ImGui::TextColored(GUI_COLOR_TEXT, "+");
                ImGui::SameLine(0, 30.f * SCALE.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", addcont.second.name.c_str());
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (35.f * SCALE.y));
                ImGui::SetWindowFontScale(1.f);
                ImGui::TextColored(GUI_COLOR_TEXT, "Updated");
                ImGui::SameLine(280.f * SCALE.x);
                auto DATE_TIME = get_date_time(gui, host, addcont.second.date);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", DATE_TIME[DateTime::DATE_MINI].c_str(), DATE_TIME[DateTime::CLOCK].c_str());
                if (host.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR) {
                    ImGui::SameLine();
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", DATE_TIME[DateTime::DAY_MOMENT].c_str());
                }
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (35.f * SCALE.y));
                ImGui::TextColored(GUI_COLOR_TEXT, "Size");
                ImGui::SameLine(280.f * SCALE.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", addcont.second.size.c_str());
            }
        }
    }

    ImGui::EndChild();

    ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
    ImGui::SetCursorPos(ImVec2(10.f * SCALE.x, display_size.y - (88.f * SCALE.y)));
    const auto is_empty = ((menu == "app") && gui.app_selector.user_apps.empty()) || ((menu == "save") && save_data_list.empty());
    if (menu.empty() || (menu == "info") || is_empty) {
        if (ImGui::Button("Back", ImVec2(64.f * SCALE.x, 40.f * SCALE.y))) {
            if (!menu.empty()) {
                if (menu == "info") {
                    menu = "app";
                    set_scroll_pos = true;
                } else
                    menu.clear();
            } else {
                if (!gui.apps_list_opened.empty() && gui.apps_list_opened[gui.current_app_selected] == "NPXS10026")
                    gui.live_area.live_area_screen = true;
                else
                    gui.live_area.app_selector = true;
                gui.live_area.content_manager = false;
            }
        }
    } else {
        ImGui::SetWindowFontScale(1.5f * RES_SCALE.x);
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, 482.f * SCALE.y), display_size, IM_COL32(39.f, 42.f, 49.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);
        if (ImGui::Button(common["cancel"].c_str(), ImVec2(202.f * SCALE.x, 44.f * SCALE.y))) {
            if (!menu.empty()) {
                menu.clear();
                contents_selected.clear();
            }
        }
        const auto state = std::any_of(std::begin(contents_selected), std::end(contents_selected), [&](const auto &c) { return !c.second; });
        ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
        ImGui::SetCursorPos(ImVec2(display_size.x - (450.f * SCALE.x), display_size.y - (88.f * SCALE.y)));
        if (ImGui::Button(state ? common["select_all"].c_str() : "Clear All", ImVec2(224.f * SCALE.x, 44.f * SCALE.y))) {
            for (auto &content : contents_selected) {
                if (state)
                    content.second = true;
                else
                    content.second = false;
            }
        }
        const auto is_enable = std::any_of(std::begin(contents_selected), std::end(contents_selected), [&](const auto &cs) { return cs.second; });
        ImGui::SameLine();
        ImGui::SetWindowFontScale(1.5f * RES_SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
        if (is_enable ? ImGui::Button(common["delete"].c_str(), ImVec2(202.f * SCALE.x, 44.f * SCALE.y)) && get_size_selected_contents(gui, host) : ImGui::Selectable(common["delete"].c_str(), false, ImGuiSelectableFlags_Disabled, ImVec2(194.f * SCALE.x, 36.f * SCALE.y)))
            popup = true;
        ImGui::PopStyleVar();
    }
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace gui
