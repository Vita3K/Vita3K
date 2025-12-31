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

#include <lang/functions.h>
#include <lang/state.h>

#include <config/state.h>
#include <dialog/state.h>
#include <gui/state.h>
#include <ime/state.h>
#include <util/fs.h>
#include <util/vector_utils.h>

#include <pugixml.hpp>

namespace lang {

static const std::vector<std::string> list_user_lang_static = {
    "id", "ms", "ua"
};

void init_lang(LangState &lang, EmuEnvState &emuenv) {
    lang = {};
    emuenv.common_dialog.lang = {};
    emuenv.ime.lang = {};

    const auto set_lang = [&](const std::string &language) {
        lang.user_lang[GUI] = language;
        lang.user_lang[LIVE_AREA] = language;
    };

    const auto sys_lang = static_cast<SceSystemParamLang>(emuenv.cfg.sys_lang);
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

    const auto system_lang_path{ emuenv.static_assets_path / "lang/system" };
    const auto user_lang_shared_path{ emuenv.shared_path / "lang/user" };
    const auto user_lang_static_path{ emuenv.static_assets_path / "lang/user" };

    // Create user lang folder if not exists and create a file to indicate where to place the lang files
    if (!fs::exists(user_lang_shared_path)) {
        fs::create_directories(user_lang_shared_path);
        fs::ofstream outfile(user_lang_shared_path / "PLACE USER LANG HERE.txt");
        outfile.close();
    }

    const auto is_user_lang_static = vector_utils::contains(list_user_lang_static, emuenv.cfg.user_lang);

    // Load lang xml
    pugi::xml_document lang_xml;
    const auto lang_xml_path{ (emuenv.cfg.user_lang.empty() ? system_lang_path / lang.user_lang[GUI] : (is_user_lang_static ? user_lang_static_path : user_lang_shared_path) / emuenv.cfg.user_lang).replace_extension("xml") };
    std::vector<uint8_t> lang_content{};
    if (fs_utils::read_data(lang_xml_path, lang_content)) {
        const auto load_xml_res = lang_xml.load_buffer(lang_content.data(), lang_content.size(), pugi::encoding_utf8);
        if (load_xml_res) {
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

                    // Debug Menu
                    set_lang_string(lang.main_menubar.debug, main_menubar.child("debug"));

                    // Configuration Menu
                    set_lang_string(lang.main_menubar.configuration, main_menubar.child("configuration"));

                    // Controls Menu
                    set_lang_string(lang.main_menubar.controls, main_menubar.child("controls"));

                    // Help Menu
                    set_lang_string(lang.main_menubar.help, main_menubar.child("help"));
                }

                // About
                set_lang_string(lang.about, lang_child.child("about"));

                // App Context
                const auto app_context = lang_child.child("app_context");
                if (!app_context.empty()) {
                    // Main
                    set_lang_string(lang.app_context.main, app_context);

                    // Delete
                    set_lang_string(lang.app_context.deleting, app_context.child("delete"));

                    // Information
                    set_lang_string(lang.app_context.info, app_context.child("info"));

                    // Time Used
                    set_lang_string(lang.app_context.time_used, app_context.child("time_used"));
                }

                // Common
                const auto common = lang_child.child("common");
                if (!common.empty()) {
                    set_lang_string(emuenv.common_dialog.lang.common, common);
                    set_lang_string(lang.common.main, common);

                    const auto set_calendar = [](std::vector<std::string> &dest, const pugi::xml_node child) {
                        if (!child.empty()) {
                            dest.clear();
                            for (const auto day : child)
                                dest.emplace_back(day.text().as_string());
                        }
                    };

                    // Day of the week
                    set_calendar(lang.common.wday, common.child("wday"));

                    // Months of the year
                    set_calendar(lang.common.ymonth, common.child("ymonth"));

                    // Small months of the year
                    set_calendar(lang.common.small_ymonth, common.child("small_ymonth"));

                    // Days of the month
                    set_calendar(lang.common.mday, common.child("mday"));

                    // Small days of the month
                    set_calendar(lang.common.small_mday, common.child("small_mday"));
                }

                // Compatibility
                const auto compatibility_child = lang_child.child("compatibility");
                if (!compatibility_child.empty()) {
                    auto &lang_compatibility = lang.compatibility;
                    // Name
                    if (!compatibility_child.attribute("name").empty())
                        lang_compatibility.name = compatibility_child.attribute("name").as_string();

                    // States
                    const auto states = compatibility_child.child("states");
                    if (!states.empty()) {
                        for (const auto state : states) {
                            const auto id = static_cast<compat::CompatibilityState>(state.attribute("id").as_int());
                            lang_compatibility.states[id] = state.text().as_string();
                        }
                    }
                }

                // Compatibility Database
                set_lang_string(lang.compat_db, lang_child.child("compat_db"));

                // Compile Shaders
                set_lang_string(lang.compile_shaders, lang_child.child("compile_shaders"));

                // Content Manager
                const auto content_manager = lang_child.child("content_manager");
                if (!content_manager.empty()) {
                    // Main
                    set_lang_string(lang.content_manager.main, content_manager);

                    // Application
                    set_lang_string(lang.content_manager.application, content_manager.child("application"));

                    // Saved Data
                    set_lang_string(lang.content_manager.saved_data, content_manager.child("saved_data"));
                }

                // Controllers
                set_lang_string(lang.controllers, lang_child.child("controllers"));

                // Controls
                set_lang_string(lang.controls, lang_child.child("controls"));

                // Dialog
                const auto dialog = lang_child.child("dialog");
                if (!dialog.empty()) {
                    // Trophy
                    set_lang_string(emuenv.common_dialog.lang.trophy, dialog.child("trophy"));

                    // Save Data
                    const auto save_data = dialog.child("save_data");
                    if (!save_data.empty()) {
                        auto &lang_save_data = emuenv.common_dialog.lang.save_data;
                        // Delete
                        set_lang_string(lang_save_data.deleting, save_data.child("delete"));

                        // Info
                        set_lang_string(lang_save_data.info, save_data.child("info"));

                        // Load
                        set_lang_string(lang_save_data.load, save_data.child("load"));

                        // Save
                        set_lang_string(lang_save_data.save, save_data.child("save"));
                    }
                }

                // Game Data
                set_lang_string(lang.game_data, lang_child.child("game_data"));

                // Home Screen
                set_lang_string(lang.home_screen, lang_child.child("home_screen"));

                // Indicator
                set_lang_string(lang.indicator, lang_child.child("indicator"));

                // Initial Setup
                if (!emuenv.cfg.initial_setup)
                    set_lang_string(lang.initial_setup, lang_child.child("initial_setup"));

                // Install Dialog
                const auto install_dialog = lang_child.child("install_dialog");
                if (!install_dialog.empty()) {
                    // Firmware Install
                    set_lang_string(lang.install_dialog.firmware_install, install_dialog.child("firmware_install"));

                    // Package Install
                    set_lang_string(lang.install_dialog.pkg_install, install_dialog.child("pkg_install"));

                    // Archive Install
                    set_lang_string(lang.install_dialog.archive_install, install_dialog.child("archive_install"));

                    // License Install
                    set_lang_string(lang.install_dialog.license_install, install_dialog.child("license_install"));

                    // Reinstall
                    set_lang_string(lang.install_dialog.reinstall, install_dialog.child("reinstall"));
                }

                // Live Area
                const auto live_area = lang_child.child("live_area");
                if (!live_area.empty()) {
                    // Main
                    set_lang_string(lang.live_area.main, live_area);

                    // Help
                    set_lang_string(lang.live_area.help, live_area.child("help"));
                }

                // Message
                set_lang_string(emuenv.common_dialog.lang.message, lang_child.child("message"));

                // Patch Check
                set_lang_string(lang.patch_check, lang_child.child("patch_check"));

                // Performance Overlay
                set_lang_string(lang.performance_overlay, lang_child.child("performance_overlay"));

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

                        // Home Screen Backgrounds
                        set_lang_string(lang.settings.theme_background.home_screen_backgrounds, theme_background.child("home_screen_backgrounds"));
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

                    // Language
                    const auto language = settings.child("language");
                    if (!language.empty()) {
                        // Main
                        auto &lang_settings = lang.settings.language;
                        set_lang_string(lang.settings.language.main, language);

                        // Input Language
                        const auto input_language = language.child("input_language");
                        if (!input_language.empty()) {
                            // Main
                            set_lang_string(lang_settings.input_language, input_language);

                            // Keyboards
                            const auto keyboards = input_language.child("keyboards");
                            if (!keyboards.empty()) {
                                set_lang_string(lang_settings.keyboards, keyboards);
                                auto &lang_ime = emuenv.ime.lang.ime_keyboards;
                                const auto &keyboard_lang_ime = keyboards.child("ime_languages");
                                if (!keyboard_lang_ime.empty()) {
                                    lang_ime.clear();
                                    const auto op = [](const auto &lang) {
                                        return std::make_pair(static_cast<SceImeLanguage>(lang.attribute("id").as_ullong()), lang.text().as_string());
                                    };
                                    std::transform(std::begin(keyboard_lang_ime), std::end(keyboard_lang_ime), std::back_inserter(lang_ime), op);
                                }
                            }
                        }
                    }
                }

                // Settings Dialog
                const auto settings_dialog = lang_child.child("settings_dialog");
                if (!settings_dialog.empty()) {
                    // Main
                    set_lang_string(lang.settings_dialog.main_window, settings_dialog);

                    // Core
                    set_lang_string(lang.settings_dialog.core, settings_dialog.child("core"));

                    // CPU
                    set_lang_string(lang.settings_dialog.cpu, settings_dialog.child("cpu"));

                    // GPU
                    set_lang_string(lang.settings_dialog.gpu, settings_dialog.child("gpu"));

                    // Audio
                    set_lang_string(lang.settings_dialog.audio, settings_dialog.child("audio"));

                    // System
                    set_lang_string(lang.settings_dialog.system, settings_dialog.child("system"));

                    // Emulator
                    set_lang_string(lang.settings_dialog.emulator, settings_dialog.child("emulator"));

                    // GUI
                    set_lang_string(lang.settings_dialog.gui, settings_dialog.child("gui"));

                    // Network
                    set_lang_string(lang.settings_dialog.network, settings_dialog.child("network"));

                    // Debug
                    set_lang_string(lang.settings_dialog.debug, settings_dialog.child("debug"));
                }

                // System Applications Title
                set_lang_string(lang.sys_apps_title, lang_child.child("sys_apps_title"));

                // Trophy Collection
                set_lang_string(lang.trophy_collection, lang_child.child("trophy_collection"));

                // User Management
                set_lang_string(lang.user_management, lang_child.child("user_management"));

                // Vita3k Update
                set_lang_string(lang.vita3k_update, lang_child.child("vita3k_update"));

                // Welcome
                set_lang_string(lang.welcome, lang_child.child("welcome"));
            }
        } else {
            LOG_ERROR("Error parsing lang file xml: {}", lang_xml_path);
            LOG_DEBUG("error: {} position: {}", load_xml_res.description(), load_xml_res.offset);
            constexpr ptrdiff_t context_window = 20;
            ptrdiff_t offset = static_cast<ptrdiff_t>(load_xml_res.offset);
            if (offset >= 0 && offset < static_cast<ptrdiff_t>(lang_content.size())) {
                ptrdiff_t start = std::max<ptrdiff_t>(0, offset - context_window);
                ptrdiff_t end = std::min<ptrdiff_t>(lang_content.size(), offset + context_window);

                ptrdiff_t error_in_context = offset - start;

                std::string error_context(reinterpret_cast<const char *>(lang_content.data() + start), end - start);

                LOG_DEBUG("Error preview: {}|{}", error_context.substr(0, error_in_context), error_context.substr(error_in_context));
            }
        }
    } else
        LOG_ERROR("Lang file xml not found: {}", lang_xml_path);
}

} // namespace lang
