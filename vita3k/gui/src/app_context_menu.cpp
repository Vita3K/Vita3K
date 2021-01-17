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

#include <gui/functions.h>

#include <util/log.h>

#include <pugixml.hpp>
#include <sstream>

namespace gui {

void delete_app(GuiState &gui, HostState &host) {
    const fs::path app_path{ fs::path(host.pref_path) / "ux0/app" / host.app_title_id };

    LOG_INFO_IF(fs::remove_all(app_path), "Application successfully deleted '{} [{}]'.", host.app_title_id, host.app_title);

    if (!fs::exists(app_path)) {
        gui.delete_app_icon = true;
        gui.app_selector.user_apps.erase(get_app_index(gui, host.app_title_id));
    } else
        LOG_ERROR("Failed to delete '{} [{}]'.", host.app_title_id, host.app_title);
}

static std::map<double, std::string> update_history_infos;

static bool get_update_history(GuiState &gui, HostState &host, const std::string &title_id) {
    update_history_infos.clear();
    const auto change_info_path{ fs::path(host.pref_path) / "ux0/app" / title_id / "sce_sys/changeinfo/" };

    std::string fname = fs::exists(change_info_path / fmt::format("changeinfo_{:0>2d}.xml", host.cfg.sys_lang)) ? fmt::format("changeinfo_{:0>2d}.xml", host.cfg.sys_lang) : "changeinfo.xml";

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file((change_info_path.string() + fname).c_str());

    for (const auto &info : doc.child("changeinfo"))
        update_history_infos[info.attribute("app_ver").as_double()] = info.text().as_string();

    for (auto &update : update_history_infos) {
        const auto startpos = "<";
        const auto endpos = ">";

        if (update.second.find_first_of('\n') != std::string::npos)
            update.second.erase(update.second.begin() + update.second.find_first_of('\n'));

        while (update.second.find("</li> <li>") != std::string::npos)
            if (update.second.find("</li> <li>") != std::string::npos)
                update.second.replace(update.second.find("</li> <li>"), update.second.find(endpos) + 2 - update.second.find(startpos), "\n");
        while (update.second.find("<li>") != std::string::npos)
            if (update.second.find("<li>") != std::string::npos)
                update.second.replace(update.second.find("<li>"), update.second.find(endpos) + 1 - update.second.find(startpos), ". ");
        while (update.second.find(startpos) != std::string::npos)
            if (update.second.find(">") + 1 != std::string::npos)
                update.second.erase(update.second.find(startpos), update.second.find(endpos) + 1 - update.second.find(startpos));

        bool found_space = false;
        auto end{ std::remove_if(update.second.begin(), update.second.end(), [&found_space](unsigned ch) {
            bool is_space = std::isspace(ch);
            std::swap(found_space, is_space);
            return found_space && is_space;
        }) };

        if (end != update.second.begin() && std::isspace(static_cast<unsigned>(end[-1])))
            --end;

        update.second.erase(end, update.second.end());
    }

    return !update_history_infos.empty();
}

#ifdef _WIN32
static const char OS_PREFIX[] = "start ";
#elif __APPLE__
static const char OS_PREFIX[] = "open ";
#else
static const char OS_PREFIX[] = "xdg-open ";
#endif

static std::string context_dialog;
static auto information = false;

void draw_app_context_menu(GuiState &gui, HostState &host, const std::string &title_id) {
    const auto APP_PATH{ fs::path(host.pref_path) / "ux0/app" / title_id };
    const auto DLC_PATH{ fs::path(host.pref_path) / "ux0/addcont" / title_id };
    const auto SAVE_DATA_PATH{ fs::path(host.pref_path) / "ux0/user" / host.io.user_id / "savedata" / title_id };
    const auto SHADER_LOG_PATH{ fs::path(host.base_path) / "shaderlog" / title_id };
    auto lang = gui.lang.app_context;
    const auto is_lang = !lang.empty();

    // App Context Menu
    if (ImGui::BeginPopupContextItem("##app_context_menu")) {
        ImGui::SetWindowFontScale(1.3f);
        if (ImGui::MenuItem("Boot"))
            pre_load_app(gui, host, false);
        if (title_id.find("NPXS") == std::string::npos) {
            if (ImGui::MenuItem("Check App Compatibility")) {
                const std::string compat_url = title_id.find("PCS") != std::string::npos ? "https://vita3k.org/compatibility?g=" + host.app_title_id : "https://github.com/Vita3K/homebrew-compatibility/issues?q=" + host.app_title;
                system((OS_PREFIX + compat_url).c_str());
            }
            if (ImGui::BeginMenu("Copy App Info")) {
                if (ImGui::MenuItem("ID and Name")) {
                    ImGui::LogToClipboard();
                    ImGui::LogText("%s [%s]", title_id.c_str(), host.app_title.c_str());
                    ImGui::LogFinish();
                }
                if (ImGui::MenuItem("ID")) {
                    ImGui::LogToClipboard();
                    ImGui::LogText("%s", title_id.c_str());
                    ImGui::LogFinish();
                }
                if (ImGui::MenuItem("Name")) {
                    ImGui::LogToClipboard();
                    ImGui::LogText("%s", host.app_title.c_str());
                    ImGui::LogFinish();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Open Folder")) {
                if (ImGui::MenuItem("Application"))
                    system((OS_PREFIX + APP_PATH.string()).c_str());
                if (fs::exists(DLC_PATH) && ImGui::MenuItem("Dlc"))
                    system((OS_PREFIX + DLC_PATH.string()).c_str());
                if (fs::exists(SAVE_DATA_PATH) && ImGui::MenuItem("Save Data"))
                    system((OS_PREFIX + SAVE_DATA_PATH.string()).c_str());
                ImGui::EndMenu();
            }
            if (!host.cfg.show_live_area_screen && ImGui::MenuItem("Live Area", nullptr, &gui.live_area.live_area_screen))
                init_live_area(gui, host);
            if (ImGui::BeginMenu("Delete")) {
                if (ImGui::MenuItem("Application"))
                    context_dialog = "app";
                if (fs::exists(DLC_PATH) && ImGui::MenuItem("DLC"))
                    fs::remove_all(DLC_PATH);
                if (fs::exists(SAVE_DATA_PATH) && ImGui::MenuItem("Save Data"))
                    context_dialog = "save";
                if (ImGui::MenuItem("Shader Log")) {
                    fs::remove_all(SHADER_LOG_PATH);
                }
                ImGui::EndMenu();
            }
            if (fs::exists(APP_PATH / "sce_sys/changeinfo/") && ImGui::MenuItem(is_lang ? lang["update_history"].c_str() : "Update History")) {
                if (get_update_history(gui, host, title_id))
                    context_dialog = "history";
                else
                    LOG_WARN("Patch note Error for title id: {}", title_id);
            }
        }
        if (ImGui::MenuItem(is_lang ? lang["information"].c_str() : "Information", nullptr, &information)) {
            if (title_id.find("NPXS") == std::string::npos) {
                get_app_info(gui, host, title_id);
                const auto app_size = get_app_size(host, title_id);
                gui.app_selector.app_info.size = app_size;
            }
            gui.live_area.information_bar = false;
        }
        ImGui::EndPopup();
    }

    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto scal = ImVec2(display_size.x / 960.0f, display_size.y / 544.0f);
    const auto WINDOW_SIZE = ImVec2(756.0f * scal.x, 436.0f * scal.y);

    const auto BUTTON_SIZE = ImVec2(320.f * scal.x, 46.f * scal.y);
    const auto PUPOP_ICON_SIZE = ImVec2(96.f * scal.x, 96.f * scal.y);
    const auto INFO_ICON_SIZE = ImVec2(128.f * scal.x, 128.f * scal.y);

    const auto app_index = get_app_index(gui, title_id);

    // Context Dialog
    if (!context_dialog.empty()) {
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
        ImGui::Begin("##context_dialog", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetNextWindowBgAlpha(0.999f);
        ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f);
        ImGui::BeginChild("##context_dialog_child", WINDOW_SIZE, true, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
        // Update History
        if (context_dialog == "history") {
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetWindowPos().x + 20.f * scal.x, ImGui::GetWindowPos().y + BUTTON_SIZE.y));
            ImGui::BeginChild("##info_update_list", ImVec2(WINDOW_SIZE.x - (30.f * scal.x), WINDOW_SIZE.y - (BUTTON_SIZE.y * 2.f) - (25.f * scal.y)), false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
            for (const auto &update : update_history_infos) {
                ImGui::SetWindowFontScale(1.4f);
                ImGui::TextColored(GUI_COLOR_TEXT, "Version %.2f", update.first);
                ImGui::SetWindowFontScale(1.f);
                ImGui::PushTextWrapPos(WINDOW_SIZE.x - (80.f * scal.x));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s\n", update.second.c_str());
                ImGui::PopTextWrapPos();
                ImGui::TextColored(GUI_COLOR_TEXT, "\n");
            }
            ImGui::EndChild();
            ImGui::SetWindowFontScale(1.4f * scal.x);
            ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (BUTTON_SIZE.x / 2.f), WINDOW_SIZE.y - BUTTON_SIZE.y - (22.f * scal.y)));
        } else {
            // Delete Data
            if (gui.app_selector.user_apps_icon.find(title_id) != gui.app_selector.user_apps_icon.end()) {
                ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (PUPOP_ICON_SIZE.x / 2.f), 24.f * scal.y));
                ImGui::Image(gui.app_selector.user_apps_icon[title_id], PUPOP_ICON_SIZE);
            }
            ImGui::SetWindowFontScale(1.6f * scal.x);
            ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize(app_index->stitle.c_str()).x / 2.f));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", host.app_short_title.c_str());
            ImGui::SetWindowFontScale(1.4f * scal.x);
            const auto app_delete = is_lang ? lang["app_delete"] : "This application and all related data, including saved data, will be deleted.";
            const auto save_delete = is_lang ? lang["save_delete"] : "Do you want to delete this saved data?";
            const auto ask_delete = context_dialog == "save" ? save_delete : app_delete;
            ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x / 2 - ImGui::CalcTextSize(ask_delete.c_str(), 0, false, WINDOW_SIZE.x - (108.f * scal.x)).x / 2, (WINDOW_SIZE.y / 2) + 10));
            ImGui::PushTextWrapPos(WINDOW_SIZE.x - (54.f * scal.x));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", ask_delete.c_str());
            ImGui::PopTextWrapPos();
            if ((context_dialog == "app") && ImGui::IsItemHovered())
                ImGui::SetTooltip("Deleting a application may take a while\ndepending on its size and your hardware.");
            ImGui::SetWindowFontScale(1.4f * scal.x);
            ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * scal.x)), WINDOW_SIZE.y - BUTTON_SIZE.y - (24.0f * scal.y)));
            if (ImGui::Button("Cancel", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_circle)) {
                context_dialog.clear();
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) + (20.f * scal.x));
        }
        if (ImGui::Button("Ok", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross)) {
            if (context_dialog == "app") {
                fs::remove_all(DLC_PATH);
                fs::remove_all(SAVE_DATA_PATH);
                fs::remove_all(SHADER_LOG_PATH);
                delete_app(gui, host);
            } else if (context_dialog == "save")
                fs::remove_all(SAVE_DATA_PATH);
            context_dialog.clear();
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::End();
    }

    // Information
    if (information) {
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
        ImGui::Begin("##information", &information, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetWindowFontScale(1.5f * scal.x);
        ImGui::SetCursorPos(ImVec2(10.0f * scal.x, 10.0f * scal.y));
        if (ImGui::Button("X", ImVec2(40.f * scal.x, 40.f * scal.y)) || ImGui::IsKeyPressed(host.cfg.keyboard_button_circle)) {
            information = false;
            gui.live_area.information_bar = true;
        }
        if (get_app_icon(gui, title_id)->first == title_id) {
            ImGui::SetCursorPos(ImVec2((display_size.x / 2.f) - (INFO_ICON_SIZE.x / 2.f), 22.f * scal.y));
            ImGui::Image(get_app_icon(gui, title_id)->second, INFO_ICON_SIZE);
        }
        const auto name = is_lang ? lang["name"] : "Name";
        ImGui::SetCursorPos(ImVec2((display_size.x / 2.f) - ImGui::CalcTextSize((name + "  ").c_str()).x, INFO_ICON_SIZE.y + (50.f * scal.y)));
        ImGui::TextColored(GUI_COLOR_TEXT, (name + " ").c_str());
        ImGui::SameLine();
        ImGui::PushTextWrapPos(display_size.x - (85.f * scal.x));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", app_index->title.c_str());
        ImGui::PopTextWrapPos();
        if (title_id.find("NPXS") == std::string::npos) {
            ImGui::Spacing();
            const auto trophy_earning = is_lang ? lang["trophy_earning"] : "Trophy Earning";
            ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((trophy_earning + "  ").c_str()).x);
            ImGui::TextColored(GUI_COLOR_TEXT, (trophy_earning + "  %s").c_str(), gui.app_selector.app_info.trophy.c_str());
            ImGui::Spacing();
            const auto parental_Controls = is_lang ? lang["parental_Controls"] : "Parental Controls";
            ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((parental_Controls + "  ").c_str()).x);
            const auto level = is_lang ? lang["level"] : "Level";
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", (parental_Controls + "  " + level).c_str());
            ImGui::SameLine();
            ImGui::TextColored(GUI_COLOR_TEXT, "%d", *reinterpret_cast<const uint16_t *>(app_index->parental_level.c_str()));
            ImGui::Spacing();
            const auto updated = is_lang ? lang["updated"] : "Updated";
            ImGui::SetCursorPosX(((display_size.x / 2.f) - ImGui::CalcTextSize((updated + "  ").c_str()).x));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", (updated + "  " + gui.app_selector.app_info.updated).c_str());
            ImGui::Spacing();
            const auto size = is_lang ? lang["size"] : "Size";
            ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((size + "  ").c_str()).x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", (size + "  " + get_unit_size(gui.app_selector.app_info.size)).c_str());
            ImGui::Spacing();
            const auto version = is_lang ? lang["version"] : "Version";
            ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((version + "  ").c_str()).x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", (version + "  " + app_index->app_ver).c_str());
        }
        ImGui::End();
    }
}

} // namespace gui
