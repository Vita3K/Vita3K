// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include <lang/functions.h>
#include <lang/state.h>

#include <pugixml.hpp>

namespace lang {
void init_lang(LangState &lang, HostState &host) {
    lang = {};
    host.common_dialog.lang = {};
    host.ime.lang = {};

    const auto set_lang = [&](std::string language) {
        lang.user_lang[GUI] = language;
        lang.user_lang[LIVE_AREA] = language;
    };

    const auto sys_lang = static_cast<SceSystemParamLang>(host.cfg.sys_lang);
    switch (sys_lang) {
    case SCE_SYSTEM_PARAM_LANG_JAPANESE: set_lang("ja"); break;
    case SCE_SYSTEM_PARAM_LANG_ENGLISH_US: set_lang("en"); break;
    case SCE_SYSTEM_PARAM_LANG_FRENCH: set_lang("fr"); break;
    case SCE_SYSTEM_PARAM_LANG_SPANISH: set_lang("es"); break;
    case SCE_SYSTEM_PARAM_LANG_GERMAN: set_lang("de"); break;
    case SCE_SYSTEM_PARAM_LANG_ITALIAN: set_lang("it"); break;
    case SCE_SYSTEM_PARAM_LANG_DUTCH: set_lang("nl"); break;
    case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT: set_lang("pt"); break;
    case SCE_SYSTEM_PARAM_LANG_RUSSIAN: set_lang("ru"); break;
    case SCE_SYSTEM_PARAM_LANG_KOREAN: set_lang("ko"); break;
    case SCE_SYSTEM_PARAM_LANG_CHINESE_T:
        lang.user_lang[GUI] = "zh-t";
        lang.user_lang[LIVE_AREA] = "ch";
        break;
    case SCE_SYSTEM_PARAM_LANG_CHINESE_S:
        lang.user_lang[GUI] = "zh-s";
        lang.user_lang[LIVE_AREA] = "zh";
        break;
    case SCE_SYSTEM_PARAM_LANG_FINNISH: set_lang("fi"); break;
    case SCE_SYSTEM_PARAM_LANG_SWEDISH: set_lang("sv"); break;
    case SCE_SYSTEM_PARAM_LANG_DANISH: set_lang("da"); break;
    case SCE_SYSTEM_PARAM_LANG_NORWEGIAN: set_lang("no"); break;
    case SCE_SYSTEM_PARAM_LANG_POLISH: set_lang("pl"); break;
    case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR: set_lang("pt-br"); break;
    case SCE_SYSTEM_PARAM_LANG_ENGLISH_GB: set_lang("en-gb"); break;
    case SCE_SYSTEM_PARAM_LANG_TURKISH: set_lang("tr"); break;
    default: break;
    }

    pugi::xml_document lang_xml;
    const auto lang_path{ fs::path(host.base_path) / "lang" };
    const auto lang_xml_path = (lang_path / (lang.user_lang[GUI] + ".xml")).string();
    if (fs::exists(lang_xml_path)) {
        if (lang_xml.load_file(lang_xml_path.c_str())) {
            // Lang
            const auto lang_child = lang_xml.child("lang");
            if (!lang_child.empty()) {
                const auto set_lang_string = [](std::map<std::string, std::string> &lang_dest, const pugi::xml_node child) {
                    if (!child.empty()) {
                        for (auto &dest : lang_dest) {
                            if (dest.first == "title") {
                                if (!child.attribute("name").empty())
                                    dest.second = child.attribute("name").as_string();
                            } else {
                                const auto id = dest.first.c_str();
                                if (!child.child(id).empty())
                                    dest.second = child.child(id).text().as_string();
                            }
                        }
                    }
                };

                // Main Menu Bar
                const auto main_menubar = lang_child.child("main_menubar");
                if (!main_menubar.empty()) {
                    // File Menu
                    set_lang_string(lang.main_menubar.file, main_menubar.child("file"));

                    // Emulation Menu
                    set_lang_string(lang.main_menubar.emulation, main_menubar.child("emulation"));

                    // Configuration Menu
                    set_lang_string(lang.main_menubar.configuration, main_menubar.child("configuration"));

                    // Controls Menu
                    set_lang_string(lang.main_menubar.controls, main_menubar.child("controls"));

                    // Help Menu
                    set_lang_string(lang.main_menubar.help, main_menubar.child("help"));
                }

                // App Context
                set_lang_string(lang.app_context, lang_child.child("app_context"));

                // Common
                const auto common = lang_child.child("common");
                if (!common.empty()) {
                    set_lang_string(host.common_dialog.lang.common, common);
                    set_lang_string(lang.common.main, common);

                    const auto set_calendar = [](std::vector<std::string> &dest, const pugi::xml_node child) {
                        if (!child.empty()) {
                            dest.clear();
                            for (const auto day : child)
                                dest.push_back(day.text().as_string());
                        }
                    };

                    // Day of the week
                    set_calendar(lang.common.wday, common.child("wday"));

                    // Months of the year
                    set_calendar(lang.common.ymonth, common.child("ymonth"));

                    // Small months of the year
                    set_calendar(lang.common.small_ymonth, common.child("small_ymonth"));
                }

                // Dialog
                const auto dialog = lang_child.child("dialog");
                if (!dialog.empty()) {
                    // Trophy
                    set_lang_string(host.common_dialog.lang.trophy, dialog.child("trophy"));

                    // Save Data
                    const auto save_data = dialog.child("save_data");
                    if (!save_data.empty()) {
                        auto &lang_save_data = host.common_dialog.lang.save_data;
                        // Delete
                        set_lang_string(lang_save_data.deleting, save_data.child("delete"));

                        // Info
                        set_lang_string(lang_save_data.info, save_data.child("info"));

                        // Load
                        set_lang_string(lang_save_data.load, save_data.child("load"));

                        // Save
                        set_lang_string(lang_save_data.save, save_data.child("Save"));
                    }
                }

                // Game Data
                set_lang_string(lang.game_data, lang_child.child("game_data"));

                // Indicator
                set_lang_string(lang.indicator, lang_child.child("indicator"));

                // Initial Setup
                if (!host.cfg.initial_setup)
                    set_lang_string(lang.initial_setup, lang_child.child("initial_setup"));

                // Live Area
                const auto live_area = lang_child.child("live_area");
                if (!live_area.empty()) {
                    lang.live_area[START] = live_area.child("start").text().as_string();
                    lang.live_area[CONTINUE] = live_area.child("continue").text().as_string();
                }

                // Settings
                const auto settings = lang_child.child("settings");
                if (!settings.empty()) {
                    // Main
                    set_lang_string(lang.settings.main, settings);

                    // Theme & Background
                    const auto theme_background = settings.child("theme_background");
                    if (!theme_background.empty()) {
                        set_lang_string(lang.settings.theme_background.main, theme_background);

                        // Theme
                        const auto theme = theme_background.child("theme");
                        if (!theme.empty()) {
                            set_lang_string(lang.settings.theme_background.theme.main, theme);

                            // Information
                            set_lang_string(lang.settings.theme_background.theme.information, theme.child("information"));
                        }

                        // Start Screen
                        set_lang_string(lang.settings.theme_background.start_screen, theme_background.child("start_screen"));
                    }

                    // Date & Time
                    const auto date_time = settings.child("date_time");
                    if (!date_time.empty()) {
                        // Main
                        set_lang_string(lang.settings.date_time.main, date_time);

                        // Date Format
                        set_lang_string(lang.settings.date_time.date_format, date_time.child("date_format"));

                        // Time Format
                        set_lang_string(lang.settings.date_time.time_format, date_time.child("time_format"));
                    }

                    // Languague
                    const auto language = settings.child("language");
                    if (!language.empty()) {
                        // Main
                        auto &lang_settings = lang.settings.language;
                        set_lang_string(lang.settings.language.main, language);

                        // Input Languague
                        const auto input_language = language.child("input_language");
                        if (!input_language.empty()) {
                            // Main
                            set_lang_string(lang_settings.input_langague, input_language);

                            // Keyboards
                            const auto keyboards = input_language.child("keyboards");
                            if (!keyboards.empty()) {
                                set_lang_string(lang_settings.Keyboards, input_language);
                                auto &lang_ime = host.ime.lang.ime_keyboards;
                                const auto &keyboard_lang_ime = keyboards.child("ime_langagues");
                                if (!keyboard_lang_ime.empty()) {
                                    lang_ime.clear();
                                    const auto op = [](const auto &lang) {
                                        return std::make_pair(SceImeLanguage(lang.attribute("id").as_ullong()), lang.text().as_string());
                                    };
                                    std::transform(std::begin(keyboard_lang_ime), std::end(keyboard_lang_ime), std::back_inserter(lang_ime), op);
                                }
                            }
                        }
                    }
                }

                // Trophy Collection
                set_lang_string(lang.trophy_collection, lang_child.child("trophy_collection"));

                // User Management
                set_lang_string(lang.user_management, lang_child.child("user_management"));
            }
        } else
            LOG_ERROR("Error open lang file xml: {}", lang_xml_path);
    }
}

} // namespace lang
