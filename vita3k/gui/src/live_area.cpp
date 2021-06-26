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
#include <host/functions.h>

#include <util/safe_time.h>

#include <io/VitaIoDevice.h>
#include <io/vfs.h>
#include <util/log.h>

#include <pugixml.hpp>

#include <chrono>
#include <sstream>
#include <stb_image.h>

namespace gui {
static std::string start, resume;

void init_lang(GuiState &gui, HostState &host) {
    start.clear(), resume.clear();
    gui.lang = {};
    auto &user_lang = gui.lang.user_lang;
    const auto sys_lang = static_cast<SceSystemParamLang>(host.cfg.sys_lang);
    switch (sys_lang) {
    case SCE_SYSTEM_PARAM_LANG_JAPANESE: user_lang = "ja"; break;
    case SCE_SYSTEM_PARAM_LANG_ENGLISH_US: user_lang = "en"; break;
    case SCE_SYSTEM_PARAM_LANG_FRENCH: user_lang = "fr"; break;
    case SCE_SYSTEM_PARAM_LANG_SPANISH: user_lang = "es"; break;
    case SCE_SYSTEM_PARAM_LANG_GERMAN: user_lang = "de"; break;
    case SCE_SYSTEM_PARAM_LANG_ITALIAN: user_lang = "it"; break;
    case SCE_SYSTEM_PARAM_LANG_DUTCH: user_lang = "nl"; break;
    case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT: user_lang = "pt"; break;
    case SCE_SYSTEM_PARAM_LANG_RUSSIAN: user_lang = "ru"; break;
    case SCE_SYSTEM_PARAM_LANG_KOREAN: user_lang = "ko"; break;
    case SCE_SYSTEM_PARAM_LANG_CHINESE_T: user_lang = "zh-t"; break;
    case SCE_SYSTEM_PARAM_LANG_CHINESE_S: user_lang = "zh-s"; break;
    case SCE_SYSTEM_PARAM_LANG_FINNISH: user_lang = "fi"; break;
    case SCE_SYSTEM_PARAM_LANG_SWEDISH: user_lang = "sv"; break;
    case SCE_SYSTEM_PARAM_LANG_DANISH: user_lang = "da"; break;
    case SCE_SYSTEM_PARAM_LANG_NORWEGIAN: user_lang = "no"; break;
    case SCE_SYSTEM_PARAM_LANG_POLISH: user_lang = "pl"; break;
    case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR: user_lang = "pt-br"; break;
    case SCE_SYSTEM_PARAM_LANG_ENGLISH_GB: user_lang = "en-gb"; break;
    case SCE_SYSTEM_PARAM_LANG_TURKISH: user_lang = "tr"; break;
    default: break;
    }

    pugi::xml_document lang_xml;
    const auto lang_path{ fs::path(host.base_path) / "lang" };
    const auto lang_xml_path = (lang_path / (user_lang + ".xml")).string();
    if (fs::exists(lang_xml_path)) {
        if (lang_xml.load_file(lang_xml_path.c_str())) {
            // Lang
            const auto lang_child = lang_xml.child("lang");
            if (!lang_child.empty()) {
                // Main Menu Bar
                const auto main_menubar = lang_child.child("main_menubar");
                if (!main_menubar.empty()) {
                    // File Menu
                    const auto file = main_menubar.child("file");
                    auto &lang_main_menubar = gui.lang.main_menubar;
                    if (!file.empty()) {
                        lang_main_menubar["file"] = file.attribute("name").as_string();
                        lang_main_menubar["open_pref_path"] = file.child("open_pref_path").text().as_string();
                        lang_main_menubar["install_firmware"] = file.child("install_firmware").text().as_string();
                        lang_main_menubar["install_pkg"] = file.child("install_pkg").text().as_string();
                        lang_main_menubar["install_zip"] = file.child("install_zip").text().as_string();
                        lang_main_menubar["install_license"] = file.child("install_license").text().as_string();
                    }

                    // Emulation Menu
                    if (!main_menubar.child("emulation").empty()) {
                        lang_main_menubar["emulation"] = main_menubar.child("emulation").attribute("name").as_string();
                        lang_main_menubar["last_apps_used"] = main_menubar.child("emulation").child("last_apps_used").text().as_string();
                    }

                    // Configuration Menu
                    const auto configuration = main_menubar.child("configuration");
                    if (!configuration.empty()) {
                        lang_main_menubar["configuration"] = configuration.attribute("name").as_string();
                        lang_main_menubar["settings"] = configuration.child("settings").text().as_string();
                        lang_main_menubar["user_management"] = configuration.child("user_management").text().as_string();
                    }

                    // Controls Menu
                    if (!main_menubar.child("controls").empty()) {
                        lang_main_menubar["controls"] = main_menubar.child("controls").attribute("name").as_string();
                        gui.lang.main_menubar["keyboard_controls"] = main_menubar.child("controls").child("keyboard_controls").text().as_string();
                    }

                    // Help Menu
                    if (!main_menubar.child("help").empty()) {
                        lang_main_menubar["help"] = main_menubar.child("help").attribute("name").as_string();
                        lang_main_menubar["about"] = main_menubar.child("help").child("about").text().as_string();
                        lang_main_menubar["welcome"] = main_menubar.child("help").child("welcome").text().as_string();
                    }
                }

                // App Context
                const auto app_context = lang_child.child("app_context");
                if (!app_context.empty()) {
                    auto &lang_app_context = gui.lang.app_context;
                    lang_app_context["update_history"] = app_context.child("update_history").text().as_string();
                    lang_app_context["information"] = app_context.child("information").text().as_string();
                    lang_app_context["app_delete"] = app_context.child("app_delete").text().as_string();
                    lang_app_context["save_delete"] = app_context.child("save_delete").text().as_string();
                    lang_app_context["name"] = app_context.child("name").text().as_string();
                    lang_app_context["trophy_earning"] = app_context.child("trophy_earning").text().as_string();
                    lang_app_context["eligible"] = app_context.child("trophy_earning").attribute("eligible").as_string();
                    lang_app_context["ineligible"] = app_context.child("trophy_earning").attribute("ineligible").as_string();
                    lang_app_context["parental_Controls"] = app_context.child("parental_Controls").text().as_string();
                    lang_app_context["level"] = app_context.child("parental_Controls").attribute("level").as_string();
                    lang_app_context["updated"] = app_context.child("updated").text().as_string();
                    lang_app_context["size"] = app_context.child("size").text().as_string();
                    lang_app_context["version"] = app_context.child("version").text().as_string();
                    lang_app_context["last_time_used"] = app_context.child("last_time_used").text().as_string();
                    lang_app_context["never"] = app_context.child("never").text().as_string();
                    lang_app_context["time_used"] = app_context.child("time_used").text().as_string();
                }

                // Common
                const auto common = lang_child.child("common");
                if (!common.empty()) {
                    auto &common_dialog = host.common_dialog.lang.common;
                    common_dialog["cancel"] = common.child("cancel").text().as_string();
                    common_dialog["delete"] = common.child("delete").text().as_string();
                    common_dialog["file_corrupted"] = common.child("file_corrupted").text().as_string();
                    common_dialog["no"] = common.child("no").text().as_string();
                    common_dialog["select_all"] = common.child("select_all").text().as_string();
                    common_dialog["select"] = common.child("select").text().as_string();
                    common_dialog["please_wait"] = common.child("please_wait").text().as_string();
                    common_dialog["yes"] = common.child("yes").text().as_string();
                    auto &lang_common = gui.lang.common;
                    if (!common.child("wday").empty()) {
                        for (const auto &day : common.child("wday"))
                            lang_common.wday.push_back(day.text().as_string());
                    }
                    if (!common.child("ymonth").empty()) {
                        for (const auto &month : common.child("ymonth"))
                            lang_common.ymonth.push_back(month.text().as_string());
                    }
                    if (!common.child("small_ymonth").empty()) {
                        for (const auto &month : common.child("small_ymonth"))
                            lang_common.small_ymonth.push_back(month.text().as_string());
                    }
                    lang_common.common["hidden_trophy"] = common.child("hidden_trophy").text().as_string();
                    lang_common.common["one_hour_ago"] = common.child("one_hour_ago").text().as_string();
                    lang_common.common["one_minute_ago"] = common.child("one_minute_ago").text().as_string();
                    lang_common.common["bronze"] = common.child("bronze").text().as_string();
                    lang_common.common["gold"] = common.child("gold").text().as_string();
                    lang_common.common["platinum"] = common.child("platinum").text().as_string();
                    lang_common.common["silver"] = common.child("silver").text().as_string();
                    lang_common.common["hours_ago"] = common.child("hours_ago").text().as_string();
                    lang_common.common["minutes_ago"] = common.child("minutes_ago").text().as_string();
                }

                // Dialog
                const auto dialog = lang_child.child("dialog");
                if (!dialog.empty()) {
                    // Trophy
                    if (!dialog.child("trophy").empty())
                        host.common_dialog.lang.trophy["preparing_start_app"] = dialog.child("trophy").child("preparing_start_app").text().as_string();
                    // Save Data
                    if (!dialog.child("save_data").empty()) {
                        auto &lang_save_data = host.common_dialog.lang.save_data;
                        const auto save_data = dialog.child("save_data");
                        if (!save_data.child("delete").empty()) {
                            const auto delete_child = save_data.child("delete");
                            lang_save_data["cancel_deleting"] = delete_child.child("cancel_deleting").text().as_string();
                            lang_save_data["deletion_complete"] = delete_child.child("deletion_complete").text().as_string();
                            lang_save_data["delete_saved_data"] = delete_child.child("delete_saved_data").text().as_string();
                        }
                        if (!save_data.child("info").empty()) {
                            lang_save_data["details"] = save_data.child("info").child("details").text().as_string();
                            lang_save_data["updated"] = save_data.child("info").child("updated").text().as_string();
                        }
                        if (!save_data.child("load").empty()) {
                            const auto load = save_data.child("load");
                            lang_save_data["load"] = load.attribute("name").as_string();
                            lang_save_data["cancel_loading"] = load.child("cancel_loading").text().as_string();
                            lang_save_data["no_saved_data"] = load.child("no_saved_data").text().as_string();
                            lang_save_data["load_complete"] = load.child("load_complete").text().as_string();
                            lang_save_data["loading"] = load.child("loading").text().as_string();
                            lang_save_data["load_saved_data"] = load.child("load_saved_data").text().as_string();
                        }
                        if (!save_data.child("save").empty()) {
                            const auto save = save_data.child("save");
                            lang_save_data["save"] = save.attribute("name").as_string();
                            lang_save_data["cancel_saving"] = save.child("cancel_saving").text().as_string();
                            lang_save_data["could_not_save"] = save.child("could_not_save").text().as_string();
                            lang_save_data["not_free_space"] = save.child("not_free_space").text().as_string();
                            lang_save_data["new_saved_data"] = save.child("new_saved_data").text().as_string();
                            lang_save_data["saving_complete"] = save.child("saving_complete").text().as_string();
                            lang_save_data["save_the_data"] = save.child("save_the_data").text().as_string();
                            lang_save_data["saving"] = save.child("saving").text().as_string();
                            lang_save_data["warning_saving"] = save.child("warning_saving").text().as_string();
                            lang_save_data["overwrite_saved_data"] = save.child("overwrite_saved_data").text().as_string();
                        }
                    }
                }

                // Game Data
                const auto game_data = lang_child.child("game_data");
                if (!game_data.empty()) {
                    auto &lang_game_data = gui.lang.game_data;
                    lang_game_data["app_close"] = game_data.child("app_close").text().as_string();
                }

                // Indicator
                const auto indicator = lang_child.child("indicator");
                if (!indicator.empty()) {
                    auto &lang_indicator = gui.lang.indicator;
                    lang_indicator["app_added_home"] = indicator.child("app_added_home").text().as_string();
                    lang_indicator["delete_all"] = indicator.child("delete_all").text().as_string();
                    lang_indicator["notif_deleted"] = indicator.child("notif_deleted").text().as_string();
                    lang_indicator["install_failed"] = indicator.child("install_failed").text().as_string();
                    lang_indicator["install_complete"] = indicator.child("install_complete").text().as_string();
                    lang_indicator["installing"] = indicator.child("installing").text().as_string();
                    lang_indicator["no_notif"] = indicator.child("no_notif").text().as_string();
                    lang_indicator["trophy_earned"] = indicator.child("trophy_earned").text().as_string();
                }

                // Live Area
                const auto live_area = lang_child.child("live_area");
                if (!live_area.empty()) {
                    start = live_area.child("start").text().as_string();
                    resume = live_area.child("continue").text().as_string();
                }

                // Settings
                const auto settings = lang_child.child("settings");
                if (!settings.empty()) {
                    auto &lang_settings = gui.lang.settings;
                    lang_settings["settings"] = settings.attribute("name").as_string();
                    const auto theme_background = settings.child("theme_background");
                    if (!theme_background.empty()) {
                        lang_settings["theme_background"] = theme_background.attribute("name").as_string();
                        lang_settings["default"] = theme_background.attribute("default").as_string();
                        const auto theme = theme_background.child("theme");
                        // Theme
                        if (!theme.empty()) {
                            lang_settings["theme"] = theme.attribute("name").as_string();
                            // Information
                            const auto information = theme.child("information");
                            if (!information.empty()) {
                                lang_settings["information"] = information.attribute("name").as_string();
                                lang_settings["name"] = information.child("name").text().as_string();
                                lang_settings["provider"] = information.child("provider").text().as_string();
                                lang_settings["updated"] = information.child("updated").text().as_string();
                                lang_settings["size"] = information.child("size").text().as_string();
                                lang_settings["version"] = information.child("version").text().as_string();
                            }
                            lang_settings["delete"] = theme.child("delete").text().as_string();
                        }
                        lang_settings["start_screen"] = theme_background.child("start_screen").attribute("name").as_string();
                        lang_settings["image"] = theme_background.child("start_screen").child("image").text().as_string();
                        lang_settings["home_screen_backgrounds"] = theme_background.child("home_screen_backgrounds").text().as_string();
                    }

                    // Date & Time
                    const auto date_time = settings.child("date_time");
                    if (!date_time.empty()) {
                        lang_settings["date_time"] = date_time.attribute("name").as_string();
                        // Date Format
                        const auto date_format = date_time.child("date_format");
                        if (!date_format.empty()) {
                            lang_settings["date_format"] = date_format.attribute("name").as_string();
                            lang_settings["yyyy_mm_dd"] = date_format.child("yyyy_mm_dd").text().as_string();
                            lang_settings["dd_mm_yyyy"] = date_format.child("dd_mm_yyyy").text().as_string();
                            lang_settings["mm_dd_yyyy"] = date_format.child("mm_dd_yyyy").text().as_string();
                        }
                        // Time Format
                        const auto time_format = date_time.child("time_format");
                        if (!time_format.empty()) {
                            lang_settings["time_format"] = time_format.attribute("name").as_string();
                            lang_settings["12_hour_clock"] = time_format.child("clock_12_hour").text().as_string();
                            lang_settings["24_hour_clock"] = time_format.child("clock_24_hour").text().as_string();
                        }
                    }

                    // Languague
                    const auto language = settings.child("language");
                    if (!language.empty()) {
                        lang_settings["language"] = language.attribute("name").as_string();
                        lang_settings["system_language"] = language.child("system_language").text().as_string();
                        // Input Languague
                        const auto input_language = language.child("input_language");
                        if (!input_language.empty()) {
                            lang_settings["input_language"] = input_language.attribute("name").as_string();
                            // Keyboards
                            const auto keyboards = input_language.child("keyboards");
                            if (!keyboards.empty()) {
                                lang_settings["keyboards"] = keyboards.attribute("name").as_string();
                                auto &lang_ime = host.ime.languages;
                                for (const auto &lang : keyboards.child("ime_langagues"))
                                    lang_ime.push_back({ SceImeLanguage(lang.attribute("id").as_ullong()), lang.text().as_string() });
                            }
                        }
                    }
                }

                // Trophy Collection
                const auto trophy_collection = lang_child.child("trophy_collection");
                if (!trophy_collection.empty()) {
                    auto &lang_trophy_collection = gui.lang.trophy_collection;
                    lang_trophy_collection["details"] = trophy_collection.child("details").text().as_string();
                    lang_trophy_collection["earned"] = trophy_collection.child("earned").text().as_string();
                    lang_trophy_collection["name"] = trophy_collection.child("name").text().as_string();
                    lang_trophy_collection["no_trophies"] = trophy_collection.child("no_trophies").text().as_string();
                    lang_trophy_collection["not_earned"] = trophy_collection.child("not_earned").text().as_string();
                    lang_trophy_collection["original"] = trophy_collection.child("original").text().as_string();
                    lang_trophy_collection["sort"] = trophy_collection.child("sort").text().as_string();
                    lang_trophy_collection["trophies"] = trophy_collection.child("trophies").text().as_string();
                    lang_trophy_collection["grade"] = trophy_collection.child("grade").text().as_string();
                    lang_trophy_collection["progress"] = trophy_collection.child("progress").text().as_string();
                    lang_trophy_collection["updated"] = trophy_collection.child("updated").text().as_string();
                }

                // User Management
                const auto user_management = lang_child.child("user_management");
                if (!user_management.empty()) {
                    auto &lang_user_management = gui.lang.user_management;
                    lang_user_management["create_user"] = user_management.child("create_user").text().as_string();
                    lang_user_management["edit_user"] = user_management.child("edit_user").text().as_string();
                    lang_user_management["delete_user"] = user_management.child("delete_user").text().as_string();
                    lang_user_management["change_avatar"] = user_management.child("change_avatar").text().as_string();
                    lang_user_management["name"] = user_management.child("name").text().as_string();
                    lang_user_management["user"] = user_management.child("user").text().as_string();
                    lang_user_management["confirm"] = user_management.child("confirm").text().as_string();
                }
            }
        } else
            LOG_ERROR("Error open lang file xml: {}", lang_xml_path);
    }

    if (start.empty()) {
        start = "Start";
        resume = "Continue";
    }
}

bool get_sys_apps_state(GuiState &gui) {
    return !gui.live_area.content_manager && !gui.live_area.settings && !gui.live_area.trophy_collection && !gui.live_area.manual && !gui.live_area.user_management;
}

#ifdef _WIN32
static const char OS_PREFIX[] = "start ";
#elif __APPLE__
static const char OS_PREFIX[] = "open ";
#else
static const char OS_PREFIX[] = "xdg-open ";
#endif

struct FRAME {
    std::string id;
    std::string multi;
    SceUInt64 autoflip;
};

struct STR {
    std::string color;
    float size;
    std::string text;
};

static std::map<std::string, std::vector<FRAME>> frames;
static std::map<std::string, std::map<std::string, std::vector<STR>>> str;
static std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, std::pair<SceInt32, std::string>>>>> liveitem;
static std::map<std::string, std::map<std::string, std::map<std::string, std::map<std::string, ImVec2>>>> items;
static std::map<std::string, std::map<std::string, std::map<std::string, ImVec2>>> items_pos;
static std::map<std::string, std::map<std::string, std::string>> target;
static std::map<std::string, std::map<std::string, uint64_t>> current_item, last_time;
static std::map<std::string, std::string> type;
static std::map<std::string, int32_t> sku_flag;

void init_live_area(GuiState &gui, HostState &host) {
    // Init type
    if (items_pos.empty()) {
        // Content manager
        items_pos["content_manager"]["gate"]["pos"] = ImVec2(620.f, 364.f);
        // A1
        items_pos["a1"]["gate"]["pos"] = ImVec2(620.f, 364.f);
        items_pos["a1"]["frame1"]["pos"] = ImVec2(900.f, 415.f);
        items_pos["a1"]["frame1"]["size"] = ImVec2(260.f, 260.f);
        items_pos["a1"]["frame2"]["pos"] = ImVec2(320.f, 415.f);
        items_pos["a1"]["frame2"]["size"] = ImVec2(260.f, 260.f);
        items_pos["a1"]["frame3"]["pos"] = ImVec2(900.f, 155.f);
        items_pos["a1"]["frame3"]["size"] = ImVec2(840.f, 150.f);
        // A2
        items_pos["a2"]["gate"]["pos"] = ImVec2(620.f, 390.f);
        items_pos["a2"]["frame1"]["pos"] = ImVec2(900.f, 405.f);
        items_pos["a2"]["frame1"]["size"] = ImVec2(260.f, 400.f);
        items_pos["a2"]["frame2"]["pos"] = ImVec2(320.f, 405.f);
        items_pos["a2"]["frame2"]["size"] = ImVec2(260.f, 400.f);
        items_pos["a2"]["frame3"]["pos"] = ImVec2(640.f, 205.f);
        items_pos["a2"]["frame3"]["size"] = ImVec2(320.f, 200.f);
        // A3
        items_pos["a3"]["gate"]["pos"] = ImVec2(620.f, 394.f);
        items_pos["a3"]["frame1"]["pos"] = ImVec2(900.f, 415.f);
        items_pos["a3"]["frame1"]["size"] = ImVec2(260.f, 200.f);
        items_pos["a3"]["frame2"]["pos"] = ImVec2(320.f, 415.f);
        items_pos["a3"]["frame2"]["size"] = ImVec2(260.f, 200.f);
        items_pos["a3"]["frame3"]["pos"] = ImVec2(900.f, 215.f);
        items_pos["a3"]["frame3"]["size"] = ImVec2(260.f, 210.f);
        items_pos["a3"]["frame4"]["pos"] = ImVec2(640.f, 215.f);
        items_pos["a3"]["frame4"]["size"] = ImVec2(320.f, 210.f);
        items_pos["a3"]["frame5"]["pos"] = ImVec2(320.f, 215.f);
        items_pos["a3"]["frame5"]["size"] = ImVec2(260.f, 210.f);
        // A4
        items_pos["a4"]["gate"]["pos"] = ImVec2(620.f, 394.f);
        items_pos["a4"]["frame1"]["pos"] = ImVec2(900.f, 415.f);
        items_pos["a4"]["frame1"]["size"] = ImVec2(260.f, 200.f);
        items_pos["a4"]["frame2"]["pos"] = ImVec2(320.f, 415.f);
        items_pos["a4"]["frame2"]["size"] = ImVec2(260.f, 200.f);
        items_pos["a4"]["frame3"]["pos"] = ImVec2(900.f, 215.f);
        items_pos["a4"]["frame3"]["size"] = ImVec2(840.f, 70.f);
        items_pos["a4"]["frame4"]["pos"] = ImVec2(900.f, 145.f);
        items_pos["a4"]["frame4"]["size"] = ImVec2(840.f, 70.f);
        items_pos["a4"]["frame5"]["pos"] = ImVec2(900.f, 75.f);
        items_pos["a4"]["frame5"]["size"] = ImVec2(840.f, 70.f);
        // A5
        items_pos["a5"]["gate"]["pos"] = ImVec2(380.f, 388.f);
        items_pos["a5"]["frame1"]["pos"] = ImVec2(900.f, 414.f);
        items_pos["a5"]["frame1"]["size"] = ImVec2(480.f, 68.f);
        items_pos["a5"]["frame2"]["pos"] = ImVec2(900.f, 345.f);
        items_pos["a5"]["frame2"]["size"] = ImVec2(480.f, 68.f);
        items_pos["a5"]["frame3"]["pos"] = ImVec2(900.f, 278.f);
        items_pos["a5"]["frame3"]["size"] = ImVec2(480.f, 68.f);
        items_pos["a5"]["frame4"]["pos"] = ImVec2(900.f, 210.f);
        items_pos["a5"]["frame4"]["size"] = ImVec2(480.f, 68.f);
        items_pos["a5"]["frame5"]["pos"] = ImVec2(900.f, 142.f);
        items_pos["a5"]["frame5"]["size"] = ImVec2(480.f, 68.f);
        items_pos["a5"]["frame6"]["pos"] = ImVec2(900.f, 74.f);
        items_pos["a5"]["frame6"]["size"] = ImVec2(480.f, 68.f);
        items_pos["a5"]["frame7"]["pos"] = ImVec2(420.f, 220.f);
        items_pos["a5"]["frame7"]["size"] = ImVec2(360.f, 210.f);
        // psmobile
        items_pos["psmobile"]["gate"]["pos"] = ImVec2(380.f, 339.f);
        //items_pos["psmobile"]["frame1"]["pos"] = ImVec2(900.f, 414.f);
        //items_pos["psmobile"]["frame1"]["size"] = ImVec2(480.f, 68.f);
        items_pos["psmobile"]["frame2"]["pos"] = ImVec2(865.f, 215.f);
        items_pos["psmobile"]["frame2"]["size"] = ImVec2(440.f, 68.f);
        items_pos["psmobile"]["frame3"]["pos"] = ImVec2(865.f, 113.f);
        items_pos["psmobile"]["frame3"]["size"] = ImVec2(440.f, 34.f);
        items_pos["psmobile"]["frame4"]["pos"] = ImVec2(865.f, 39.f);
        items_pos["psmobile"]["frame4"]["size"] = ImVec2(440.f, 34.f);
    }

    const auto user_lang = gui.lang.user_lang;
    const auto app_path = gui.apps_list_opened[gui.current_app_selected];
    const auto is_sys_app = app_path.find("NPXS") != std::string::npos;
    const auto is_ps_app = app_path.find("PCS") != std::string::npos;
    const VitaIoDevice app_device = is_sys_app ? VitaIoDevice::vs0 : VitaIoDevice::ux0;
    const auto APP_INDEX = get_app_index(gui, app_path);

    if (is_ps_app && (sku_flag.find(app_path) == sku_flag.end()))
        sku_flag[app_path] = get_license_sku_flag(host, APP_INDEX->content_id);

    if (gui.live_area_contents.find(app_path) == gui.live_area_contents.end()) {
        auto default_contents = false;
        const auto fw_path{ fs::path(host.pref_path) / "vs0" };
        const auto default_fw_contents{ fw_path / "data/internal/livearea/default/sce_sys/livearea/contents/template.xml" };
        const auto APP_PATH{ fs::path(host.pref_path) / app_device._to_string() / "app" / app_path };
        const auto live_area_path{ fs::path("sce_sys") / ((sku_flag[app_path] == 3) && fs::exists(APP_PATH / "sce_sys/retail/livearea") ? "retail/livearea" : "livearea") };
        auto template_xml{ APP_PATH / live_area_path / "contents/template.xml" };

        pugi::xml_document doc;

        if (!doc.load_file(template_xml.c_str())) {
            if (is_ps_app || is_sys_app)
                LOG_WARN("Live Area Contents is corrupted or missing for title: {} '{}' in path: {}.", APP_INDEX->title_id, APP_INDEX->title, template_xml.string());
            if (doc.load_file(default_fw_contents.c_str())) {
                template_xml = default_fw_contents;
                default_contents = true;
                LOG_INFO("Using default firmware contents.");
            } else {
                type[app_path] = "a1";
                LOG_WARN("Default firmware contents is corrupted or missing, install firmware for fix it.");
                return;
            }
        }

        if (doc.load_file(template_xml.c_str())) {
            std::map<std::string, std::string> name;

            type[app_path].clear();
            type[app_path] = doc.child("livearea").attribute("style").as_string();

            if (!doc.child("livearea").child("livearea-background").child("image").child("lang").text().empty()) {
                for (const auto &livearea_background : doc.child("livearea").child("livearea-background")) {
                    if (livearea_background.child("lang").text().as_string() == user_lang) {
                        name["livearea-background"] = livearea_background.text().as_string();
                        break;
                    } else if (!livearea_background.child("exclude-lang").empty() && livearea_background.child("exclude-lang").text().as_string() != user_lang) {
                        name["livearea-background"] = livearea_background.text().as_string();
                        break;
                    } else if (livearea_background.attribute("default").as_string() == std::string("on")) {
                        name["livearea-background"] = livearea_background.text().as_string();
                        break;
                    }
                }
            } else
                name["livearea-background"] = doc.child("livearea").child("livearea-background").child("image").text().as_string();

            if (name["livearea-background"].empty())
                name["livearea-background"] = doc.child("livearea").child("livearea-background").child("image").text().as_string();

            if (!default_contents) {
                for (const auto &gate : doc.child("livearea").child("gate")) {
                    if (gate.child("lang")) {
                        if (gate.child("lang").text().as_string() == user_lang) {
                            name["gate"] = gate.text().as_string();
                            break;
                        } else if (gate.attribute("default").as_string() == std::string("on"))
                            name["gate"] = gate.text().as_string();
                        else
                            for (const auto &lang : gate)
                                if (lang.text().as_string() == user_lang) {
                                    name["gate"] = gate.text().as_string();
                                    break;
                                }
                    } else if (gate.child("cntry")) {
                        if (gate.child("cntry").attribute("lang").as_string() == user_lang) {
                            name["gate"] = gate.text().as_string();
                            break;
                        } else
                            name["gate"] = gate.text().as_string();
                    } else
                        name["gate"] = gate.text().as_string();
                }
                if (name["gate"].find('\n') != std::string::npos)
                    name["gate"].erase(remove(name["gate"].begin(), name["gate"].end(), '\n'), name["gate"].end());
                name["gate"].erase(remove_if(name["gate"].begin(), name["gate"].end(), isspace), name["gate"].end());
            }

            if (name["livearea-background"].find('\n') != std::string::npos)
                name["livearea-background"].erase(remove(name["livearea-background"].begin(), name["livearea-background"].end(), '\n'), name["livearea-background"].end());
            name["livearea-background"].erase(remove_if(name["livearea-background"].begin(), name["livearea-background"].end(), isspace), name["livearea-background"].end());

            for (const auto &contents : name) {
                int32_t width = 0;
                int32_t height = 0;
                vfs::FileBuffer buffer;

                if (default_contents)
                    vfs::read_file(VitaIoDevice::vs0, buffer, host.pref_path, "data/internal/livearea/default/sce_sys/livearea/contents/" + contents.second);
                else if (app_device == VitaIoDevice::vs0)
                    vfs::read_file(VitaIoDevice::vs0, buffer, host.pref_path, "app/" + app_path + "/sce_sys/livearea/contents/" + contents.second);
                else
                    vfs::read_app_file(buffer, host.pref_path, app_path, live_area_path.string() + "/contents/" + contents.second);

                if (buffer.empty()) {
                    if (is_ps_app || is_sys_app)
                        LOG_WARN("Contents {} '{}' Not found for title {} [{}].", contents.first, contents.second, app_path, APP_INDEX->title);
                    continue;
                }
                stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                if (!data) {
                    LOG_ERROR("Invalid Live Area Contents for title {} [{}].", app_path, APP_INDEX->title);
                    continue;
                }

                gui.live_area_contents[app_path][contents.first].init(gui.imgui_state.get(), data, width, height);
                stbi_image_free(data);
            }

            std::map<std::string, std::map<std::string, std::vector<std::string>>> items_name;

            frames[app_path].clear();
            liveitem[app_path].clear();
            str[app_path].clear();
            target[app_path].clear();

            for (const auto &livearea : doc.child("livearea")) {
                if (!livearea.attribute("id").empty()) {
                    frames[app_path].push_back({ livearea.attribute("id").as_string(), livearea.attribute("multi").as_string(), livearea.attribute("autoflip").as_uint() });

                    std::string frame = frames[app_path].back().id;

                    // Position background
                    if (!livearea.child("liveitem").child("background").empty()) {
                        if (!livearea.child("liveitem").child("background").attribute("x").empty())
                            liveitem[app_path][frame]["background"]["x"].first = livearea.child("liveitem").child("background").attribute("x").as_int();
                        if (!livearea.child("liveitem").child("background").attribute("y").empty())
                            liveitem[app_path][frame]["background"]["y"].first = livearea.child("liveitem").child("background").attribute("y").as_int();
                        if (!livearea.child("liveitem").child("background").attribute("align").empty())
                            liveitem[app_path][frame]["background"]["align"].second = livearea.child("liveitem").child("background").attribute("align").as_string();
                        if (!livearea.child("liveitem").child("background").attribute("valign").empty())
                            liveitem[app_path][frame]["background"]["valign"].second = livearea.child("liveitem").child("background").attribute("valign").as_string();
                    }

                    // Position image
                    if (!livearea.child("liveitem").child("image").empty()) {
                        if (!livearea.child("liveitem").child("image").attribute("x").empty())
                            liveitem[app_path][frame]["image"]["x"].first = livearea.child("liveitem").child("image").attribute("x").as_int();
                        if (!livearea.child("liveitem").child("image").attribute("y").empty())
                            liveitem[app_path][frame]["image"]["y"].first = livearea.child("liveitem").child("image").attribute("y").as_int();
                        if (!livearea.child("liveitem").child("image").attribute("align").empty())
                            liveitem[app_path][frame]["image"]["align"].second = livearea.child("liveitem").child("image").attribute("align").as_string();
                        if (!livearea.child("liveitem").child("image").attribute("valign").empty())
                            liveitem[app_path][frame]["image"]["valign"].second = livearea.child("liveitem").child("image").attribute("valign").as_string();
                        if (!livearea.child("liveitem").child("image").attribute("origin").empty())
                            liveitem[app_path][frame]["image"]["origin"].second = livearea.child("liveitem").child("image").attribute("origin").as_string();
                    }

                    if (!livearea.child("liveitem").child("text").empty()) {
                        // SceInt32
                        if (!livearea.child("liveitem").child("text").attribute("width").empty())
                            liveitem[app_path][frame]["text"]["width"].first = livearea.child("liveitem").child("text").attribute("width").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("height").empty())
                            liveitem[app_path][frame]["text"]["height"].first = livearea.child("liveitem").child("text").attribute("height").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("x").empty())
                            liveitem[app_path][frame]["text"]["x"].first = livearea.child("liveitem").child("text").attribute("x").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("y").empty())
                            liveitem[app_path][frame]["text"]["y"].first = livearea.child("liveitem").child("text").attribute("y").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("margin-top").empty())
                            liveitem[app_path][frame]["text"]["margin-top"].first = livearea.child("liveitem").child("text").attribute("margin-top").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("margin-buttom").empty())
                            liveitem[app_path][frame]["text"]["margin-buttom"].first = livearea.child("liveitem").child("text").attribute("margin-buttom").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("margin-left").empty())
                            liveitem[app_path][frame]["text"]["margin-left"].first = livearea.child("liveitem").child("text").attribute("margin-left").as_int();
                        if (!livearea.child("liveitem").child("text").attribute("margin-right").empty())
                            liveitem[app_path][frame]["text"]["margin-right"].first = livearea.child("liveitem").child("text").attribute("margin-right").as_int();

