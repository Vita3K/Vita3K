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

#include <config/state.h>
#include <config/version.h>
#include <dialog/state.h>
#include <gui/functions.h>
#include <include/cpu.h>
#include <include/environment.h>
#include <io/state.h>
#include <renderer/state.h>

#include <util/log.h>
#include <util/safe_time.h>

#include <SDL3/SDL_cpuinfo.h>
#include <SDL3/SDL_misc.h>
#undef main

#include <boost/algorithm/string/replace.hpp>

#include <pugixml.hpp>
#include <regex>

namespace gui {

static std::map<double, std::string> update_history_infos;

static bool get_update_history(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    update_history_infos.clear();
    const auto change_info_path{ emuenv.pref_path / "ux0/app" / app_path / "sce_sys/changeinfo/" };

    std::string fname = fs::exists(change_info_path / fmt::format("changeinfo_{:0>2d}.xml", emuenv.cfg.sys_lang)) ? fmt::format("changeinfo_{:0>2d}.xml", emuenv.cfg.sys_lang) : "changeinfo.xml";

    pugi::xml_document doc;
    if (!doc.load_file((change_info_path / fname).c_str()))
        return false;

    for (const auto &info : doc.child("changeinfo")) {
        double app_ver = info.attribute("app_ver").as_double();
        std::string text = info.text().as_string();

        // Replace HTML tags and special character entities
        text = std::regex_replace(text, std::regex(R"(<li>)"), reinterpret_cast<const char *>(u8"\u30FB"));
        text = std::regex_replace(text, std::regex(R"(<br/>|<br>|</li>)"), "\n");
        text = std::regex_replace(text, std::regex(R"(<[^>]+>)"), "");
        text = std::regex_replace(text, std::regex(R"(&nbsp;)"), " ");
        text = std::regex_replace(text, std::regex(R"(&reg;)"), reinterpret_cast<const char *>(u8"\u00AE"));

        // Remove duplicate newlines and spaces
        auto end = std::unique(text.begin(), text.end(), [](char a, char b) {
            return std::isspace(a) && std::isspace(b);
        });
        if (end != text.begin() && std::isspace(static_cast<unsigned>(end[-1])))
            --end;
        text.erase(end, text.end());

        update_history_infos[app_ver] = text;
    }

    return !update_history_infos.empty();
}

std::vector<TimeApp>::iterator get_time_app_index(GuiState &gui, EmuEnvState &emuenv, const std::string &app) {
    const auto time_app_index = std::find_if(gui.time_apps[emuenv.io.user_id].begin(), gui.time_apps[emuenv.io.user_id].end(), [&](const TimeApp &t) {
        return t.app == app;
    });

    return time_app_index;
}

static std::string get_time_app_used(GuiState &gui, const int64_t &time_used) {
    constexpr uint32_t one_min = 60;
    constexpr uint32_t one_hour = one_min * 60;
    constexpr uint32_t twenty_four_hours = 24;
    constexpr uint32_t one_day = one_hour * twenty_four_hours;
    constexpr uint32_t seven_days = 7;
    constexpr uint32_t one_week = one_day * seven_days;

    auto &lang = gui.lang.app_context.time_used;

    if (time_used < one_min)
        return fmt::format(fmt::runtime(lang["time_used_seconds"]), time_used);
    else {
        const std::chrono::seconds sec(time_used);
        const uint32_t minutes = std::chrono::duration_cast<std::chrono::minutes>(sec).count() % one_min;
        const uint32_t seconds = sec.count() % one_min;
        if (time_used < one_hour)
            return fmt::format(fmt::runtime(lang["time_used_minutes"]), minutes, seconds);
        else {
            const uint32_t count_hours = std::chrono::duration_cast<std::chrono::hours>(sec).count();
            if (time_used < one_day)
                return fmt::format(fmt::runtime(lang["time_used_hours"]), count_hours, minutes, seconds);
            else {
                const uint32_t count_days = count_hours / twenty_four_hours;
                const uint32_t hours_per_day = count_hours - (count_days * twenty_four_hours);
                if (time_used < one_week)
                    return fmt::format(fmt::runtime(lang["time_used_days"]), count_days, hours_per_day, minutes, seconds);
                else {
                    const uint32_t count_weeks = count_days / seven_days;
                    const uint32_t count_days_week = count_days - (count_weeks * seven_days);
                    return fmt::format(fmt::runtime(lang["time_used_weeks"]), count_weeks, count_days_week, hours_per_day, minutes, seconds);
                }
            }
        }
    }
}

void get_time_apps(GuiState &gui, EmuEnvState &emuenv) {
    gui.time_apps.clear();
    const auto time_path{ emuenv.pref_path / "ux0/user/time.xml" };

    pugi::xml_document time_xml;
    if (fs::exists(time_path)) {
        if (time_xml.load_file(time_path.c_str())) {
            const auto time_child = time_xml.child("time");
            if (!time_child.child("user").empty()) {
                for (const auto &user : time_child) {
                    auto user_id = user.attribute("id").as_string();
                    for (const auto &app : user)
                        // Can't use emplace_back due to Clang 15 for macos
                        gui.time_apps[user_id].push_back({ app.text().as_string(), app.attribute("last-time-used").as_llong(), app.attribute("time-used").as_llong() });
                }
            }
        } else {
            LOG_ERROR("Time XML found is corrupted on path: {}", time_path);
            fs::remove(time_path);
        }
    }
}

static void save_time_apps(GuiState &gui, EmuEnvState &emuenv) {
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

    const auto time_path{ emuenv.pref_path / "ux0/user/time.xml" };
    const auto save_xml = time_xml.save_file(time_path.c_str());
    if (!save_xml)
        LOG_ERROR("Failed to save xml");
}

void update_time_app_used(GuiState &gui, EmuEnvState &emuenv, const std::string &app) {
    const auto &time_app_index = get_time_app_index(gui, emuenv, app);
    time_app_index->time_used += std::time(nullptr) - time_app_index->last_time_used;

    save_time_apps(gui, emuenv);
}

void update_last_time_app_used(GuiState &gui, EmuEnvState &emuenv, const std::string &app) {
    const auto &time_app_index = get_time_app_index(gui, emuenv, app);
    if (time_app_index != gui.time_apps[emuenv.io.user_id].end())
        time_app_index->last_time_used = std::time(nullptr);
    else // Can't use emplace_back due to Clang 15 for macos
        gui.time_apps[emuenv.io.user_id].push_back({ app, std::time(nullptr), 0 });

    get_app_index(gui, app)->last_time = std::time(nullptr);
    if (gui.users[emuenv.io.user_id].sort_apps_type == LAST_TIME)
        gui.app_selector.is_app_list_sorted = false;

    save_time_apps(gui, emuenv);
}

void delete_app(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    const auto APP_INDEX = get_app_index(gui, app_path);
    const auto &title_id = APP_INDEX->title_id;
    try {
        fs::remove_all(emuenv.pref_path / "ux0/app" / app_path);

        const auto CUSTOM_CONFIG_PATH{ emuenv.config_path / "config" / fmt::format("config_{}.xml", app_path) };
        if (fs::exists(CUSTOM_CONFIG_PATH))
            fs::remove_all(CUSTOM_CONFIG_PATH);
        const auto ADDCONT_PATH{ emuenv.pref_path / "ux0/addcont" / APP_INDEX->addcont };
        if (fs::exists(ADDCONT_PATH))
            fs::remove_all(ADDCONT_PATH);
        const auto LICENSE_PATH{ emuenv.pref_path / "ux0/license" / title_id };
        if (fs::exists(LICENSE_PATH))
            fs::remove_all(LICENSE_PATH);
        const auto PATCH_PATH{ emuenv.pref_path / "ux0/patch" / title_id };
        if (fs::exists(PATCH_PATH))
            fs::remove_all(PATCH_PATH);
        const auto SAVE_DATA_PATH{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "savedata" / APP_INDEX->savedata };
        if (fs::exists(SAVE_DATA_PATH))
            fs::remove_all(SAVE_DATA_PATH);
        const auto SHADER_CACHE_PATH{ emuenv.cache_path / "shaders" / title_id };
        if (fs::exists(SHADER_CACHE_PATH))
            fs::remove_all(SHADER_CACHE_PATH);
        const auto SHADER_LOG_PATH{ emuenv.cache_path / "shaderlog" / title_id };
        if (fs::exists(SHADER_LOG_PATH))
            fs::remove_all(SHADER_LOG_PATH);
        const auto SHADER_LOG_PATH_2{ emuenv.log_path / "shaderlog" / title_id };
        if (fs::exists(SHADER_LOG_PATH_2))
            fs::remove_all(SHADER_LOG_PATH_2);
        const auto EXPORT_TEXTURES_PATH{ emuenv.shared_path / "textures/export" / title_id };
        if (fs::exists(EXPORT_TEXTURES_PATH))
            fs::remove_all(EXPORT_TEXTURES_PATH);
        const auto IMPORT_TEXTURES_PATH{ emuenv.shared_path / "textures/import" / title_id };
        if (fs::exists(IMPORT_TEXTURES_PATH))
            fs::remove_all(IMPORT_TEXTURES_PATH);

        if (gui.app_selector.user_apps_icon.contains(app_path)) {
            gui.app_selector.user_apps_icon[app_path] = {};
            gui.app_selector.user_apps_icon.erase(app_path);
        }

        const auto time_app_index = get_time_app_index(gui, emuenv, app_path);
        if (time_app_index != gui.time_apps[emuenv.io.user_id].end()) {
            gui.time_apps[emuenv.io.user_id].erase(time_app_index);
            save_time_apps(gui, emuenv);
        }

        erase_app_notice(gui, title_id);
        save_notice_list(emuenv);

        LOG_INFO("Application successfully deleted '{} [{}]'.", title_id, APP_INDEX->title);

        gui.app_selector.user_apps.erase(gui.app_selector.user_apps.begin() + (APP_INDEX - &gui.app_selector.user_apps[0]));

        save_apps_cache(gui, emuenv);
    } catch (std::exception &e) {
        LOG_ERROR("Failed to delete '{} [{}]'.\n{}", title_id, APP_INDEX->title, e.what());
    }
}

void open_path(const std::string &path) {
    if (path.starts_with("http")) {
        SDL_OpenURL(path.c_str());
    } else {
#ifdef _WIN32
        static const char OS_PREFIX[] = "start \"Vita3K\" ";
#elif __APPLE__
        static const char OS_PREFIX[] = "open ";
#else
        static const char OS_PREFIX[] = "xdg-open ";
        system((OS_PREFIX + ("\"" + path + "\" & disown")).c_str());
        return;
#endif

        system((OS_PREFIX + ("\"" + path + "\"")).c_str());
    }
}

void draw_app_context_menu(GuiState &gui, EmuEnvState &emuenv, const std::string &app_path) {
    static std::string context_dialog;

    const auto APP_INDEX = get_app_index(gui, app_path);
    const auto &title_id = APP_INDEX->title_id;

    const auto APP_PATH{ emuenv.pref_path / "ux0/app" / app_path };
    const auto CUSTOM_CONFIG_PATH{ emuenv.config_path / "config" / fmt::format("config_{}.xml", app_path) };
    const auto ADDCONT_PATH{ emuenv.pref_path / "ux0/addcont" / APP_INDEX->addcont };
    const auto LICENSE_PATH{ emuenv.pref_path / "ux0/license" / title_id };
    const auto MANUAL_PATH{ APP_PATH / "sce_sys/manual" };
    const auto SAVE_DATA_PATH{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "savedata" / APP_INDEX->savedata };
    const auto SHADER_CACHE_PATH{ emuenv.cache_path / "shaders" / title_id };
    const auto SHADER_LOG_PATH{ emuenv.cache_path / "shaderlog" / title_id };
    const auto EXPORT_TEXTURES_PATH{ emuenv.shared_path / "textures/export" / title_id };
    const auto IMPORT_TEXTURES_PATH{ emuenv.shared_path / "textures/import" / title_id };
    const auto ISSUES_URL = "https://github.com/Vita3K/compatibility/issues";

    const ImVec2 display_size(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);

    const auto is_12_hour_format = emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR;

    auto &lang = gui.lang.app_context;
    auto &app_str = gui.lang.content_manager.application;
    auto &savedata_str = gui.lang.content_manager.saved_data;
    auto &common = emuenv.common_dialog.lang.common;
    auto &lang_compat = gui.lang.compatibility;
    auto &textures = gui.lang.settings_dialog.gpu;

    const auto is_commercial_app = title_id.starts_with("PCS") || (title_id == "NPXS10007");
    const auto is_system_app = title_id.starts_with("NPXS") && (title_id != "NPXS10007");
    const auto has_state_report = gui.compat.compat_db_loaded && gui.compat.app_compat_db.contains(title_id);
    const auto compat_state = has_state_report ? gui.compat.app_compat_db[title_id].state : compat::UNKNOWN;
    const auto &compat_state_color = gui.compat.compat_color[compat_state];
    const auto &compat_state_str = has_state_report ? lang_compat.states[compat_state] : lang_compat.states[compat::UNKNOWN];

    // App Context Menu
    if (ImGui::BeginPopupContextItem("##app_context_menu")) {
        ImGui::SetWindowFontScale(1.1f);
        const auto &START_STR = app_path == emuenv.io.app_path ? gui.lang.live_area.main["continue"] : gui.lang.live_area.main["start"];
        if (ImGui::MenuItem(START_STR.c_str()))
            pre_run_app(gui, emuenv, app_path);
        if (!is_system_app) {
            if (ImGui::BeginMenu(lang_compat.name.c_str())) {
                if (!is_commercial_app || !gui.compat.compat_db_loaded) {
                    if (ImGui::MenuItem(lang.main["check_app_state"].c_str())) {
                        const std::string compat_url = is_commercial_app ? "https://vita3k.org/compatibility?g=" + title_id : "https://github.com/Vita3K/homebrew-compatibility/issues?q=" + APP_INDEX->title;
                        open_path(compat_url);
                    }
                } else {
                    ImGui::Spacing();
                    TextColoredCentered(compat_state_color, compat_state_str.c_str());
                    ImGui::Spacing();
                    if (has_state_report) {
                        tm updated_at_tm = {};
                        SAFE_LOCALTIME(&gui.compat.app_compat_db[title_id].updated_at, &updated_at_tm);
                        auto UPDATED_AT = get_date_time(gui, emuenv, updated_at_tm);
                        ImGui::Spacing();
                        TextColoredCentered(GUI_COLOR_TEXT, fmt::format("{} {} {} {}", lang.info["updated"].c_str(), UPDATED_AT[DateTime::DATE_MINI], UPDATED_AT[DateTime::CLOCK], is_12_hour_format ? UPDATED_AT[DateTime::DAY_MOMENT] : "").c_str());
                    }
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    if (has_state_report) {
                        const auto copy_vita3k_summary = [&]() {
                            const auto vita3k_summary = fmt::format(
                                "# Vita3K summary\n- Version: {}\n- Build number: {}\n- Commit hash: https://github.com/vita3k/vita3k/commit/{}\n- GPU backend: {}",
                                app_version, app_number, app_hash, emuenv.cfg.backend_renderer);
                            ImGui::LogToClipboard();
                            ImGui::LogText("%s", vita3k_summary.c_str());
                            ImGui::LogFinish();
                        };
                        if (ImGui::MenuItem(lang.main["copy_vita3k_summary"].c_str()))
                            copy_vita3k_summary();
                        if (ImGui::MenuItem(lang.main["open_state_report"].c_str())) {
                            copy_vita3k_summary();
                            open_path(fmt::format("{}/{}", ISSUES_URL, gui.compat.app_compat_db[title_id].issue_id));
                        }
                    } else {
                        if (ImGui::MenuItem(lang.main["create_state_report"].c_str())) {
                            // Create body of state report

                            // Encode title for URL
                            const auto encode_title_url = [](std::string title) {
                                const std::map<std::string, std::string> replace_map = {
                                    { "#", "%23" },
                                    { "&", "%26" },
                                    { "+", "%2B" },
                                    { ";", "%3B" }
                                    // Add other replacement associations here if necessary.
                                };

                                // Replace all occurrences found in title
                                for (const auto &[replace, with] : replace_map) {
                                    boost::replace_all(title, replace, with);
                                }

                                return title;
                            };
                            const auto title = encode_title_url(APP_INDEX->title);

                            // Create App summary
                            const auto app_summary = fmt::format(
                                "%23 App summary%0A- App name: {}%0A- App serial: {}%0A- App version: {}",
                                title, title_id, APP_INDEX->app_ver);

                            // Create Vita3K summary
                            const auto vita3k_summary = fmt::format(
                                "%23 Vita3K summary%0A- Version: {}%0A- Build number: {}%0A- Commit hash: https://github.com/vita3k/vita3k/commit/{}%0A- GPU backend: {}",
                                app_version, app_number, app_hash, emuenv.cfg.backend_renderer);

#ifdef _WIN32
                            const auto user = std::getenv("USERNAME");
#else
                            auto user = std::getenv("USER");
#endif // _WIN32

                            // Test environment summary
                            const auto test_env_summary = fmt::format(
                                "%23 Test environment summary%0A- Tested by: {} <!-- Change your username if is needed -->%0A- OS: {}%0A- CPU: {}%0A- GPU: {}%0A- RAM: {} GB",
                                user ? user : "?", CppCommon::Environment::OSVersion(), CppCommon::CPU::Architecture(), emuenv.renderer->get_gpu_name(), SDL_GetSystemRAM() / 1000);

                            const auto rest_of_body = "%23 Issues%0A<!-- Summary of problems -->%0A%0A%23 Screenshots%0A![image](https://?)%0A%0A%23 Log%0A%0A%23 Recommended labels%0A<!-- See https://github.com/Vita3K/compatibility/labels -->%0A- A?%0A- B?%0A- C?";

                            open_path(fmt::format(
                                "{}/new?assignees=&labels=&projects=&template=1-ISSUE_TEMPLATE.md&title={} [{}]&body={}%0A%0A{}%0A%0A{}%0A%0A{}",
                                ISSUES_URL, title, title_id, app_summary, vita3k_summary, test_env_summary, rest_of_body));
                        }
                    }
                }
                if (is_commercial_app && ImGui::MenuItem(lang.main["update_database"].c_str()))
                    load_and_update_compat_user_apps(gui, emuenv);

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(lang.main["copy_app_info"].c_str())) {
                if (ImGui::MenuItem(lang.main["name_and_id"].c_str())) {
                    ImGui::LogToClipboard();
                    ImGui::LogText("%s [%s]", APP_INDEX->title.c_str(), title_id.c_str());
                    ImGui::LogFinish();
                }
                if (ImGui::MenuItem(lang.info["name"].c_str())) {
                    ImGui::LogToClipboard();
                    ImGui::LogText("%s", APP_INDEX->title.c_str());
                    ImGui::LogFinish();
                }
                if (ImGui::MenuItem(lang.info["title_id"].c_str())) {
                    ImGui::LogToClipboard();
                    ImGui::LogText("%s", title_id.c_str());
                    ImGui::LogFinish();
                }
                if (ImGui::MenuItem(lang.main["app_summary"].c_str())) {
                    ImGui::LogToClipboard();
                    ImGui::LogText("# App summary\n- App name: %s\n- App serial: %s\n- App version: %s", APP_INDEX->title.c_str(), title_id.c_str(), APP_INDEX->app_ver.c_str());
                    ImGui::LogFinish();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(lang.main["custom_config"].c_str())) {
                if (!fs::exists(CUSTOM_CONFIG_PATH)) {
                    if (ImGui::MenuItem(lang.main["create"].c_str(), nullptr, &gui.configuration_menu.custom_settings_dialog))
                        init_config(gui, emuenv, app_path);
                } else {
                    if (ImGui::MenuItem(lang.main["edit"].c_str(), nullptr, &gui.configuration_menu.custom_settings_dialog))
                        init_config(gui, emuenv, app_path);
                    if (ImGui::MenuItem(lang.main["remove"].c_str()))
                        fs::remove(CUSTOM_CONFIG_PATH);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(lang.main["open_folder"].c_str())) {
                if (ImGui::MenuItem(app_str["title"].c_str()))
                    open_path(APP_PATH.string());
                if (fs::exists(ADDCONT_PATH) && ImGui::MenuItem(lang.main["addcont"].c_str()))
                    open_path(ADDCONT_PATH.string());
                if (fs::exists(LICENSE_PATH) && ImGui::MenuItem(lang.main["license"].c_str()))
                    open_path(LICENSE_PATH.string());
                if (fs::exists(SAVE_DATA_PATH) && ImGui::MenuItem(savedata_str["title"].c_str()))
                    open_path(SAVE_DATA_PATH.string());
                if (fs::exists(SHADER_CACHE_PATH) && ImGui::MenuItem(lang.main["shaders_cache"].c_str()))
                    open_path(SHADER_CACHE_PATH.string());
                if (fs::exists(SHADER_LOG_PATH) && ImGui::MenuItem(lang.main["shaders_log"].c_str()))
                    open_path(SHADER_LOG_PATH.string());
                if (fs::exists(EXPORT_TEXTURES_PATH) && ImGui::MenuItem(textures["export_textures"].c_str()))
                    open_path(EXPORT_TEXTURES_PATH.string());
                if (fs::exists(IMPORT_TEXTURES_PATH) && ImGui::MenuItem(textures["import_textures"].c_str()))
                    open_path(IMPORT_TEXTURES_PATH.string());
                ImGui::EndMenu();
            }
            if (!emuenv.cfg.show_live_area_screen && ImGui::BeginMenu("Live Area")) {
                if (ImGui::MenuItem("Live Area", nullptr, &gui.vita_area.live_area_screen))
                    open_live_area(gui, emuenv, app_path);
                if (ImGui::MenuItem(common["search"].c_str(), nullptr))
                    open_search(APP_INDEX->title);
                if (fs::exists(MANUAL_PATH) && !fs::is_empty(MANUAL_PATH) && ImGui::MenuItem(lang.main["manual"].c_str(), nullptr))
                    open_manual(gui, emuenv, app_path);
                if (ImGui::MenuItem(lang.main["update"].c_str()))
                    update_app(gui, emuenv, app_path);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(common["delete"].c_str())) {
                if (ImGui::MenuItem(app_str["title"].c_str()))
                    context_dialog = lang.deleting["app_delete"];
                if (fs::exists(ADDCONT_PATH) && ImGui::MenuItem(lang.main["addcont"].c_str()))
                    context_dialog = lang.deleting["addcont_delete"];
                if (fs::exists(LICENSE_PATH) && ImGui::MenuItem(lang.main["license"].c_str()))
                    context_dialog = lang.deleting["license_delete"];
                if (fs::exists(SAVE_DATA_PATH) && ImGui::MenuItem(savedata_str["title"].c_str()))
                    context_dialog = lang.deleting["saved_data_delete"];
                if (fs::exists(SHADER_CACHE_PATH) && ImGui::MenuItem(lang.main["shaders_cache"].c_str()))
                    fs::remove_all(SHADER_CACHE_PATH);
                if (fs::exists(SHADER_LOG_PATH) && ImGui::MenuItem(lang.main["shaders_log"].c_str()))
                    fs::remove_all(SHADER_LOG_PATH);
                if (fs::exists(EXPORT_TEXTURES_PATH) && ImGui::MenuItem(textures["export_textures"].c_str()))
                    fs::remove_all(EXPORT_TEXTURES_PATH);
                if (fs::exists(IMPORT_TEXTURES_PATH) && ImGui::MenuItem(textures["import_textures"].c_str()))
                    fs::remove_all(IMPORT_TEXTURES_PATH);
                ImGui::EndMenu();
            }
            if (fs::exists(APP_PATH / "sce_sys/changeinfo/") && ImGui::MenuItem(lang.main["update_history"].c_str())) {
                if (get_update_history(gui, emuenv, app_path))
                    context_dialog = "history";
                else
                    LOG_WARN("Patch note Error for Title ID {} in path {}", title_id, app_path);
            }
        }
        if (ImGui::MenuItem(lang.info["title"].c_str(), nullptr, &gui.vita_area.app_information)) {
            if (!is_system_app) {
                get_app_info(gui, emuenv, app_path);
                const auto app_size = get_app_size(gui, emuenv, app_path);
                gui.app_selector.app_info.size = app_size;
            }
            gui.vita_area.information_bar = false;
        }
        ImGui::EndPopup();
    }

    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    const auto WINDOW_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);

    const auto BUTTON_SIZE = ImVec2(320.f * SCALE.x, 46.f * SCALE.y);
    const auto PUPOP_ICON_SIZE = ImVec2(96.f * SCALE.x, 96.f * SCALE.y);
    const auto INFO_ICON_SIZE = ImVec2(128.f * SCALE.x, 128.f * SCALE.y);

    // Context Dialog
    if (!context_dialog.empty()) {
        ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
        ImGui::Begin("##context_dialog", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetNextWindowBgAlpha(0.999f);
        ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x + (display_size.x / 2.f) - (WINDOW_SIZE.x / 2.f), emuenv.logical_viewport_pos.y + (display_size.y / 2.f) - (WINDOW_SIZE.y / 2.f)), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
        ImGui::BeginChild("##context_dialog_child", WINDOW_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
        // Update History
        if (context_dialog == "history") {
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetWindowPos().x + 20.f * SCALE.x, ImGui::GetWindowPos().y + BUTTON_SIZE.y));
            ImGui::BeginChild("##info_update_list", ImVec2(WINDOW_SIZE.x - (30.f * SCALE.x), WINDOW_SIZE.y - (BUTTON_SIZE.y * 2.f) - (25.f * SCALE.y)), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
            // Reverse iterator to show the latest update first
            for (auto it = update_history_infos.rbegin(); it != update_history_infos.rend(); ++it) {
                ImGui::SetWindowFontScale(1.3f);
                const auto version_str = fmt::format(fmt::runtime(lang.main["history_version"]), it->first);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", version_str.c_str());
                ImGui::SetWindowFontScale(0.9f);
                ImGui::PushTextWrapPos(WINDOW_SIZE.x - (80.f * SCALE.x));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s\n", it->second.c_str());
                ImGui::PopTextWrapPos();
                ImGui::TextColored(GUI_COLOR_TEXT, "\n");
            }
            ImGui::EndChild();
            ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
            ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (BUTTON_SIZE.x / 2.f), WINDOW_SIZE.y - BUTTON_SIZE.y - (22.f * SCALE.y)));
        } else {
            // Delete Data
            const auto ICON_MARGIN = 24.f * SCALE.y;
            if (gui.app_selector.user_apps_icon.contains(title_id)) {
                ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (PUPOP_ICON_SIZE.x / 2.f), ICON_MARGIN));
                const auto POS_MIN = ImGui::GetCursorScreenPos();
                const ImVec2 POS_MAX(POS_MIN.x + PUPOP_ICON_SIZE.x, POS_MIN.y + PUPOP_ICON_SIZE.y);
                ImGui::GetWindowDrawList()->AddImageRounded(gui.app_selector.user_apps_icon[title_id], POS_MIN, POS_MAX, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, PUPOP_ICON_SIZE.x * SCALE.x, ImDrawFlags_RoundCornersAll);
            }
            ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);
            ImGui::SetCursorPosY(ICON_MARGIN + PUPOP_ICON_SIZE.y + (4.f * SCALE.y));
            TextColoredCentered(GUI_COLOR_TEXT, APP_INDEX->stitle.c_str());
            ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
            ImGui::SetCursorPosY((WINDOW_SIZE.y / 2) + 10);
            TextColoredCentered(GUI_COLOR_TEXT, context_dialog.c_str(), 54.f * SCALE.x);
            if (context_dialog == lang.deleting["app_delete"])
                SetTooltipEx(lang.deleting["app_delete_description"].c_str());
            ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
            ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * SCALE.x)), WINDOW_SIZE.y - BUTTON_SIZE.y - (24.0f * SCALE.y)));
            if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle))) {
                context_dialog.clear();
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) + (20.f * SCALE.x));
        }
        if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross))) {
            if (context_dialog == lang.deleting["app_delete"])
                delete_app(gui, emuenv, app_path);
            if (context_dialog == lang.deleting["addcont_delete"])
                fs::remove_all(ADDCONT_PATH);
            if (context_dialog == lang.deleting["license_delete"])
                fs::remove_all(LICENSE_PATH);
            else if (context_dialog == lang.deleting["saved_data_delete"])
                fs::remove_all(SAVE_DATA_PATH);
            context_dialog.clear();
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::End();
    }

    // Information
    if (gui.vita_area.app_information) {
        ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y), ImGuiCond_Always);
        ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
        ImGui::Begin("##information", &gui.vita_area.app_information, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        ImGui::SetWindowFontScale(1.5f * RES_SCALE.x);
        ImGui::SetCursorPos(ImVec2(10.0f * SCALE.x, 10.0f * SCALE.y));
        if (ImGui::Button("X", ImVec2(40.f * SCALE.x, 40.f * SCALE.y)) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle))) {
            gui.vita_area.app_information = false;
            gui.vita_area.information_bar = true;
        }
        if (get_app_icon(gui, title_id)->first == title_id) {
            ImGui::SetCursorPos(ImVec2((display_size.x / 2.f) - (INFO_ICON_SIZE.x / 2.f), 22.f * SCALE.x));
            const auto POS_MIN = ImGui::GetCursorScreenPos();
            const ImVec2 POS_MAX(POS_MIN.x + INFO_ICON_SIZE.x, POS_MIN.y + INFO_ICON_SIZE.y);
            ImGui::GetWindowDrawList()->AddImageRounded(get_app_icon(gui, title_id)->second, POS_MIN, POS_MAX, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, INFO_ICON_SIZE.x * SCALE.x, ImDrawFlags_RoundCornersAll);
        }
        ImGui::SetCursorPos(ImVec2((display_size.x / 2.f) - ImGui::CalcTextSize((lang.info["name"] + "  ").c_str()).x, INFO_ICON_SIZE.y + (50.f * SCALE.y)));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s ", lang.info["name"].c_str());
        ImGui::SameLine();
        ImGui::PushTextWrapPos(display_size.x - (85.f * SCALE.x));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", APP_INDEX->title.c_str());
        ImGui::PopTextWrapPos();
        if (!is_system_app && (title_id != "NPXS10007")) {
            ImGui::Spacing();
            ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((lang.info["trophy_earning"] + "  ").c_str()).x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s  %s", lang.info["trophy_earning"].c_str(), gui.app_selector.app_info.trophy.c_str());
            ImGui::Spacing();
            ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((lang.info["parental_controls"] + "  ").c_str()).x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s  %s", lang.info["parental_controls"].c_str(), lang.info["level"].c_str());
            ImGui::SameLine();
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", APP_INDEX->parental_level.c_str());
            ImGui::Spacing();
            ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((lang.info["updated"] + "  ").c_str()).x);
            auto DATE_TIME = get_date_time(gui, emuenv, gui.app_selector.app_info.updated);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s  %s %s", lang.info["updated"].c_str(), DATE_TIME[DateTime::DATE_MINI].c_str(), DATE_TIME[DateTime::CLOCK].c_str());
            if (is_12_hour_format) {
                ImGui::SameLine();
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", DATE_TIME[DateTime::DAY_MOMENT].c_str());
            }
            ImGui::Spacing();
            ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((lang.info["size"] + "  ").c_str()).x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", (lang.info["size"] + "  " + get_unit_size(gui.app_selector.app_info.size)).c_str());
            ImGui::Spacing();
            ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((lang.info["version"] + "  ").c_str()).x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s  %s", lang.info["version"].c_str(), APP_INDEX->app_ver.c_str());
            ImGui::Spacing();
            ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((lang.info["title_id"] + "  ").c_str()).x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s  %s", lang.info["title_id"].c_str(), APP_INDEX->title_id.c_str());
            ImGui::Spacing();
            ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((lang_compat.name + "  ").c_str()).x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang_compat.name.c_str());
            ImGui::SameLine();
            ImGui::TextColored(compat_state_color, " %s", compat_state_str.c_str());
            ImGui::Spacing();
            ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((lang.info["last_time_used"] + "  ").c_str()).x);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s ", lang.info["last_time_used"].c_str());
            ImGui::SameLine();
            const auto time_app_index = get_time_app_index(gui, emuenv, app_path);
            if (time_app_index != gui.time_apps[emuenv.io.user_id].end()) {
                tm date_tm = {};
                SAFE_LOCALTIME(&time_app_index->last_time_used, &date_tm);
                auto LAST_TIME = get_date_time(gui, emuenv, date_tm);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", LAST_TIME[DateTime::DATE_MINI].c_str(), LAST_TIME[DateTime::CLOCK].c_str());
                if (is_12_hour_format) {
                    ImGui::SameLine();
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", LAST_TIME[DateTime::DAY_MOMENT].c_str());
                }
                ImGui::Spacing();
                ImGui::SetCursorPosX((display_size.x / 2.f) - ImGui::CalcTextSize((lang.info["time_used"] + "  ").c_str()).x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s  %s", lang.info["time_used"].c_str(), get_time_app_used(gui, time_app_index->time_used).c_str());
            } else
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang.info["never"].c_str());
        }
        ImGui::End();
    }
}

} // namespace gui
