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
#include <util/safe_time.h>

#include <pugixml.hpp>
#include <sstream>

namespace gui {

static std::map<double, std::string> update_history_infos;

static bool get_update_history(GuiState &gui, HostState &host, const std::string &app_path) {
    update_history_infos.clear();
    const auto change_info_path{ fs::path(host.pref_path) / "ux0/app" / app_path / "sce_sys/changeinfo/" };

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
static const char OS_PREFIX[] = "start \"Vita3K\" ";
#elif __APPLE__
static const char OS_PREFIX[] = "open ";
#else
static const char OS_PREFIX[] = "xdg-open ";
#endif

static std::vector<TimeApp>::iterator get_time_app_index(GuiState &gui, HostState &host, const std::string &app) {
    const auto time_app_index = std::find_if(gui.time_apps[host.io.user_id].begin(), gui.time_apps[host.io.user_id].end(), [&](const TimeApp &t) {
        return t.app == app;
    });

    return time_app_index;
}

static std::string get_time_app_used(const int64_t &time_used) {
    std::string time_app_used;
    if (time_used < 60)
        time_app_used = fmt::format("{}s", time_used);
    else {
        const std::chrono::seconds sec(time_used);
        const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(sec).count() % 60;
        const auto seconds = sec.count() % 60;
        if (time_used < 3600)
            time_app_used = fmt::format("{}m:{}s", minutes, seconds);
        else {
            const auto hours = std::chrono::duration_cast<std::chrono::hours>(sec).count();
            time_app_used = fmt::format("{}h:{}m:{}s", hours, minutes, seconds);
        }
    }

    return time_app_used;
}

void get_time_apps(GuiState &gui, HostState &host) {
    gui.time_apps.clear();
    const auto time_path{ fs::path(host.pref_path) / "ux0/user/time.xml" };

    pugi::xml_document time_xml;
    if (fs::exists(time_path)) {
        if (time_xml.load_file(time_path.c_str())) {
            const auto time_child = time_xml.child("time");
            if (!time_child.child("user").empty()) {
                for (const auto &user : time_child) {
                    auto user_id = user.attribute("id").as_string();
                    for (const auto &app : user)
                        gui.time_apps[user_id].push_back({ app.text().as_string(), app.attribute("last-time-used").as_llong(), app.attribute("time-used").as_llong() });
                }
            }
        } else {
            LOG_ERROR("Time XML found is corrupted on path: {}", time_path.string());
            fs::remove(time_path);
        }
    }
}

static void save_time_apps(GuiState &gui, HostState &host) {
    pugi::xml_document time_xml;
    auto declarationUser = time_xml.append_child(pugi::node_declaration);
    declarationUser.append_attribute("version") = "1.0";
    declarationUser.append_attribute("encoding") = "utf-8";

    auto time_child = time_xml.append_child("time");

    for (const auto &user : gui.time_apps) {
        // Sort by last time used
        std::sort(gui.time_apps[user.first].begin(), gui.time_apps[user.first].end(), [&](const TimeApp &ta, const TimeApp &tb) {
            return ta.last_time_used > tb.last_time_used;
        });

        auto user_child = time_child.append_child("user");
        user_child.append_attribute("id") = user.first.c_str();

        for (const auto &app : user.second) {
            auto app_child = user_child.append_child("app");
            app_child.append_attribute("last-time-used") = app.last_time_used;
            app_child.append_attribute("time-used") = app.time_used;
            app_child.append_child(pugi::node_pcdata).set_value(app.app.c_str());
        }
    }

    const auto time_path{ fs::path(host.pref_path) / "ux0/user/time.xml" };
    const auto save_xml = time_xml.save_file(time_path.c_str());
    if (!save_xml)
        LOG_ERROR("Fail save xml");
}

void update_time_app_used(GuiState &gui, HostState &host, const std::string &app) {
    const auto &time_app_index = get_time_app_index(gui, host, app);
    time_app_index->time_used += std::time(nullptr) - time_app_index->last_time_used;

    save_time_apps(gui, host);
}

void update_last_time_app_used(GuiState &gui, HostState &host, const std::string &app) {
    const auto &time_app_index = get_time_app_index(gui, host, app);
    if (time_app_index != gui.time_apps[host.io.user_id].end())
        time_app_index->last_time_used = std::time(nullptr);
    else
        gui.time_apps[host.io.user_id].push_back({ app, std::time(nullptr), 0 });

    save_time_apps(gui, host);
}

void delete_app(GuiState &gui, HostState &host, const std::string &app_path) {
    const auto APP_INDEX = get_app_index(gui, app_path);
    const auto title_id = APP_INDEX->title_id;
    try {
        const auto PREF_PATH = fs::path(host.pref_path);
        fs::remove_all(PREF_PATH / "ux0/app" / app_path);

        const auto BASE_PATH = fs::path(host.base_path);
        const auto CUSTOM_CONFIG_PATH{ BASE_PATH / "config" / fmt::format("config_{}.xml", app_path) };
        if (fs::exists(CUSTOM_CONFIG_PATH))
            fs::remove_all(CUSTOM_CONFIG_PATH);
        const auto DLC_PATH{ PREF_PATH / "ux0/addcont" / title_id };
        if (fs::exists(DLC_PATH))
            fs::remove_all(DLC_PATH);
        const auto LICENSE_PATH{ PREF_PATH / "ux0/license" / title_id };
        if (fs::exists(LICENSE_PATH))
            fs::remove_all(LICENSE_PATH);
        const auto PATCH_PATH{ PREF_PATH / "ux0/patch" / title_id };
        if (fs::exists(PATCH_PATH))
            fs::remove_all(PATCH_PATH);
        const auto SAVE_DATA_PATH{ PREF_PATH / "ux0/user" / host.io.user_id / "savedata" / title_id };
        if (fs::exists(SAVE_DATA_PATH))
            fs::remove_all(SAVE_DATA_PATH);
        const auto SHADER_CACHE_PATH{ BASE_PATH / "shaders" / title_id };
        if (fs::exists(SHADER_CACHE_PATH))
            fs::remove_all(SHADER_CACHE_PATH);
        const auto SHADER_LOG_PATH{ BASE_PATH / "shaderlog" / title_id };
        if (fs::exists(SHADER_LOG_PATH))
            fs::remove_all(SHADER_LOG_PATH);

        if (gui.app_selector.user_apps_icon.find(app_path) != gui.app_selector.user_apps_icon.end()) {
            gui.app_selector.user_apps_icon[app_path] = {};
            gui.app_selector.user_apps_icon.erase(app_path);
        }

        const auto time_app_index = get_time_app_index(gui, host, app_path);
        if (time_app_index != gui.time_apps[host.io.user_id].end()) {
            gui.time_apps[host.io.user_id].erase(time_app_index);
            save_time_apps(gui, host);
        }

        LOG_INFO("Application successfully deleted '{} [{}]'.", title_id, APP_INDEX->title);

        gui.app_selector.user_apps.erase(APP_INDEX);

        save_apps_cache(gui, host);
    } catch (std::exception &e) {
        LOG_ERROR("Failed to delete '{} [{}]'.\n{}", title_id, APP_INDEX->title, e.what());
    }
}

static std::string context_dialog;
static auto information = false;

void draw_app_context_menu(GuiState &gui, HostState &host, const std::string &app_path) {
    const auto APP_INDEX = get_app_index(gui, app_path);
    const auto title_id = APP_INDEX->title_id;

    const auto APP_PATH{ fs::path(host.pref_path) / "ux0/app" / app_path };
    const auto CUSTOM_CONFIG_PATH{ fs::path(host.base_path) / "config" / fmt::format("config_{}.xml", app_path) };
    const auto DLC_PATH{ fs::path(host.pref_path) / "ux0/addcont" / title_id };
    const auto LICENSE_PATH{ fs::path(host.pref_path) / "ux0/license" / title_id };
    const auto SAVE_DATA_PATH{ fs::path(host.pref_path) / "ux0/user" / host.io.user_id / "savedata" / title_id };
    const auto SHADER_CACHE_PATH{ fs::path(host.base_path) / "shaders" / title_id };
    const auto SHADER_LOG_PATH{ fs::path(host.base_path) / "shaderlog" / title_id };

    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);

    auto lang = gui.lang.app_context;
    const auto is_lang = !lang.empty();
    auto common = host.common_dialog.lang.common;

    // App Context Menu
    if (ImGui::BeginPopupContextItem("##app_context_menu")) {
        ImGui::SetWindowFontScale(1.3f * RES_SCALE.x);
        if (ImGui::MenuItem("Boot"))
            pre_load_app(gui, host, false, app_path);
        if (title_id.find("NPXS") == std::string::npos) {
            if (ImGui::MenuItem("Check App Compatibility")) {
                const std::string compat_url = title_id.find("PCS") != std::string::npos ? "https://vita3k.org/compatibility?g=" + title_id : "https://github.com/Vita3K/homebrew-compatibility/issues?q=" + APP_INDEX->title;
                system((OS_PREFIX + compat_url).c_str());
            }
            if (ImGui::BeginMenu("Copy App Info")) {
                if (ImGui::MenuItem("ID and Name")) {
                    ImGui::LogToClipboard();
                    ImGui::LogText("%s [%s]", title_id.c_str(), APP_INDEX->title.c_str());
                    ImGui::LogFinish();
                }
                if (ImGui::MenuItem("ID")) {
                    ImGui::LogToClipboard();
                    ImGui::LogText("%s", title_id.c_str());
                    ImGui::LogFinish();
                }
                if (ImGui::MenuItem("Name")) {
                    ImGui::LogToClipboard();
                    ImGui::LogText("%s", APP_INDEX->title.c_str());
                    ImGui::LogFinish();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Custom Config")) {
                if (!fs::exists(CUSTOM_CONFIG_PATH)) {
                    if (ImGui::MenuItem("Create", nullptr, &gui.configuration_menu.custom_settings_dialog))
                        init_config(gui, host, app_path);
                } else {
                    if (ImGui::MenuItem("Edit", nullptr, &gui.configuration_menu.custom_settings_dialog))
                        init_config(gui, host, app_path);
                    if (ImGui::MenuItem("Remove"))
                        fs::remove(CUSTOM_CONFIG_PATH);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Open Folder")) {
                if (ImGui::MenuItem("Application"))
                    system((OS_PREFIX + ("\"" + APP_PATH.string() + "\"")).c_str());
                if (fs::exists(DLC_PATH) && ImGui::MenuItem("Dlc"))
                    system((OS_PREFIX + DLC_PATH.string()).c_str());
                if (fs::exists(LICENSE_PATH) && ImGui::MenuItem("License"))
                    system((OS_PREFIX + LICENSE_PATH.string()).c_str());
                if (fs::exists(SAVE_DATA_PATH) && ImGui::MenuItem("Save Data"))
                    system((OS_PREFIX + SAVE_DATA_PATH.string()).c_str());
                if (ImGui::MenuItem("Shader Cache")) {
                    if (!fs::exists(SHADER_CACHE_PATH))
                        fs::create_directories(SHADER_CACHE_PATH);
                    system((OS_PREFIX + SHADER_CACHE_PATH.string()).c_str());
                }
                if (fs::exists(SHADER_LOG_PATH) && ImGui::MenuItem("Shader Log"))
                    system((OS_PREFIX + SHADER_LOG_PATH.string()).c_str());
                ImGui::EndMenu();
            }
            if (!host.cfg.show_live_area_screen && ImGui::MenuItem("Live Area", nullptr, &gui.live_area.live_area_screen))
                init_live_area(gui, host);
            if (ImGui::BeginMenu(!common["delete"].empty() ? common["delete"].c_str() : "Delete")) {
                if (ImGui::MenuItem("Application"))
                    context_dialog = "app";
                if (fs::exists(DLC_PATH) && ImGui::MenuItem("DLC"))
                    fs::remove_all(DLC_PATH);
                if (fs::exists(LICENSE_PATH) && ImGui::MenuItem("License"))
                    fs::remove_all(LICENSE_PATH);
                if (fs::exists(SAVE_DATA_PATH) && ImGui::MenuItem("Save Data"))
                    context_dialog = "save";
                if (fs::exists(SHADER_CACHE_PATH) && ImGui::MenuItem("Shader Cache"))
                    fs::remove_all(SHADER_CACHE_PATH);
                if (fs::exists(SHADER_LOG_PATH) && ImGui::MenuItem("Shader Log"))
                    fs::remove_all(SHADER_LOG_PATH);
                ImGui::EndMenu();
            }
            if (fs::exists(APP_PATH / "sce_sys/changeinfo/") && ImGui::MenuItem(is_lang ? lang["update_history"].c_str() : "Update History")) {
                if (get_update_history(gui, host, app_path))
                    context_dialog = "history";
                else
                    LOG_WARN("Patch note Error for title id: {} in path: {}", title_id, app_path);
            }
        }
        if (ImGui::MenuItem(is_lang ? lang["information"].c_str() : "Information", nullptr, &information)) {
            if (title_id.find("NPXS") == std::string::npos) {
                get_app_info(gui, host, app_path);
                const auto app_size = get_app_size(gui, host, app_path);
                gui.app_selector.app_info.size = app_size;
            }
            gui.live_area.information_bar = false;
        }
        ImGui::EndPopup();
    }

    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);
    const auto WINDOW_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);

    const auto BUTTON_SIZE = ImVec2(320.f * SCALE.x, 46.f * SCALE.y);
    const auto PUPOP_ICON_SIZE = ImVec2(96.f * SCALE.x, 96.f * SCALE.y);
    const auto INFO_ICON_SIZE = ImVec2(128.f * SCALE.x, 128.f * SCALE.y);

    // Context Dialog
    if (!context_dialog.empty()) {
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
        ImGui::Begin("##context_dialog", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetNextWindowBgAlpha(0.999f);
        ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
        ImGui::BeginChild("##context_dialog_child", WINDOW_SIZE, true, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
        // Update History
        if (context_dialog == "history") {
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetWindowPos().x + 20.f * SCALE.x, ImGui::GetWindowPos().y + BUTTON_SIZE.y));
            ImGui::BeginChild("##info_update_list", ImVec2(WINDOW_SIZE.x - (30.f * SCALE.x), WINDOW_SIZE.y - (BUTTON_SIZE.y * 2.f) - (25.f * SCALE.y)), false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
            for (const auto &update : update_history_infos) {
                ImGui::SetWindowFontScale(1.4f);
                ImGui::TextColored(GUI_COLOR_TEXT, "Version %.2f", update.first);
                ImGui::SetWindowFontScale(1.f);
                ImGui::PushTextWrapPos(WINDOW_SIZE.x - (80.f * SCALE.x));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s\n", update.second.c_str());
                ImGui::PopTextWrapPos();
                ImGui::TextColored(GUI_COLOR_TEXT, "\n");
            }
            ImGui::EndChild();
            ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
            ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (BUTTON_SIZE.x / 2.f), WINDOW_SIZE.y - BUTTON_SIZE.y - (22.f * SCALE.y)));
        } else {
            // Delete Data
            if (gui.app_selector.user_apps_icon.find(title_id) != gui.app_selector.user_apps_icon.end()) {
                ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (PUPOP_ICON_SIZE.x / 2.f), 24.f * SCALE.y));
                ImGui::Image(gui.app_selector.user_apps_icon[title_id], PUPOP_ICON_SIZE);
            }
            ImGui::SetWindowFontScale(1.6f);
            ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - (ImGui::CalcTextSize(APP_INDEX->stitle.c_str()).x / 2.f));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", APP_INDEX->stitle.c_str());
            ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
            const auto app_delete = is_lang ? lang["app_delete"] : "This application and all related data, including saved data, will be deleted.";
            const auto save_delete = is_lang ? lang["save_delete"] : "Do you want to delete this saved data?";
            const auto ask_delete = context_dialog == "save" ? save_delete : app_delete;
            ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x / 2 - ImGui::CalcTextSize(ask_delete.c_str(), 0, false, WINDOW_SIZE.x - (108.f * SCALE.x)).x / 2, (WINDOW_SIZE.y / 2) + 10));
            ImGui::PushTextWrapPos(WINDOW_SIZE.x - (54.f * SCALE.x));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", ask_delete.c_str());
            ImGui::PopTextWrapPos();
            if ((context_dialog == "app") && ImGui::IsItemHovered())
                ImGui::SetTooltip("Deleting a application may take a while\ndepending on its size and your hardware.");
            ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
            ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * SCALE.x)), WINDOW_SIZE.y - BUTTON_SIZE.y - (24.0f * SCALE.y)));
            if (ImGui::Button(!common["cancel"].empty() ? common["cancel"].c_str() : "Cancel", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_circle)) {
                context_dialog.clear();
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) + (20.f * SCALE.x));
        }
        if (ImGui::Button("OK", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross)) {
            if (context_dialog == "app")
                delete_app(gui, host, app_path);
            else if (context_dialog == "save")
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
        ImGui::SetWindowFontScale(1.5f * RES_SCALE.x);
        ImGui::SetCursorPos(ImVec2(10.0f * SCALE.x, 10.0f * SCALE.y));
        if (ImGui::Button("X", ImVec2(40.f * SCALE.x, 40.f * SCALE.y)) || ImGui::IsKeyPressed(host.cfg.keyboard_button_circle)) {
            information = false;
            gui.live_area.information_bar = true;
        }
        if (get_app_icon(gui, title_id)->first == title_id) {
            ImGui::SetCursorPos(ImVec2((display_size.x / 2.f) - (INFO_ICON_SIZE.x / 2.f), 22.f * SCALE.x));
            ImGui::Image(get_app_icon(gui, title_id)->second, INFO_ICON_SIZE);
        }
        const auto name = is_lang ? lang["name"] : "Name";
        ImGui::SetCursorPos(ImVec2((display_size.x / 2.f) - ImGui::CalcTextSize((name + "  ").c_str()).x, INFO_ICON_SIZE.y + (50.f * SCALE.y)));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s ", name.c_str());
        ImGui::SameLine();
        ImGui::PushTextWrapPos(display_size.x - (85.f * SCALE.x));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", APP_INDEX->title.c_str());
        ImGui::PopTextWrapPos();
        if (title_id.find("NPXS") == std::string::npos) {
            ImGui::Spacing();
            const auto trophy_earning = is_lang ? lang["trophy_earning"] : "Trophy Earning";
            ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((trophy_earning + "  ").c_str()).x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s  %s", trophy_earning.c_str(), gui.app_selector.app_info.trophy.c_str());
            ImGui::Spacing();
            const auto parental_Controls = is_lang ? lang["parental_Controls"] : "Parental Controls";
            ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((parental_Controls + "  ").c_str()).x);
            const auto level = is_lang ? lang["level"] : "Level";
            ImGui::TextColored(GUI_COLOR_TEXT, "%s  %s", parental_Controls.c_str(), level.c_str());
            ImGui::SameLine();
            ImGui::TextColored(GUI_COLOR_TEXT, "%d", *reinterpret_cast<const uint16_t *>(APP_INDEX->parental_level.c_str()));
            ImGui::Spacing();
            const auto updated = is_lang ? lang["updated"] : "Updated";
            ImGui::SetCursorPosX(((display_size.x / 2.f) - ImGui::CalcTextSize((updated + "  ").c_str()).x));
            auto DATE_TIME = get_date_time(gui, host, gui.app_selector.app_info.updated);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s  %s %s", updated.c_str(), DATE_TIME["date"].c_str(), DATE_TIME["clock"].c_str());
            if (gui.users[host.io.user_id].clock_12_hour) {
                ImGui::SameLine();
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", DATE_TIME["day-moment"].c_str());
            }
            ImGui::Spacing();
            const auto size = is_lang ? lang["size"] : "Size";
            ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((size + "  ").c_str()).x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", (size + "  " + get_unit_size(gui.app_selector.app_info.size)).c_str());
            ImGui::Spacing();
            const auto version = is_lang ? lang["version"] : "Version";
            ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((version + "  ").c_str()).x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s  %s", version.c_str(), APP_INDEX->app_ver.c_str());
            ImGui::Spacing();
            const auto last_time_used = !lang["last_time_used"].empty() ? lang["last_time_used"] : "Last time used";
            ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((last_time_used + "  ").c_str()).x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s ", last_time_used.c_str());
            ImGui::SameLine();
            const auto time_app_index = get_time_app_index(gui, host, app_path);
            if (time_app_index != gui.time_apps[host.io.user_id].end()) {
                tm date_tm = {};
                SAFE_LOCALTIME(&time_app_index->last_time_used, &date_tm);
                auto LAST_TIME = get_date_time(gui, host, date_tm);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", LAST_TIME["date"].c_str(), LAST_TIME["clock"].c_str());
                if (gui.users[host.io.user_id].clock_12_hour) {
                    ImGui::SameLine();
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", LAST_TIME["day-moment"].c_str());
                }
                ImGui::Spacing();
                const auto time_used = !lang["time_used"].empty() ? lang["time_used"] : "Time used";
                ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((time_used + "  ").c_str()).x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s  %s", time_used.c_str(), get_time_app_used(time_app_index->time_used).c_str());
            } else
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", !lang["never"].empty() ? lang["never"].c_str() : "Never");
        }
        ImGui::End();
    }
}

} // namespace gui