                        // String
                        if (!livearea.child("liveitem").child("text").attribute("align").empty())
                            liveitem[app_path][frame]["text"]["align"].second = livearea.child("liveitem").child("text").attribute("align").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("valign").empty())
                            liveitem[app_path][frame]["text"]["valign"].second = livearea.child("liveitem").child("text").attribute("valign").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("origin").empty())
                            liveitem[app_path][frame]["text"]["origin"].second = livearea.child("liveitem").child("text").attribute("origin").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("line-align").empty())
                            liveitem[app_path][frame]["text"]["line-align"].second = livearea.child("liveitem").child("text").attribute("line-align").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("text-align").empty())
                            liveitem[app_path][frame]["text"]["text-align"].second = livearea.child("liveitem").child("text").attribute("text-align").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("text-valign").empty())
                            liveitem[app_path][frame]["text"]["text-valign"].second = livearea.child("liveitem").child("text").attribute("text-valign").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("word-wrap").empty())
                            liveitem[app_path][frame]["text"]["word-wrap"].second = livearea.child("liveitem").child("text").attribute("word-wrap").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("word-scroll").empty())
                            liveitem[app_path][frame]["text"]["word-scroll"].second = livearea.child("liveitem").child("text").attribute("word-scroll").as_string();
                        if (!livearea.child("liveitem").child("text").attribute("ellipsis").empty())
                            liveitem[app_path][frame]["text"]["ellipsis"].second = livearea.child("liveitem").child("text").attribute("ellipsis").as_string();
                    }

                    for (const auto &frame_id : livearea) {
                        if (frame_id.name() == std::string("liveitem")) {
                            if (!frame_id.child("cntry").empty()) {
                                for (const auto &cntry : frame_id) {
                                    if ((cntry.name() == std::string("cntry")) && (cntry.attribute("lang").as_string() == user_lang)) {
                                        if (!frame_id.child("background").text().empty())
                                            items_name[frame]["background"].push_back({ frame_id.child("background").text().as_string() });
                                        if (!frame_id.child("image").text().empty())
                                            items_name[frame]["image"].push_back({ frame_id.child("image").text().as_string() });
                                        if (!frame_id.child("target").text().empty())
                                            target[app_path][frame] = frame_id.child("target").text().as_string();
                                        if (!frame_id.child("text").child("str").text().empty()) {
                                            for (const auto &str_child : frame_id.child("text"))
                                                str[app_path][frame].push_back({ str_child.attribute("color").as_string(), str_child.attribute("size").as_float(), str_child.text().as_string() });
                                        }
                                        break;
                                    }
                                }
                            } else if (!frame_id.child("exclude-lang").empty()) {
                                bool exclude_lang = false;
                                for (const auto &liveitem : frame_id)
                                    if ((liveitem.name() == std::string("exclude-lang")) && (liveitem.text().as_string() == user_lang)) {
                                        exclude_lang = true;
                                        break;
                                    }
                                if (!exclude_lang) {
                                    if (!frame_id.child("background").text().empty())
                                        items_name[frame]["background"].push_back({ frame_id.child("background").text().as_string() });
                                    if (!frame_id.child("image").text().empty())
                                        items_name[frame]["image"].push_back({ frame_id.child("image").text().as_string() });
                                    if (!frame_id.child("target").text().empty())
                                        target[app_path][frame] = frame_id.child("target").text().as_string();
                                    if (!frame_id.child("text").child("str").text().empty()) {
                                        for (const auto &str_child : frame_id.child("text"))
                                            str[app_path][frame].push_back({ str_child.attribute("color").as_string(), str_child.attribute("size").as_float(), str_child.text().as_string() });
                                    }
                                    break;
                                }
                            } else if (frame_id.child("lang").text().as_string() == user_lang) {
                                if (!frame_id.child("background").text().empty())
                                    items_name[frame]["background"].push_back({ frame_id.child("background").text().as_string() });
                                if (!frame_id.child("image").text().empty())
                                    items_name[frame]["image"].push_back({ frame_id.child("image").text().as_string() });
                                if (!frame_id.child("target").text().empty())
                                    target[app_path][frame] = frame_id.child("target").text().as_string();
                                if (!frame_id.child("text").child("str").text().empty()) {
                                    for (const auto &str_child : frame_id.child("text"))
                                        str[app_path][frame].push_back({ str_child.attribute("color").as_string(), str_child.attribute("size").as_float(), str_child.text().as_string() });
                                }
                                break;
                            }
                        }
                    }

                    // if empty
                    const auto liveitem = livearea.child("liveitem");
                    if (items_name[frame]["background"].empty() && !liveitem.child("background").text().empty())
                        items_name[frame]["background"].push_back({ liveitem.child("background").text().as_string() });
                    if (items_name[frame]["image"].empty() && !liveitem.child("image").text().empty())
                        items_name[frame]["image"].push_back({ liveitem.child("image").text().as_string() });
                    if (target[app_path][frame].empty() && !liveitem.child("target").text().empty())
                        target[app_path][frame] = liveitem.child("target").text().as_string();
                    if (str[app_path][frame].empty() && !liveitem.child("text").child("str").text().empty()) {
                        for (const auto &str_child : liveitem.child("text"))
                            str[app_path][frame].push_back({ str_child.attribute("color").as_string(), str_child.attribute("size").as_float(), str_child.text().as_string() });
                    }

                    // Remove space or return line if exist on target
                    if (!target[app_path][frame].empty()) {
                        if (target[app_path][frame].find('\n') != std::string::npos)
                            target[app_path][frame].erase(remove(target[app_path][frame].begin(), target[app_path][frame].end(), '\n'), target[app_path][frame].end());
                        target[app_path][frame].erase(remove_if(target[app_path][frame].begin(), target[app_path][frame].end(), isspace), target[app_path][frame].end());
                    }
                }
            }

            for (auto &item : items_name) {
                current_item[app_path][item.first] = 0;
                if (!item.second["background"].empty()) {
                    for (auto &bg_name : item.second["background"])
                        gui.live_items[app_path][item.first]["background"].push_back({});
                }
                if (!item.second["image"].empty()) {
                    for (auto &img_name : item.second["image"])
                        gui.live_items[app_path][item.first]["image"].push_back({});
                }
            }

            auto pos = 0;
            std::map<std::string, std::string> current_frame;
            for (auto &item : items_name) {
                if (!item.second.empty()) {
                    for (auto &bg_name : item.second["background"]) {
                        if (!bg_name.empty()) {
                            if (current_frame["background"] != item.first) {
                                current_frame["background"] = item.first;
                                pos = 0;
                            } else
                                ++pos;

                            if (bg_name.find('\n') != std::string::npos)
                                bg_name.erase(remove(bg_name.begin(), bg_name.end(), '\n'), bg_name.end());
                            bg_name.erase(remove_if(bg_name.begin(), bg_name.end(), isspace), bg_name.end());

                            int32_t width = 0;
                            int32_t height = 0;
                            vfs::FileBuffer buffer;

                            if (app_device == VitaIoDevice::vs0)
                                vfs::read_file(VitaIoDevice::vs0, buffer, host.pref_path, "app/" + app_path + "/sce_sys/livearea/contents/" + bg_name);
                            else
                                vfs::read_app_file(buffer, host.pref_path, app_path, live_area_path.string() + "/contents/" + bg_name);

                            if (buffer.empty()) {
                                if (is_ps_app || is_sys_app)
                                    LOG_WARN("background, Id: {}, Name: '{}', Not found for title: {} [{}].", item.first, bg_name, app_path, APP_INDEX->title);
                                continue;
                            }
                            stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                            if (!data) {
                                LOG_ERROR("Frame: {}, Invalid Live Area Contents for title: {} [{}].", item.first, app_path, APP_INDEX->title);
                                continue;
                            }

                            items[app_path][item.first]["background"]["size"] = ImVec2(float(width), float(height));
                            gui.live_items[app_path][item.first]["background"][pos].init(gui.imgui_state.get(), data, width, height);
                            stbi_image_free(data);
                        }
                    }

                    for (auto &img_name : item.second["image"]) {
                        if (!img_name.empty()) {
                            if (current_frame["image"] != item.first) {
                                current_frame["image"] = item.first;
                                pos = 0;
                            } else
                                ++pos;

                            if (img_name.find('\n') != std::string::npos)
                                img_name.erase(remove(img_name.begin(), img_name.end(), '\n'), img_name.end());
                            img_name.erase(remove_if(img_name.begin(), img_name.end(), isspace), img_name.end());

                            int32_t width = 0;
                            int32_t height = 0;
                            vfs::FileBuffer buffer;

                            if (app_device == VitaIoDevice::vs0)
                                vfs::read_file(VitaIoDevice::vs0, buffer, host.pref_path, "app/" + app_path + "/sce_sys/livearea/contents/" + img_name);
                            else
                                vfs::read_app_file(buffer, host.pref_path, app_path, live_area_path.string() + "/contents/" + img_name);

                            if (buffer.empty()) {
                                if (is_ps_app || is_sys_app)
                                    LOG_WARN("Image, Id: {} Name: '{}', Not found for title {} [{}].", item.first, img_name, app_path, APP_INDEX->title);
                                continue;
                            }
                            stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                            if (!data) {
                                LOG_ERROR("Frame: {}, Invalid Live Area Contents for title: {} [{}].", item.first, app_path, APP_INDEX->title);
                                continue;
                            }

                            items[app_path][item.first]["image"]["size"] = ImVec2(float(width), float(height));
                            gui.live_items[app_path][item.first]["image"][pos].init(gui.imgui_state.get(), data, width, height);
                            stbi_image_free(data);
                        }
                    }
                }
            }
        }
    }
    if (type[app_path].empty())
        type[app_path] = "a1";
}

inline uint64_t current_time() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

static const ImU32 ARROW_COLOR = 4294967295; // White

void draw_live_area_screen(GuiState &gui, HostState &host) {
    const ImVec2 display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);
    const auto INFORMATION_BAR_HEIGHT = 32.f * SCALE.y;

    const auto app_path = gui.apps_list_opened[gui.current_app_selected];
    const VitaIoDevice app_device = app_path.find("NPXS") != std::string::npos ? VitaIoDevice::vs0 : VitaIoDevice::ux0;
    const auto is_background = (gui.users[host.io.user_id].use_theme_bg && !gui.theme_backgrounds.empty()) || !gui.user_backgrounds.empty();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);

    ImGui::SetNextWindowBgAlpha(is_background ? 0.5f : 0.f);
    ImGui::Begin("##live_area", &gui.live_area.live_area_screen, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

    if (is_background)
        ImGui::GetBackgroundDrawList()->AddImage((gui.users[host.io.user_id].use_theme_bg && !gui.theme_backgrounds.empty()) ? gui.theme_backgrounds[gui.current_theme_bg] : gui.user_backgrounds[gui.users[host.io.user_id].backgrounds[gui.current_user_bg]],
            ImVec2(0.f, 32.f), display_size);
    else
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, INFORMATION_BAR_HEIGHT), display_size, IM_COL32(11.f, 90.f, 252.f, 180.f), 0.f, ImDrawCornerFlags_All);

    const auto background_pos = ImVec2(900.0f * SCALE.x, 500.0f * SCALE.y);
    const auto pos_bg = ImVec2(display_size.x - background_pos.x, display_size.y - background_pos.y);
    const auto background_size = ImVec2(840.0f * SCALE.x, 500.0f * SCALE.y);

    if (gui.live_area_contents[app_path].find("livearea-background") != gui.live_area_contents[app_path].end())
        ImGui::GetWindowDrawList()->AddImage(gui.live_area_contents[app_path]["livearea-background"], pos_bg, ImVec2(pos_bg.x + background_size.x, pos_bg.y + background_size.y));
    else
        ImGui::GetWindowDrawList()->AddRectFilled(pos_bg, ImVec2(pos_bg.x + background_size.x, pos_bg.y + background_size.y), IM_COL32(148.f, 164.f, 173.f, 255.f), 0.f, ImDrawCornerFlags_All);

    for (const auto &frame : frames[app_path]) {
        if (frame.autoflip != 0) {
            if (last_time[app_path][frame.id] == 0)
                last_time[app_path][frame.id] = current_time();

            while (last_time[app_path][frame.id] + frame.autoflip < current_time()) {
                last_time[app_path][frame.id] += frame.autoflip;

                if (gui.live_items[app_path][frame.id].find("background") != gui.live_items[app_path][frame.id].end()) {
                    if (current_item[app_path][frame.id] != int(gui.live_items[app_path][frame.id]["background"].size()) - 1)
                        ++current_item[app_path][frame.id];
                    else
                        current_item[app_path][frame.id] = 0;
                } else if (gui.live_items[app_path][frame.id].find("image") != gui.live_items[app_path][frame.id].end()) {
                    if (current_item[app_path][frame.id] != int(gui.live_items[app_path][frame.id]["image"].size()) - 1)
                        ++current_item[app_path][frame.id];
                    else
                        current_item[app_path][frame.id] = 0;
                }
            }
        }

        if (type[app_path] == "psmobile") {
            LOG_WARN_IF(items_pos[type[app_path]][frame.id].empty(), "Info not found for {}, with {}, on title: {}",
                type[app_path], frame.id, app_path);
        }

        const auto FRAME_SIZE = items_pos[type[app_path]][frame.id]["size"];

        auto FRAME_POS = ImVec2(items_pos[type[app_path]][frame.id]["pos"].x * SCALE.x,
            items_pos[type[app_path]][frame.id]["pos"].y * SCALE.y);

        auto bg_size = items[app_path][frame.id]["background"]["size"];

        // Resize items
        const auto bg_resize = ImVec2(bg_size.x / FRAME_SIZE.x, bg_size.y / FRAME_SIZE.y);
        if (bg_size.x > FRAME_SIZE.x)
            bg_size.x /= bg_resize.x;
        if (bg_size.y > FRAME_SIZE.y)
            bg_size.y /= bg_resize.y;

        auto img_size = items[app_path][frame.id]["image"]["size"];
        const auto img_resize = ImVec2(img_size.x / FRAME_SIZE.x, img_size.y / FRAME_SIZE.y);
        if (img_size.x > FRAME_SIZE.x)
            img_size.x /= img_resize.x;
        if (img_size.y > FRAME_SIZE.y)
            img_size.y /= img_resize.y;

        // Items pos init (Center)
        auto bg_pos_init = ImVec2((FRAME_SIZE.x - bg_size.x) / 2.f, (FRAME_SIZE.y - bg_size.y) / 2.f);
        auto img_pos_init = ImVec2((FRAME_SIZE.x - img_size.x) / 2.f, (FRAME_SIZE.y - img_size.y) / 2.f);

        // Allign items
        if ((liveitem[app_path][frame.id]["background"]["align"].second == "left") && (liveitem[app_path][frame.id]["background"]["x"].first >= 0))
            bg_pos_init.x = 0.0f;
        else if ((liveitem[app_path][frame.id]["background"]["align"].second == "right") && (liveitem[app_path][frame.id]["background"]["x"].first <= 0))
            bg_pos_init.x = FRAME_SIZE.x - bg_size.x;
        else
            bg_pos_init.x += liveitem[app_path][frame.id]["background"]["x"].first;

        if ((liveitem[app_path][frame.id]["image"]["align"].second == "left") && (liveitem[app_path][frame.id]["image"]["x"].first >= 0))
            img_pos_init.x = 0.0f;
        else if ((liveitem[app_path][frame.id]["image"]["align"].second == "right") && (liveitem[app_path][frame.id]["image"]["x"].first <= 0))
            img_pos_init.x = FRAME_SIZE.x - img_size.x;
        else
            img_pos_init.x += liveitem[app_path][frame.id]["image"]["x"].first;

        // Valign items
        if ((liveitem[app_path][frame.id]["background"]["valign"].second == "top") && (liveitem[app_path][frame.id]["background"]["y"].first <= 0))
            bg_pos_init.y = 0.0f;
        else if ((liveitem[app_path][frame.id]["background"]["valign"].second == "bottom") && (liveitem[app_path][frame.id]["background"]["y"].first >= 0))
            bg_pos_init.y = FRAME_SIZE.y - bg_size.y;
        else
            bg_pos_init.y -= liveitem[app_path][frame.id]["background"]["y"].first;

        if ((liveitem[app_path][frame.id]["image"]["valign"].second == "top") && (liveitem[app_path][frame.id]["image"]["y"].first <= 0))
            img_pos_init.y = 0.0f;
        else if ((liveitem[app_path][frame.id]["image"]["valign"].second == "bottom") && (liveitem[app_path][frame.id]["image"]["y"].first >= 0))
            img_pos_init.y = FRAME_SIZE.y - img_size.y;
        else
            img_pos_init.y -= liveitem[app_path][frame.id]["image"]["y"].first;

        // Set items pos
        auto bg_pos = ImVec2((display_size.x - FRAME_POS.x) + (bg_pos_init.x * SCALE.x), (display_size.y - FRAME_POS.y) + (bg_pos_init.y * SCALE.y));

        auto img_pos = ImVec2((display_size.x - FRAME_POS.x) + (img_pos_init.x * SCALE.x), (display_size.y - FRAME_POS.y) + (img_pos_init.y * SCALE.y));

        if (bg_size.x == FRAME_SIZE.x)
            bg_pos.x = display_size.x - FRAME_POS.x;
        if (bg_size.y == FRAME_SIZE.y)
            bg_pos.y = display_size.y - FRAME_POS.y;

        if (img_size.x == FRAME_SIZE.x)
            img_pos.x = display_size.x - FRAME_POS.x;
        if (img_size.y == FRAME_SIZE.y)
            img_pos.y = display_size.y - FRAME_POS.y;

        // Scal size items
        const auto bg_scal_size = ImVec2(bg_size.x * SCALE.x, bg_size.y * SCALE.y);
        const auto img_scal_size = ImVec2(img_size.x * SCALE.x, img_size.y * SCALE.y);

        const auto pos_frame = ImVec2(display_size.x - FRAME_POS.x, display_size.y - FRAME_POS.y);

        // Scal size frame
        const auto scal_size_frame = ImVec2(FRAME_SIZE.x * SCALE.x, FRAME_SIZE.y * SCALE.y);

        // Reset position if get outside frame
        if ((bg_pos.x + bg_scal_size.x) > (pos_frame.x + scal_size_frame.x))
            bg_pos.x += (pos_frame.x + scal_size_frame.x) - (bg_pos.x + bg_scal_size.x);
        if (bg_pos.x < pos_frame.x)
            bg_pos.x += pos_frame.x - bg_pos.x;

        if ((bg_pos.y + bg_scal_size.y) > (pos_frame.y + scal_size_frame.y))
            bg_pos.y += (pos_frame.y + scal_size_frame.y) - (bg_pos.y + bg_scal_size.y);
        if (bg_pos.y < pos_frame.y)
            bg_pos.y += pos_frame.y - bg_pos.y;

        if ((img_pos.x + img_scal_size.x) > (pos_frame.x + scal_size_frame.x))
            img_size.x += (pos_frame.x + scal_size_frame.x) - (img_pos.x + img_scal_size.x);
        if (img_pos.x < pos_frame.x)
            img_pos.x += pos_frame.x - img_pos.x;

        if ((img_pos.y + img_scal_size.y) > (pos_frame.y + scal_size_frame.y))
            img_pos.y += (pos_frame.y + scal_size_frame.y) - (img_pos.y + img_scal_size.y);
        if (img_pos.y < pos_frame.y)
            img_pos.y += pos_frame.y - img_pos.y;

        // Display items
        if (gui.live_items[app_path][frame.id].find("background") != gui.live_items[app_path][frame.id].end()) {
            ImGui::SetCursorPos(bg_pos);
            ImGui::Image(gui.live_items[app_path][frame.id]["background"][current_item[app_path][frame.id]], bg_scal_size);
        }
        if (gui.live_items[app_path][frame.id].find("image") != gui.live_items[app_path][frame.id].end()) {
            ImGui::SetCursorPos(img_pos);
            ImGui::Image(gui.live_items[app_path][frame.id]["image"][current_item[app_path][frame.id]], img_scal_size);
        }

        // Target link
        if (!target[app_path][frame.id].empty() && (target[app_path][frame.id].find("psts:") == std::string::npos)) {
            ImGui::SetCursorPos(pos_frame);
            ImGui::PushID(frame.id.c_str());
            if (ImGui::Selectable("##target_link", false, ImGuiSelectableFlags_None, scal_size_frame))
                system((OS_PREFIX + target[app_path][frame.id]).c_str());
            ImGui::PopID();
        }

        // Text
        for (const auto &str_tag : str[app_path][frame.id]) {
            if (!str_tag.text.empty()) {
                std::vector<ImVec4> str_color;

                if (!str_tag.color.empty()) {
                    int color;

                    if (frame.autoflip)
                        sscanf(str[app_path][frame.id][current_item[app_path][frame.id]].color.c_str(), "#%x", &color);
                    else
                        sscanf(str_tag.color.c_str(), "#%x", &color);

                    str_color.push_back(ImVec4(((color >> 16) & 0xFF) / 255.f, ((color >> 8) & 0xFF) / 255.f, ((color >> 0) & 0xFF) / 255.f, 1.f));
                } else
                    str_color.push_back(ImVec4(1.f, 1.f, 1.f, 1.f));

                auto str_size = scal_size_frame, text_pos = pos_frame;

                // Origin
                if (liveitem[app_path][frame.id]["text"]["origin"].second.empty() || (liveitem[app_path][frame.id]["text"]["origin"].second == "background")) {
                    if (gui.live_items[app_path][frame.id].find("background") != gui.live_items[app_path][frame.id].end())
                        str_size = bg_scal_size, text_pos = bg_pos;
                    else if (!liveitem[app_path][frame.id]["text"]["origin"].second.empty() && (gui.live_items[app_path][frame.id].find("image") != gui.live_items[app_path][frame.id].end()))
                        str_size = img_scal_size, text_pos = img_pos;
                } else if (liveitem[app_path][frame.id]["text"]["origin"].second == "image") {
                    if (gui.live_items[app_path][frame.id].find("image") != gui.live_items[app_path][frame.id].end())
                        str_size = img_scal_size, text_pos = img_pos;
                    else if (gui.live_items[app_path][frame.id].find("background") != gui.live_items[app_path][frame.id].end())
                        str_size = bg_scal_size, text_pos = bg_pos;
                }

                auto str_wrap = scal_size_frame.x;
                if (liveitem[app_path][frame.id]["text"]["allign"].second == "outside-right")
                    str_wrap = str_size.x;

                if (liveitem[app_path][frame.id]["text"]["width"].first > 0) {
                    if (liveitem[app_path][frame.id]["text"]["word-wrap"].second != "off")
                        str_wrap = float(liveitem[app_path][frame.id]["text"]["width"].first) * SCALE.x;
                    text_pos.x += (str_size.x - (float(liveitem[app_path][frame.id]["text"]["width"].first) * SCALE.x)) / 2.f;
                    str_size.x = float(liveitem[app_path][frame.id]["text"]["width"].first) * SCALE.x;
                }

                if ((liveitem[app_path][frame.id]["text"]["height"].first > 0)
                    && ((liveitem[app_path][frame.id]["text"]["word-scroll"].second == "on" || liveitem[app_path][frame.id]["text"]["height"].first <= FRAME_SIZE.y))) {
                    text_pos.y += (str_size.y - (float(liveitem[app_path][frame.id]["text"]["height"].first) * SCALE.y)) / 2.f;
                    str_size.y = float(liveitem[app_path][frame.id]["text"]["height"].first) * SCALE.y;
                }

                const auto size_text_scale = str_tag.size != 0 ? str_tag.size / 19.2f : 1.f;
                ImGui::SetWindowFontScale(size_text_scale * RES_SCALE.x);

                // Calcule text pixel size
                ImVec2 calc_text_size;
                if (frame.autoflip > 0) {
                    if (liveitem[app_path][frame.id]["text"]["word-wrap"].second != "off")
                        calc_text_size = ImGui::CalcTextSize(str[app_path][frame.id][current_item[app_path][frame.id]].text.c_str(), 0, false, str_wrap);
                    else
                        calc_text_size = ImGui::CalcTextSize(str[app_path][frame.id][current_item[app_path][frame.id]].text.c_str());
                } else {
                    if (liveitem[app_path][frame.id]["text"]["word-wrap"].second != "off")
                        calc_text_size = ImGui::CalcTextSize(str_tag.text.c_str(), 0, false, str_wrap);
                    else
                        calc_text_size = ImGui::CalcTextSize(str_tag.text.c_str());
                }

                /*if (liveitem[app_path][frame.id]["text"]["ellipsis"].second == "on") {
                    // TODO ellipsis
                }*/

                ImVec2 str_pos_init;

                // Allign
                if (liveitem[app_path][frame.id]["text"]["align"].second.empty()) {
                    if (liveitem[app_path][frame.id]["text"]["text-align"].second.empty()) {
                        if (liveitem[app_path][frame.id]["text"]["line-align"].second == "left")
                            str_pos_init.x = 0.f;
                        else if (liveitem[app_path][frame.id]["text"]["line-align"].second == "right")
                            str_pos_init.x = str_size.x - calc_text_size.x;
                        else if (liveitem[app_path][frame.id]["text"]["line-align"].second == "outside-right") {
                            text_pos.x += str_size.x;
                            if (liveitem[app_path][frame.id]["text"]["origin"].second == "image") {
                                if (gui.live_items[app_path][frame.id].find("background") != gui.live_items[app_path][frame.id].end())
                                    str_size.x = bg_scal_size.x - img_scal_size.x - (img_pos.x - bg_pos.x);
                                else
                                    str_size = scal_size_frame, text_pos = pos_frame;
                            }
                        } else
                            str_pos_init.x = (str_size.x - calc_text_size.x) / 2.0f;
                    } else {
                        if ((liveitem[app_path][frame.id]["text"]["text-align"].second == "center")
                            || ((liveitem[app_path][frame.id]["text"]["text-align"].second == "left") && (liveitem[app_path][frame.id]["text"]["x"].first < 0)))
                            str_pos_init.x = (str_size.x - calc_text_size.x) / 2.0f;
                        else if (liveitem[app_path][frame.id]["text"]["text-align"].second == "left")
                            str_pos_init.x = 0.f;
                        else if (liveitem[app_path][frame.id]["text"]["text-align"].second == "right")
                            str_pos_init.x = str_size.x - calc_text_size.x;
                        else if (liveitem[app_path][frame.id]["text"]["text-align"].second == "outside-right") {
                            text_pos.x += str_size.x;
                            if (liveitem[app_path][frame.id]["text"]["origin"].second == "image") {
                                if (gui.live_items[app_path][frame.id].find("background") != gui.live_items[app_path][frame.id].end())
                                    str_size.x = bg_scal_size.x - img_scal_size.x - (img_pos.x - bg_pos.x);
                                else
                                    str_size = scal_size_frame, text_pos = pos_frame;
                            }
                        }
                    }
                } else {
                    if (liveitem[app_path][frame.id]["text"]["align"].second == "center")
                        str_pos_init.x = (str_size.x - calc_text_size.x) / 2.0f;
                    else if (liveitem[app_path][frame.id]["text"]["align"].second == "left")
                        str_pos_init.x = 0.f;
                    else if (liveitem[app_path][frame.id]["text"]["align"].second == "right")
                        str_pos_init.x = str_size.x - calc_text_size.x;
                    else if (liveitem[app_path][frame.id]["text"]["align"].second == "outside-right") {
                        text_pos.x += str_size.x;
                        if (liveitem[app_path][frame.id]["text"]["origin"].second == "image") {
                            if (gui.live_items[app_path][frame.id].find("background") != gui.live_items[app_path][frame.id].end())
                                str_size.x = bg_scal_size.x - img_scal_size.x - (img_pos.x - bg_pos.x);
                            else
                                str_size = scal_size_frame, text_pos = pos_frame;
                        }
                    }
                }

                // Valign
                if (liveitem[app_path][frame.id]["text"]["valign"].second.empty()) {
                    if (liveitem[app_path][frame.id]["text"]["text-valign"].second.empty() || (liveitem[app_path][frame.id]["text"]["text-valign"].second == "center"))
                        str_pos_init.y = (str_size.y - calc_text_size.y) / 2.f;
                    else if (liveitem[app_path][frame.id]["text"]["text-valign"].second == "bottom")
                        str_pos_init.y = str_size.y - calc_text_size.y;
                    else if (liveitem[app_path][frame.id]["text"]["text-valign"].second == "top")
                        str_pos_init.y = 0.f;
                } else {
                    if ((liveitem[app_path][frame.id]["text"]["valign"].second == "center")
                        || ((liveitem[app_path][frame.id]["text"]["valign"].second == "bottom") && (liveitem[app_path][frame.id]["text"]["y"].first != 0))
                        || ((liveitem[app_path][frame.id]["text"]["valign"].second == "top") && (liveitem[app_path][frame.id]["text"]["y"].first != 0)))
                        str_pos_init.y = (str_size.y - calc_text_size.y) / 2.f;
                    else if (liveitem[app_path][frame.id]["text"]["valign"].second == "bottom")
                        str_pos_init.y = str_size.y - calc_text_size.y;
                    else if (liveitem[app_path][frame.id]["text"]["valign"].second == "top") {
                        str_pos_init.y = 0.f;
                    } else if (liveitem[app_path][frame.id]["text"]["valign"].second == "outside-top") {
                        str_pos_init.y = 0.f;
                    }
                }

                auto pos_str = ImVec2(str_pos_init.x, str_pos_init.y - (liveitem[app_path][frame.id]["text"]["y"].first * SCALE.y));

                if (liveitem[app_path][frame.id]["text"]["x"].first > 0) {
                    text_pos.x += liveitem[app_path][frame.id]["text"]["x"].first * SCALE.x;
                    str_size.x -= liveitem[app_path][frame.id]["text"]["x"].first * SCALE.x;
                }

                if ((liveitem[app_path][frame.id]["text"]["margin-left"].first > 0) && !liveitem[app_path][frame.id]["text"]["width"].first) {
                    text_pos.x += liveitem[app_path][frame.id]["text"]["margin-left"].first * SCALE.x;
                    str_size.x -= liveitem[app_path][frame.id]["text"]["margin-left"].first * SCALE.x;
                }

                if (liveitem[app_path][frame.id]["text"]["margin-right"].first > 0)
                    str_size.x -= liveitem[app_path][frame.id]["text"]["margin-right"].first * SCALE.x;

                if (liveitem[app_path][frame.id]["text"]["margin-top"].first > 0) {
                    text_pos.y += liveitem[app_path][frame.id]["text"]["margin-top"].first * SCALE.y;
                    str_size.y -= liveitem[app_path][frame.id]["text"]["margin-top"].first * SCALE.y;
                }

                // Text Display
                // TODO Multible color support on same frame, used by eg: Asphalt: Injection
                // TODO Correct display few line on same frame, used by eg: Asphalt: Injection
                ImGui::SetNextWindowPos(text_pos);
                ImGui::BeginChild(frame.id.c_str(), str_size, false, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
                if (liveitem[app_path][frame.id]["text"]["word-wrap"].second != "off")
                    ImGui::PushTextWrapPos(str_wrap);
                if (liveitem[app_path][frame.id]["text"]["word-scroll"].second == "on") {
                    static std::map<std::string, std::map<std::string, std::pair<bool, float>>> scroll;
                    if (liveitem[app_path][frame.id]["text"]["word-wrap"].second == "off") {
                        if (scroll[app_path][frame.id].first) {
                            if (scroll[app_path][frame.id].second >= 0.f)
                                scroll[app_path][frame.id].second -= 0.5f;
                            else
                                scroll[app_path][frame.id].first = false;
                        } else {
                            if (scroll[app_path][frame.id].second <= ImGui::GetScrollMaxX())
                                scroll[app_path][frame.id].second += 0.5f;
                            else
                                scroll[app_path][frame.id].first = true;
                        }
                        ImGui::SetScrollX(scroll[app_path][frame.id].second);
                    } else {
                        if (scroll[app_path][frame.id].first) {
                            if (scroll[app_path][frame.id].second >= 0.f)
                                scroll[app_path][frame.id].second -= 0.5f;
                            else
                                scroll[app_path][frame.id].first = false;
                        } else {
                            if (scroll[app_path][frame.id].second <= ImGui::GetScrollMaxY())
                                scroll[app_path][frame.id].second += 0.5f;
                            else
                                scroll[app_path][frame.id].first = true;
                        }
                        ImGui::SetScrollY(scroll[app_path][frame.id].second);
                    }
                }
                ImGui::SetCursorPos(pos_str);
                if (frame.autoflip > 0)
                    ImGui::TextColored(str_color[0], "%s", str[app_path][frame.id][current_item[app_path][frame.id]].text.c_str());
                else
                    ImGui::TextColored(str_color[0], "%s", str_tag.text.c_str());
                if (liveitem[app_path][frame.id]["text"]["word-wrap"].second != "off")
                    ImGui::PopTextWrapPos();
                ImGui::EndChild();
                ImGui::SetWindowFontScale(RES_SCALE.x);
            }
        }
    }

    ImGui::SetWindowFontScale(RES_SCALE.x);
    const auto default_font_scale = (25.f * host.dpi_scale) * (ImGui::GetFontSize() / (19.2f * host.dpi_scale));
    const auto font_size_scale = default_font_scale / ImGui::GetFontSize();

    const std::string BUTTON_STR = app_path == host.io.app_path ? resume : start;
    const auto GATE_SIZE = ImVec2(280.0f * SCALE.x, 158.0f * SCALE.y);
    const auto GATE_POS = ImVec2(display_size.x - (items_pos[type[app_path]]["gate"]["pos"].x * SCALE.x), display_size.y - (items_pos[type[app_path]]["gate"]["pos"].y * SCALE.y));
    const auto START_SIZE = ImVec2((ImGui::CalcTextSize(BUTTON_STR.c_str()).x * font_size_scale), (ImGui::CalcTextSize(BUTTON_STR.c_str()).y * font_size_scale));
    const auto START_BUTTON_SIZE = ImVec2(START_SIZE.x + 26.0f * SCALE.x, START_SIZE.y + 5.0f * SCALE.y);
    const auto POS_BUTTON = ImVec2((GATE_POS.x + (GATE_SIZE.x - START_BUTTON_SIZE.x) / 2.0f), (GATE_POS.y + (GATE_SIZE.y - START_BUTTON_SIZE.y) / 1.08f));
    const auto POS_START = ImVec2(POS_BUTTON.x + (START_BUTTON_SIZE.x - START_SIZE.x) / 2.f, POS_BUTTON.y + (START_BUTTON_SIZE.y - START_SIZE.y) / 2.f);
    const auto SELECT_SIZE = ImVec2(GATE_SIZE.x - (10.f * SCALE.x), GATE_SIZE.y - (5.f * SCALE.y));
    const auto SELECT_POS = ImVec2(GATE_POS.x + (5.f * SCALE.y), GATE_POS.y + (2.f * SCALE.y));
    const auto SIZE_GATE = ImVec2(GATE_POS.x + GATE_SIZE.x, GATE_POS.y + GATE_SIZE.y);

    const auto BUTTON_SIZE = ImVec2(72.f * SCALE.x, 30.f * SCALE.y);

    if (gui.live_area_contents[app_path].find("gate") != gui.live_area_contents[app_path].end()) {
        ImGui::SetCursorPos(GATE_POS);
        ImGui::Image(gui.live_area_contents[app_path]["gate"], GATE_SIZE);
    }
    ImGui::PushID(app_path.c_str());
    ImGui::GetWindowDrawList()->AddRectFilled(POS_BUTTON, ImVec2(POS_BUTTON.x + START_BUTTON_SIZE.x, POS_BUTTON.y + START_BUTTON_SIZE.y), IM_COL32(20, 168, 222, 255), 10.0f * SCALE.x, ImDrawCornerFlags_All);
    ImGui::GetWindowDrawList()->AddText(gui.vita_font, default_font_scale, POS_START, IM_COL32(255, 255, 255, 255), BUTTON_STR.c_str());
    ImGui::SetCursorPos(SELECT_POS);
    ImGui::SetCursorPos(SELECT_POS);
    if (ImGui::Selectable("##gate", false, ImGuiSelectableFlags_None, SELECT_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross))
        pre_run_app(gui, host, app_path);
    ImGui::PopID();
    ImGui::GetWindowDrawList()->AddRect(GATE_POS, SIZE_GATE, IM_COL32(192, 192, 192, 255), 10.f * SCALE.x, ImDrawCornerFlags_All, 12.f * SCALE.x);

    if (app_device == VitaIoDevice::ux0) {
        const auto widget_scal_size = ImVec2(80.0f * SCALE.x, 80.f * SCALE.y);
        const auto manual_path{ fs::path(host.pref_path) / "ux0/app" / app_path / "sce_sys/manual/" };
        const auto scal_widget_font_size = 23.0f / ImGui::GetFontSize();

        auto search_pos = ImVec2(578.0f * SCALE.x, 505.0f * SCALE.y);
        if (!fs::exists(manual_path) || fs::is_empty(manual_path))
            search_pos = ImVec2(520.0f * SCALE.x, 505.0f * SCALE.y);

        const auto pos_scal_search = ImVec2(display_size.x - search_pos.x, display_size.y - search_pos.y);

        const std::string SEARCH = "Search";
        const auto SEARCH_SCAL_SIZE = ImVec2((ImGui::CalcTextSize(SEARCH.c_str()).x * scal_widget_font_size) * SCALE.x, (ImGui::CalcTextSize(SEARCH.c_str()).y * scal_widget_font_size) * SCALE.y);
        const auto POS_STR_SEARCH = ImVec2(pos_scal_search.x + ((widget_scal_size.x / 2.f) - (SEARCH_SCAL_SIZE.x / 2.f)),
            pos_scal_search.y + ((widget_scal_size.x / 2.f) - (SEARCH_SCAL_SIZE.y / 2.f)));
        ImGui::GetWindowDrawList()->AddRectFilled(pos_scal_search, ImVec2(pos_scal_search.x + widget_scal_size.x, pos_scal_search.y + widget_scal_size.y), IM_COL32(20, 168, 222, 255), 12.0f * SCALE.x, ImDrawCornerFlags_All);
        ImGui::GetWindowDrawList()->AddText(gui.vita_font, 23.0f * SCALE.x, POS_STR_SEARCH, IM_COL32(255, 255, 255, 255), SEARCH.c_str());
        ImGui::SetCursorPos(pos_scal_search);
        if (ImGui::Selectable("##Search", ImGuiSelectableFlags_None, false, widget_scal_size)) {
            auto search_url = "http://www.google.com/search?q=" + get_app_index(gui, app_path)->title;
            std::replace(search_url.begin(), search_url.end(), ' ', '+');
            system((OS_PREFIX + search_url).c_str());
        }

        if (fs::exists(manual_path) && !fs::is_empty(manual_path)) {
            const auto manual_pos = ImVec2(463.0f * SCALE.x, 505.0f * SCALE.y);
            const auto pos_scal_manual = ImVec2(display_size.x - manual_pos.x, display_size.y - manual_pos.y);

            const std::string MANUAL_STR = "Manual";
            const auto MANUAL_STR_SCAL_SIZE = ImVec2((ImGui::CalcTextSize(MANUAL_STR.c_str()).x * scal_widget_font_size) * SCALE.x, (ImGui::CalcTextSize(MANUAL_STR.c_str()).y * scal_widget_font_size) * SCALE.y);
            const auto MANUAL_STR_POS = ImVec2(pos_scal_manual.x + ((widget_scal_size.x / 2.f) - (MANUAL_STR_SCAL_SIZE.x / 2.f)),
                pos_scal_manual.y + ((widget_scal_size.x / 2.f) - (MANUAL_STR_SCAL_SIZE.y / 2.f)));
            ImGui::GetWindowDrawList()->AddRectFilled(pos_scal_manual, ImVec2(pos_scal_manual.x + widget_scal_size.x, pos_scal_manual.y + widget_scal_size.y), IM_COL32(202, 0, 106, 255), 12.0f * SCALE.x, ImDrawCornerFlags_All);
            ImGui::GetWindowDrawList()->AddText(gui.vita_font, 23.0f * SCALE.x, MANUAL_STR_POS, IM_COL32(255, 255, 255, 255), MANUAL_STR.c_str());
            ImGui::SetCursorPos(pos_scal_manual);
            if (ImGui::Selectable("##manual", ImGuiSelectableFlags_None, false, widget_scal_size)) {
                if (init_manual(gui, host, app_path)) {
                    gui.live_area.information_bar = false;
                    gui.live_area.live_area_screen = false;
                    gui.live_area.manual = true;
                } else
                    LOG_ERROR("Error opening Manual");
            }
        }
    }

    if (!gui.live_area.content_manager && !gui.live_area.manual) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f * SCALE.x);
        ImGui::SetCursorPos(ImVec2(display_size.x - (60.0f * SCALE.x) - BUTTON_SIZE.x, 44.0f * SCALE.y));
        if (ImGui::Button("Esc", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_circle)) {
            if (app_path == host.io.app_path) {
                update_time_app_used(gui, host, app_path);
                host.kernel.exit_delete_all_threads();
                host.load_exec = true;
            } else {
                gui.apps_list_opened.erase(get_app_open_list_index(gui, app_path));
                if (gui.current_app_selected == 0) {
                    gui.live_area.live_area_screen = false;
                    gui.live_area.app_selector = true;
                }
                --gui.current_app_selected;
            }
        }
        ImGui::SetCursorPos(ImVec2(60.f * SCALE.x, 44.0f * SCALE.y));
        if (ImGui::Button("Help", BUTTON_SIZE))
            ImGui::OpenPopup("Live Area Help");
        ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Live Area Help", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2.f - (ImGui::CalcTextSize("Help").x / 2.f));
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Help");
            ImGui::Spacing();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2.f - (ImGui::CalcTextSize("Using configuration set for keyboard in control setting").x / 2.f));
            ImGui::TextColored(GUI_COLOR_TEXT, "Using configuration set for keyboard in control setting");
            if (gui.modules.empty()) {
                ImGui::Spacing();
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2.f - (ImGui::CalcTextSize("Firmware not detected, Install it is recommended for font text in Livea Area").x / 2.f));
                ImGui::TextColored(GUI_COLOR_TEXT, "Firmware not detected, Install it is recommended for font text in Livea Area");
            }
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Live Area Help");
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT, "%-16s    %-16s", "Browse in app list", "D-pad, Left Stick, Wheel in Up/Down or using Slider");
            ImGui::TextColored(GUI_COLOR_TEXT, "%-16s    %-16s", "Start App", "Click on Start or Press on Cross");
            ImGui::TextColored(GUI_COLOR_TEXT, "%-16s    %-16s", "Show/Hide Live Area during app run", "Press on PS");
            ImGui::TextColored(GUI_COLOR_TEXT, "%-16s    %-16s", "Exit Live Area", "Click on Esc or Press on Circle");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Manual Help");
            ImGui::Spacing();
            ImGui::TextColored(GUI_COLOR_TEXT, "%-16s    %-16s", "Browse page", "D-pad, Left Stick in Left/Right or Wheel in Up/Down or Click on </>");
            ImGui::TextColored(GUI_COLOR_TEXT, "%-16s    %-16s", "Hide/Show button", "Right Click");
            ImGui::TextColored(GUI_COLOR_TEXT, "%-16s    %-16s", "Zoom if available", "Double left click");
            ImGui::TextColored(GUI_COLOR_TEXT, "%-16s    %-16s", "Scroll in zoom", "Wheel Up/Down");
            ImGui::TextColored(GUI_COLOR_TEXT, "%-16s    %-16s", "Exit Manual", "Click on X or Press on PS");
            ImGui::Spacing();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2.f - (BUTTON_SIZE.x / 2.f));
            if (ImGui::Button("OK", BUTTON_SIZE))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
    }

    const auto SELECTABLE_SIZE = ImVec2(50.f * SCALE.x, 60.f * SCALE.y);
    const auto wheel_counter = ImGui::GetIO().MouseWheel;
    const auto ARROW_LEFT_CENTER = ImVec2(30.f * SCALE.x, display_size.y - (250.f * SCALE.y));
    ImGui::GetForegroundDrawList()->AddTriangleFilled(
        ImVec2(ARROW_LEFT_CENTER.x + (16.f * SCALE.x), ARROW_LEFT_CENTER.y - (20.f * SCALE.y)),
        ImVec2(ARROW_LEFT_CENTER.x - (16.f * SCALE.x), ARROW_LEFT_CENTER.y),
        ImVec2(ARROW_LEFT_CENTER.x + (16.f * SCALE.x), ARROW_LEFT_CENTER.y + (20.f * SCALE.y)), ARROW_COLOR);
    ImGui::SetCursorPos(ImVec2(ARROW_LEFT_CENTER.x - (SELECTABLE_SIZE.x / 2.f), ARROW_LEFT_CENTER.y - (SELECTABLE_SIZE.y / 2.f)));
    if ((ImGui::Selectable("##left", false, ImGuiSelectableFlags_None, SELECTABLE_SIZE)) || ImGui::IsKeyPressed(host.cfg.keyboard_button_l1) || ImGui::IsKeyPressed(host.cfg.keyboard_leftstick_left) || (wheel_counter == 1)) {
        if (gui.current_app_selected == 0) {
            gui.live_area.live_area_screen = false;
            gui.live_area.app_selector = true;
        }
        --gui.current_app_selected;
    }
    if (gui.current_app_selected < gui.apps_list_opened.size() - 1) {
        const auto ARROW_RIGHT_CENTER = ImVec2(display_size.x - (30.f * SCALE.x), display_size.y - (250.f * SCALE.y));
        ImGui::GetForegroundDrawList()->AddTriangleFilled(
            ImVec2(ARROW_RIGHT_CENTER.x - (16.f * SCALE.x), ARROW_RIGHT_CENTER.y - (20.f * SCALE.y)),
            ImVec2(ARROW_RIGHT_CENTER.x + (16.f * SCALE.x), ARROW_RIGHT_CENTER.y),
            ImVec2(ARROW_RIGHT_CENTER.x - (16.f * SCALE.x), ARROW_RIGHT_CENTER.y + (20.f * SCALE.y)), ARROW_COLOR);
        ImGui::SetCursorPos(ImVec2(ARROW_RIGHT_CENTER.x - (SELECTABLE_SIZE.x / 2.f), ARROW_RIGHT_CENTER.y - (SELECTABLE_SIZE.y / 2.f)));
        if ((ImGui::Selectable("##right", false, ImGuiSelectableFlags_None, SELECTABLE_SIZE)) || ImGui::IsKeyPressed(host.cfg.keyboard_button_r1) || ImGui::IsKeyPressed(host.cfg.keyboard_leftstick_right) || (wheel_counter == -1))
            ++gui.current_app_selected;
    }
    ImGui::SetWindowFontScale(1.f);
    ImGui::End();
}

} // namespace gui
