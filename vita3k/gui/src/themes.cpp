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

#include <config/functions.h>
#include <config/state.h>
#include <gui/functions.h>

#include <util/log.h>
#include <util/string_utils.h>

#include <io/device.h>

#include <nfd.h>
#include <pugixml.hpp>
#include <stb_image.h>

namespace gui {

static void add_user_image_background(GuiState &gui, HostState &host) {
    nfdchar_t *image_path;
    nfdresult_t result = NFD_OpenDialog("bmp,gif,jpg,png,tif", nullptr, &image_path);

    if ((result == NFD_OKAY) && (gui.user_backgrounds.find(image_path) == gui.user_backgrounds.end())) {
        const std::wstring image_path_wstr = string_utils::utf_to_wide(image_path);

        if (!fs::exists(fs::path(image_path_wstr))) {
            LOG_WARN("Image doesn't exist: {}.", image_path);
            return;
        }

        int32_t width = 0;
        int32_t height = 0;

#ifdef _WIN32
        FILE *f = _wfopen(image_path_wstr.c_str(), L"rb");
#else
        FILE *f = fopen(image_path, "rb");
#endif

        stbi_uc *data = stbi_load_from_file(f, &width, &height, nullptr, STBI_rgb_alpha);

        if (!data) {
            LOG_ERROR("Invalid or corrupted image: {}.", image_path);
            return;
        }

        gui.user_backgrounds[image_path].init(gui.imgui_state.get(), data, width, height);
        stbi_image_free(data);
        fclose(f);

        if (gui.user_backgrounds[image_path]) {
            gui.current_user_bg = 0;
            host.cfg->user_backgrounds.push_back(image_path);
            host.cfg->use_theme_background = false;
            config::serialize_config(*host.cfg, host.cfg->config_path);
        }
    }
}

static std::map<std::string, std::string> start_param;
static std::map<std::string, ImVec2> date;
static ImU32 date_color;

void init_theme_start_background(GuiState &gui, HostState &host, const std::string &content_id) {
    std::string theme_start_name;

    start_param.clear();
    gui.start_background = {};
    if (!content_id.empty() && content_id != "default") {
        const auto THEME_PATH_XML{ fs::path(host.pref_path) / "ux0/theme" / content_id / "theme.xml" };
        pugi::xml_document doc;
        if (doc.load_file(THEME_PATH_XML.c_str())) {
            const auto theme = doc.child("theme");

            // Start Layout
            if (!theme.child("StartScreenProperty").child("m_dateColor").empty())
                start_param["dateColor"] = theme.child("StartScreenProperty").child("m_dateColor").text().as_string();
            if (!theme.child("StartScreenProperty").child("m_dateLayout").empty())
                start_param["dateLayout"] = theme.child("StartScreenProperty").child("m_dateLayout").text().as_string();

            // Theme Start
            if (!theme.child("StartScreenProperty").child("m_filePath").empty()) {
                theme_start_name = theme.child("StartScreenProperty").child("m_filePath").text().as_string();
            }
        }
    }

    date["date"] = ImVec2(900.f, 192.f);
    date["clock"] = ImVec2(900.f, 160.f);
    if (!start_param["dateLayout"].empty()) {
        switch (std::stoi(start_param["dateLayout"])) {
        case 0:
            break;
        case 1:
            date["date"].y = 468.f;
            date["clock"].y = 436.f;
            break;
        case 2:
            date["date"].x = 50.f;
            date["clock"].x = 50.f;
            break;
        default:
            LOG_WARN("Date layout is unknown : {}", start_param["dateLayout"]);
            break;
        }
    }

    if (!start_param["dateColor"].empty()) {
        int color;
        sscanf(start_param["dateColor"].c_str(), "%x", &color);
        date_color = (color & 0xFF00FF00u) | ((color & 0x00FF0000u) >> 16u) | ((color & 0x000000FFu) << 16u);
    } else
        date_color = 4294967295; // White

    int32_t width = 0;
    int32_t height = 0;
    vfs::FileBuffer buffer;

    if (theme_start_name.empty()) {
        const auto DEFAULT_START_PATH{ fs::path("data/internal/keylock/keylock.png") };
        if (fs::exists(fs::path(host.pref_path) / "vs0" / DEFAULT_START_PATH))
            vfs::read_file(VitaIoDevice::vs0, buffer, host.pref_path, DEFAULT_START_PATH);
        else {
            LOG_WARN("Default start background not found, install firmware for fix this.");
            return;
        }
    } else
        vfs::read_file(VitaIoDevice::ux0, buffer, host.pref_path, "theme/" + content_id + "/" + theme_start_name);

    if (buffer.empty()) {
        LOG_WARN("Background not found: '{}', for content id: {}.", theme_start_name, content_id);
        return;
    }
    stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
    if (!data) {
        LOG_ERROR("Invalid Background: '{}' for content id: {}.", theme_start_name, content_id);
        return;
    }

    gui.start_background.init(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);
}

bool init_user_start_background(GuiState &gui, const std::string &image_path) {
    const std::wstring image_path_wstr = string_utils::utf_to_wide(image_path);

    if (!fs::exists(image_path_wstr)) {
        LOG_WARN("Image doesn't exist: {}.", image_path);
        return false;
    }

    gui.start_background = {};
    int32_t width = 0;
    int32_t height = 0;

#ifdef _WIN32
    FILE *f = _wfopen(image_path_wstr.c_str(), L"rb");
#else
    FILE *f = fopen(image_path.c_str(), "rb");
#endif

    stbi_uc *data = stbi_load_from_file(f, &width, &height, nullptr, STBI_rgb_alpha);

    if (!data) {
        LOG_ERROR("Invalid or corrupted image: {}.", image_path);
        return false;
    }

    gui.start_background.init(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);
    fclose(f);

    date["date"] = ImVec2(898, 186.f);
    date["clock"] = ImVec2(898.f, 146.f);

    date_color = 4294967295; // White

    return gui.start_background;
}

bool init_theme(GuiState &gui, HostState &host, const std::string &content_id) {
    std::vector<std::string> theme_bg_name;
    std::map<std::string, std::string> bar_param;
    std::map<std::string, std::string> theme_icon_name;

    gui.app_selector.sys_apps_icon.clear();
    gui.current_theme_bg = 0;
    gui.information_bar_color.clear();
    gui.theme_backgrounds.clear();
    gui.theme_backgrounds_font_color.clear();
    gui.theme_information_bar_notice.clear();

    if (content_id != "default") {
        const auto THEME_XML_PATH{ fs::path(host.pref_path) / "ux0/theme" / content_id / "theme.xml" };
        pugi::xml_document doc;

        if (doc.load_file(THEME_XML_PATH.c_str())) {
            const auto theme = doc.child("theme");

            // Theme Apps Icon
            if (!theme.child("HomeProperty").child("m_trophy").child("m_iconFilePath").text().empty())
                theme_icon_name["NPXS10008"] = theme.child("HomeProperty").child("m_trophy").child("m_iconFilePath").text().as_string();
            if (!theme.child("HomeProperty").child("m_settings").child("m_iconFilePath").text().empty())
                theme_icon_name["NPXS10015"] = theme.child("HomeProperty").child("m_settings").child("m_iconFilePath").text().as_string();
            if (!theme.child("HomeProperty").child("m_hostCollabo").child("m_iconFilePath").text().empty())
                theme_icon_name["NPXS10026"] = theme.child("HomeProperty").child("m_hostCollabo").child("m_iconFilePath").text().as_string();

            // Get Bar Color
            if (!theme.child("InfomationBarProperty").child("m_barColor").empty())
                bar_param["barColor"] = theme.child("InfomationBarProperty").child("m_barColor").text().as_string();
            if (!theme.child("InfomationBarProperty").child("m_indicatorColor").empty())
                bar_param["indicatorColor"] = theme.child("InfomationBarProperty").child("m_indicatorColor").text().as_string();

            // Notice Icon
            std::map<std::string, std::string> notice_name;
            if (!theme.child("InfomationBarProperty").child("m_noNoticeFilePath").text().empty())
                notice_name["no"] = theme.child("InfomationBarProperty").child("m_noNoticeFilePath").text().as_string();
            if (!theme.child("InfomationBarProperty").child("m_newNoticeFilePath").text().empty())
                notice_name["new"] = theme.child("InfomationBarProperty").child("m_newNoticeFilePath").text().as_string();

            for (const auto &notice : notice_name) {
                int32_t width = 0;
                int32_t height = 0;
                vfs::FileBuffer buffer;

                const auto type = notice.first;
                const auto name = notice.second;

                vfs::read_file(VitaIoDevice::ux0, buffer, host.pref_path, "theme/" + content_id + "/" + name);

                if (buffer.empty()) {
                    LOG_WARN("Notice icon, Name: '{}', Not found for content id: {}.", name, content_id);
                    continue;
                }
                stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                if (!data) {
                    LOG_ERROR("Invalid notice icon for content id: {}.", content_id);
                    continue;
                }
                gui.theme_information_bar_notice[type].init(gui.imgui_state.get(), data, width, height);
                stbi_image_free(data);
            }

            // Home
            for (const auto &param : theme.child("HomeProperty").child("m_bgParam")) {
                // Theme Background
                if (!param.child("m_imageFilePath").text().empty()) {
                    theme_bg_name.push_back(param.child("m_imageFilePath").text().as_string());
                    gui.theme_backgrounds.push_back({});
                }
                // Font Color
                if (!param.child("m_fontColor").text().empty()) {
                    int color;
                    sscanf(param.child("m_fontColor").text().as_string(), "%x", &color);
                    gui.theme_backgrounds_font_color.push_back(ImVec4((float((color >> 16) & 0xFF)) / 255.f, (float((color >> 8) & 0xFF)) / 255.f, (float((color >> 0) & 0xFF)) / 255.f, 1.f));
                }
            }
        } else
            LOG_ERROR("theme.xml not found for Content ID: {}, in path: {}", content_id, THEME_XML_PATH.string());
    } else {
        const std::vector<std::string> app_id_bg_list = {
            "NPXS10002",
            "NPXS10006",
            "NPXS10013",
            "NPXS10018",
            "NPXS10098",
        };
        for (const auto &bg : app_id_bg_list) {
            if (fs::exists(fs::path(host.pref_path) / "vs0/app" / bg / "sce_sys/pic0.png")) {
                gui.theme_backgrounds.push_back({});
                theme_bg_name.push_back(bg);
            }
        }
    }

    if (!bar_param["barColor"].empty()) {
        int color;
        sscanf(bar_param["barColor"].c_str(), "%x", &color);
        gui.information_bar_color["bar"] = (color & 0xFF00FF00u) | ((color & 0x00FF0000u) >> 16u) | ((color & 0x000000FFu) << 16u);
    } else
        gui.information_bar_color["bar"] = 4278190080; // Black

    if (!bar_param["indicatorColor"].empty()) {
        int color;
        sscanf(bar_param["indicatorColor"].c_str(), "%x", &color);
        gui.information_bar_color["indicator"] = (color & 0xFF00FF00u) | ((color & 0x00FF0000u) >> 16u) | ((color & 0x000000FFu) << 16u);
    } else
        gui.information_bar_color["indicator"] = 4294967295; // White

    if (!theme_icon_name.empty()) {
        for (const auto &icon : theme_icon_name) {
            int32_t width = 0;
            int32_t height = 0;
            vfs::FileBuffer buffer;

            const auto title_id = icon.first;
            const auto name = icon.second;

            vfs::read_file(VitaIoDevice::ux0, buffer, host.pref_path, "theme/" + content_id + "/" + name);

            if (buffer.empty()) {
                LOG_WARN("Name: '{}', Not found icon for content id: {}.", name, content_id);
                continue;
            }
            stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
            if (!data) {
                LOG_ERROR("Name: '{}', Invalid icon for content id: {}.", name, content_id);
                continue;
            }

            gui.app_selector.sys_apps_icon[title_id].init(gui.imgui_state.get(), data, width, height);
            stbi_image_free(data);
        }
    } else
        init_apps_icon(gui, host, gui.app_selector.sys_apps);

    for (auto pos = 0; pos < theme_bg_name.size(); pos++) {
        int32_t width = 0;
        int32_t height = 0;
        vfs::FileBuffer buffer;

        if (content_id == "default")
            vfs::read_file(VitaIoDevice::vs0, buffer, host.pref_path, "app/" + theme_bg_name[pos] + "/sce_sys/pic0.png");
        else
            vfs::read_file(VitaIoDevice::ux0, buffer, host.pref_path, "theme/" + content_id + "/" + theme_bg_name[pos]);

        if (buffer.empty()) {
            LOG_WARN("Background not found: '{}', for content id: {}.", theme_bg_name[pos], content_id);
            continue;
        }
        stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
        if (!data) {
            LOG_ERROR("Invalid Background: '{}', for content id: {}.", theme_bg_name[pos], content_id);
            continue;
        }

        gui.theme_backgrounds[pos].init(gui.imgui_state.get(), data, width, height);
        stbi_image_free(data);
    }

    return !gui.theme_backgrounds.empty();
}

struct Theme {
    std::string title;
    std::string provided;
    std::string updated;
    size_t size;
    std::string version;
};

static std::map<std::string, Theme> themes_info;
static std::vector<std::pair<std::string, time_t>> themes_list;

void get_themes_list(GuiState &gui, HostState &host) {
    gui.themes_preview.clear(), themes_info.clear(), themes_list.clear();

    const auto theme_path{ fs::path(host.pref_path) / "ux0/theme" };
    const auto fw_theme_path{ fs::path(host.pref_path) / "vs0/data/internal/theme" };
    if ((!fs::exists(fw_theme_path) || fs::is_empty(fw_theme_path)) && (!fs::exists(theme_path) || fs::is_empty(theme_path))) {
        LOG_WARN("Theme path is empty");
        return;
    }

    std::string user_lang;
    const auto sys_lang = static_cast<SceSystemParamLang>(host.cfg->sys_lang);
    switch (sys_lang) {
    case SCE_SYSTEM_PARAM_LANG_JAPANESE: user_lang = "m_ja"; break;
    case SCE_SYSTEM_PARAM_LANG_ENGLISH_US: user_lang = "m_en"; break;
    case SCE_SYSTEM_PARAM_LANG_FRENCH: user_lang = "m_fr"; break;
    case SCE_SYSTEM_PARAM_LANG_SPANISH: user_lang = "m_es"; break;
    case SCE_SYSTEM_PARAM_LANG_GERMAN: user_lang = "m_de"; break;
    case SCE_SYSTEM_PARAM_LANG_ITALIAN: user_lang = "m_it"; break;
    case SCE_SYSTEM_PARAM_LANG_DUTCH: user_lang = "m_nl"; break;
    case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_PT: user_lang = "m_pt"; break;
    case SCE_SYSTEM_PARAM_LANG_RUSSIAN: user_lang = "m_ru"; break;
    case SCE_SYSTEM_PARAM_LANG_KOREAN: user_lang = "m_ko"; break;
    case SCE_SYSTEM_PARAM_LANG_CHINESE_T: user_lang = "m_ch"; break;
    case SCE_SYSTEM_PARAM_LANG_CHINESE_S: user_lang = "m_zh"; break;
    case SCE_SYSTEM_PARAM_LANG_FINNISH: user_lang = "m_fi"; break;
    case SCE_SYSTEM_PARAM_LANG_SWEDISH: user_lang = "m_sv"; break;
    case SCE_SYSTEM_PARAM_LANG_DANISH: user_lang = "m_da"; break;
    case SCE_SYSTEM_PARAM_LANG_NORWEGIAN: user_lang = "m_no"; break;
    case SCE_SYSTEM_PARAM_LANG_POLISH: user_lang = "m_pl"; break;
    case SCE_SYSTEM_PARAM_LANG_PORTUGUESE_BR: user_lang = "m_pt-br"; break;
    case SCE_SYSTEM_PARAM_LANG_ENGLISH_GB: user_lang = "m_en-gb"; break;
    case SCE_SYSTEM_PARAM_LANG_TURKISH: user_lang = "m_tr"; break;
    default: break;
    }

    std::map<std::string, std::map<std::string, std::string>> theme_preview_name;
    for (const auto &theme : fs::directory_iterator(theme_path)) {
        if (!theme.path().empty() && fs::is_directory(theme.path())
            && !theme.path().filename_is_dot() && !theme.path().filename_is_dot_dot()) {
            vfs::FileBuffer params;
            std::string content_id = theme.path().stem().generic_string();
            const auto theme_path_xml{ theme_path / content_id / "theme.xml" };
            pugi::xml_document doc;

            if (doc.load_file(theme_path_xml.c_str())) {
                const auto infomation = doc.child("theme").child("InfomationProperty");

                // Thumbmail theme
                theme_preview_name[content_id]["package"] = infomation.child("m_packageImageFilePath").text().as_string();

                // Preview theme
                theme_preview_name[content_id]["home"] = infomation.child("m_homePreviewFilePath").text().as_string();
                theme_preview_name[content_id]["start"] = infomation.child("m_startPreviewFilePath").text().as_string();

                for (const auto &param : infomation.child("m_title").child("m_param"))
                    if ((strncmp(param.name(), user_lang.c_str(), user_lang.size()) == 0))
                        themes_info[content_id].title = param.text().as_string();

                if (themes_info[content_id].title.empty())
                    themes_info[content_id].title = infomation.child("m_title").child("m_default").text().as_string();

                size_t theme_size = 0;
                for (const auto &theme : fs::recursive_directory_iterator(theme_path / content_id))
                    if (fs::is_regular_file(theme.path()))
                        theme_size += fs::file_size(theme.path());

                themes_info[content_id].provided = infomation.child("m_provider").child("m_default").text().as_string();

                const auto updated = fs::last_write_time(theme_path / content_id);
                const auto updated_date = std::localtime(&updated);

                themes_info[content_id].updated = fmt::format("{}/{}/{}  {:0>2d}:{:0>2d}", updated_date->tm_mday, updated_date->tm_mon + 1, updated_date->tm_year + 1900, updated_date->tm_hour, updated_date->tm_min);

                themes_info[content_id].size = theme_size / KB(1);
                themes_info[content_id].version = infomation.child("m_contentVer").text().as_string();

                themes_list.push_back({ content_id, updated });
            } else
                LOG_ERROR("theme not found for title: {}", content_id);
        }
    }

    std::sort(themes_list.begin(), themes_list.end(), [&](const auto &ta, const auto &tb) {
        return ta.second > tb.second;
    });

    if (fs::exists(fw_theme_path) && !fs::is_empty(fw_theme_path)) {
        theme_preview_name["default"]["package"] = "data/internal/theme/theme_defaultImage.png";

        theme_preview_name["default"]["home"] = "data/internal/theme/defaultTheme_homeScreen.png";
        theme_preview_name["default"]["start"] = "data/internal/theme/defaultTheme_startScreen.png";

        themes_info["default"].title = "Default";
        themes_list.push_back({ "default", {} });
    } else
        LOG_WARN("Default theme not found, install firmware fix this!");

    for (const auto &theme : themes_info) {
        for (const auto &name : theme_preview_name[theme.first]) {
            if (!name.second.empty()) {
                int32_t width = 0;
                int32_t height = 0;
                vfs::FileBuffer buffer;

                if (theme.first == "default")
                    vfs::read_file(VitaIoDevice::vs0, buffer, host.pref_path, name.second);
                else
                    vfs::read_file(VitaIoDevice::ux0, buffer, host.pref_path, "theme/" + theme.first + "/" + name.second);

                if (buffer.empty()) {
                    LOG_WARN("Background, Name: '{}', Not found for title: {} [{}].", name.second, theme.first, theme.second.title);
                    continue;
                }
                stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                if (!data) {
                    LOG_ERROR("Invalid Background for title: {} [{}].", theme.first, theme.second.title);
                    continue;
                }

                gui.themes_preview[theme.first][name.first].init(gui.imgui_state.get(), data, width, height);
                stbi_image_free(data);
            }
        }
    }
}

static const char *const wmonth[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static const char *const wday[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};

void draw_start_screen(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto SCAL = ImVec2(display_size.x / 960.0f, display_size.y / 544.0f);
    const auto MENUBAR_HEIGHT = 32.f * SCAL.y;
    auto DATE_POS = ImVec2(display_size.x - (date["date"].x * SCAL.x), display_size.y - (date["date"].y * SCAL.y));
    auto CLOCK_POS = ImVec2(display_size.x - (date["clock"].x * SCAL.x), display_size.y - (date["clock"].y * SCAL.y));

    ImGui::SetNextWindowPos(ImVec2(0.f, MENUBAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
    ImGui::Begin("##start_screen", &gui.live_area.start_screen, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    if (gui.start_background)
        ImGui::GetForegroundDrawList()->AddImage(gui.start_background, ImVec2(0.f, MENUBAR_HEIGHT), display_size);
    else
        ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(0.f, MENUBAR_HEIGHT), display_size, IM_COL32(128.f, 128.f, 128.f, 128.f), 0.f, ImDrawCornerFlags_All);

    ImGui::GetForegroundDrawList()->AddRect(ImVec2(32.f * SCAL.x, 64.f * SCAL.y), ImVec2(display_size.x - (32.f * SCAL.x), display_size.y - (32.f * SCAL.y)), IM_COL32(255.f, 255.f, 255.f, 255.f), 20.0f, ImDrawCornerFlags_All);

    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    const auto local = *localtime(&tt);

    const auto scal_default_font = 19.2f / ImGui::GetFontSize();
    const auto scal_date_font_size = 34.f * scal_default_font;
    const auto scal_data_font = scal_date_font_size / ImGui::GetFontSize();

    const auto scal_clock_font_size = 124.f * scal_default_font;
    const auto scal_clock_font = scal_clock_font_size / ImGui::GetFontSize();

    const auto DATE_STR = fmt::format("{} {} ({})", local.tm_mday, wmonth[local.tm_mon], wday[local.tm_wday]);
    const auto CLOCK_STR = fmt::format("{:0>2d}:{:0>2d}", local.tm_hour, local.tm_min);

    if (start_param["dateLayout"] == "2") {
        const auto DATE_SIZE = ImGui::CalcTextSize(DATE_STR.c_str()).x * scal_data_font;
        const auto CLOCK_SIZE = ImGui::CalcTextSize(CLOCK_STR.c_str()).x * scal_clock_font;
        DATE_POS.x -= DATE_SIZE * SCAL.x;
        CLOCK_POS.x -= CLOCK_SIZE * SCAL.x;
    }

    ImGui::GetForegroundDrawList()->AddText(gui.live_area_font, scal_date_font_size * SCAL.x, DATE_POS, date_color, DATE_STR.c_str());
    ImGui::PushFont(gui.live_area_font_large);
    ImGui::GetForegroundDrawList()->AddText(gui.live_area_font_large, scal_clock_font_size * SCAL.x, CLOCK_POS, date_color, CLOCK_STR.c_str());
    ImGui::PopFont();

    if (ImGui::IsMouseClicked(0) || ImGui::IsKeyPressed(host.cfg->keyboard_button_circle)) {
        gui.live_area.start_screen = false;
        gui.live_area.app_selector = true;
    }

    ImGui::End();
}

static std::string popup, menu, selected, title, start, delete_user_background, delete_theme;
static ImGuiTextFilter search_bar;
static float scroll_pos;
static bool set_scroll_pos;

void draw_themes_selection(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto SCAL = ImVec2(display_size.x / 960.0f, display_size.y / 544.0f);

    const auto BUTTON_SIZE = ImVec2(310.f * SCAL.x, 46.f * SCAL.y);
    const auto ICON_SIZE = ImVec2(100.f * SCAL.x, 100.f * SCAL.y);
    const auto MENUBAR_HEIGHT = 32.f * SCAL.y;
    const auto WINDOW_SIZE = ImVec2(display_size.x, display_size.y - MENUBAR_HEIGHT);
    const auto SIZE_PREVIEW = ImVec2(360.f * SCAL.x, 204.f * SCAL.y);
    const auto SIZE_MINI_PREVIEW = ImVec2(256.f * SCAL.x, 145.f * SCAL.y);
    const auto SIZE_LIST = ImVec2(780 * SCAL.x, 414.f * SCAL.y);
    const auto SIZE_PACKAGE = ImVec2(226.f * SCAL.x, 128.f * SCAL.y);
    const auto SIZE_MINI_PACKAGE = ImVec2(170.f * SCAL.x, 96.f * SCAL.y);
    const auto POPUP_SIZE = ImVec2(756.0f * SCAL.x, 436.0f * SCAL.y);

    const auto is_background = gui.apps_background.find("NPXS10015") != gui.apps_background.end();

    ImGui::SetNextWindowPos(ImVec2(0, MENUBAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(is_background ? 0.f : 0.999f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::Begin("##themes", &gui.live_area.theme_background, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    if (is_background)
        ImGui::GetBackgroundDrawList()->AddImage(gui.apps_background["NPXS10015"], ImVec2(0.f, MENUBAR_HEIGHT), display_size);
    ImGui::SetWindowFontScale(1.6f * SCAL.x);
    const auto theme_str = ImGui::CalcTextSize(title.c_str(), 0, false, SIZE_LIST.x);
    ImGui::PushTextWrapPos(((display_size.x - SIZE_LIST.x) / 2.f) + SIZE_LIST.x);
    ImGui::SetCursorPos(ImVec2((display_size.x / 2.f) - (theme_str.x / 2.f), (35.f * SCAL.y) - (theme_str.y / 2.f)));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", title.c_str());
    ImGui::PopTextWrapPos();

    // Search Bar
    if ((menu == "theme") && selected.empty()) {
        ImGui::SetWindowFontScale(1.2f * SCAL.x);
        const auto search_size = ImGui::CalcTextSize("Search");
        ImGui::SetCursorPos(ImVec2(display_size.x - 220.f - search_size.x, (35.f * SCAL.y) - (search_size.y / 2.f)));
        ImGui::TextColored(GUI_COLOR_TEXT, "Search");
        ImGui::SameLine();
        search_bar.Draw("##search_bar", 200);
        ImGui::SetWindowFontScale(1.6f * SCAL.x);
    }

    ImGui::SetCursorPosY(64.0f * SCAL.y);
    ImGui::Separator();
    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, (!menu.empty() ? 118.f : 96.0f) * SCAL.y), ImGuiCond_Always, ImVec2(0.5f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
    ImGui::BeginChild("##themes_child", SIZE_LIST, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    // Themes & Backgrounds
    if (menu.empty()) {
        title = "Theme & Background";
        ImGui::SetWindowFontScale(1.2f);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
        const auto SIZE_SELECT = 80.f * SCAL.y;
        if (!themes_info.empty()) {
            if (ImGui::Selectable("Theme", false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT)))
                menu = "theme";
            if (!host.cfg->theme_content_id.empty()) {
                ImGui::SetWindowFontScale(0.74f);
                const auto CALC_TITLE = ImGui::CalcTextSize(themes_info[host.cfg->theme_content_id].title.c_str(), 0, false, 260.f * SCAL.x).y / 2.f;
                ImGui::SameLine(0, 420.f * SCAL.x);
                const auto CALC_POS_TITLE = (SIZE_SELECT / 2.f) - CALC_TITLE;
                ImGui::SetCursorPosY(CALC_POS_TITLE);
                ImGui::PushTextWrapPos(SIZE_LIST.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[host.cfg->theme_content_id].title.c_str());
                ImGui::PopTextWrapPos();
                ImGui::SetWindowFontScale(1.2f);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (CALC_POS_TITLE > 0 ? CALC_POS_TITLE : -CALC_POS_TITLE));
            }
            ImGui::Separator();
        }
        if (ImGui::Selectable("Start Screen", false, ImGuiSelectableFlags_None, ImVec2(0.f, 80.f * SCAL.y)))
            menu = "start";
        ImGui::Separator();
        if (ImGui::Selectable("Home Screen User Backgrounds", false, ImGuiSelectableFlags_None, ImVec2(0.f, 80.f * SCAL.y)))
            menu = "background";
        ImGui::PopStyleVar();
        ImGui::Separator();
    } else {
        if (menu == "theme") {
            // Theme List
            if (selected.empty()) {
                title = "Theme";

                // Delete Theme
                if (!delete_theme.empty()) {
                    gui.themes_preview.erase(delete_theme);
                    themes_info.erase(delete_theme);
                    const auto theme_list_index = std::find_if(themes_list.begin(), themes_list.end(), [&](const auto &t) {
                        return t.first == delete_theme;
                    });
                    themes_list.erase(theme_list_index);
                    delete_theme.clear();
                }

                // Set Scroll Pos
                if (set_scroll_pos) {
                    ImGui::SetScrollY(scroll_pos);
                    set_scroll_pos = false;
                }

                ImGui::Columns(3, nullptr, false);
                for (const auto &theme : themes_list) {
                    if (!search_bar.PassFilter(themes_info[theme.first].title.c_str()) && !search_bar.PassFilter(theme.first.c_str()))
                        continue;
                    const auto POS_IMAGE = ImGui::GetCursorPosY();
                    if (gui.themes_preview[theme.first].find("package") != gui.themes_preview[theme.first].end())
                        ImGui::Image(gui.themes_preview[theme.first]["package"], SIZE_PACKAGE);
                    const auto POS_TITLE = ImGui::GetCursorPosY();
                    ImGui::SetCursorPosY(POS_IMAGE);
                    ImGui::PushID(theme.first.c_str());
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                    ImGui::SetWindowFontScale(1.8f);
                    if (ImGui::Selectable(host.cfg->theme_content_id == theme.first ? "V" : "##preview", false, ImGuiSelectableFlags_None, SIZE_PACKAGE)) {
                        selected = theme.first;
                        scroll_pos = ImGui::GetScrollY();
                    }
                    ImGui::SetWindowFontScale(0.6f);
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar();
                    ImGui::PopID();
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + SIZE_PACKAGE.x);
                    ImGui::SetCursorPosY(POS_TITLE);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[theme.first].title.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);
            } else {
                // Theme Select
                title = themes_info[selected].title;
                if (popup.empty()) {
                    if (gui.themes_preview[selected].find("home") != gui.themes_preview[selected].end()) {
                        ImGui::SetCursorPos(ImVec2(15.f * SCAL.x, (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (72.f * SCAL.y)));
                        ImGui::Image(gui.themes_preview[selected]["home"], SIZE_PREVIEW);
                    }
                    if (gui.themes_preview[selected].find("start") != gui.themes_preview[selected].end()) {
                        ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) + (15.f * SCAL.y), (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (72.f * SCAL.y)));
                        ImGui::Image(gui.themes_preview[selected]["start"], SIZE_PREVIEW);
                    }
                    ImGui::SetWindowFontScale(1.2f);
                    ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) - (BUTTON_SIZE.x / 2.f), (SIZE_LIST.y - 82.f) - BUTTON_SIZE.y));
                    if ((selected != host.cfg->theme_content_id) && (ImGui::Button("Select", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg->keyboard_button_cross))) {
                        host.cfg->user_start_background.clear();
                        if (init_theme(gui, host, selected)) {
                            host.cfg->theme_content_id = selected;
                            host.cfg->use_theme_background = true;
                        } else
                            host.cfg->use_theme_background = false;
                        init_theme_start_background(gui, host, selected);
                        host.cfg->start_background = (selected == "default") ? "default" : "theme";
                        config::serialize_config(*host.cfg, host.cfg->config_path);
                        set_scroll_pos = true;
                        selected.clear();
                    }
                } else if (popup == "delete") {
                    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
                    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
                    ImGui::Begin("##delete_theme", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                    ImGui::SetNextWindowBgAlpha(0.999f);
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f);
                    ImGui::BeginChild("##delete_theme_popup", POPUP_SIZE, true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                    ImGui::SetCursorPos(ImVec2(48.f * SCAL.x, 28.f * SCAL.y));
                    ImGui::SetWindowFontScale(1.6f * SCAL.x);
                    ImGui::Image(gui.themes_preview[selected]["package"], SIZE_MINI_PACKAGE);
                    ImGui::SameLine();
                    const auto CALC_TITLE = ImGui::CalcTextSize(themes_info[selected].title.c_str(), nullptr, false, POPUP_SIZE.x - SIZE_MINI_PACKAGE.x - 48.f).y / 2.f;
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (SIZE_MINI_PACKAGE.y / 2.f) - CALC_TITLE);
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + POPUP_SIZE.x - SIZE_MINI_PACKAGE.x - 48.f);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[selected].title.c_str());
                    ImGui::PopTextWrapPos();
                    const auto CALC_TEXT = ImGui::CalcTextSize("This theme will be deleted.");
                    ImGui::SetCursorPos(ImVec2(POPUP_SIZE.x / 2 - (CALC_TEXT.x / 2.f), POPUP_SIZE.y / 2.f - (CALC_TEXT.y / 2.f)));
                    ImGui::TextColored(GUI_COLOR_TEXT, "This theme will be deleted.");
                    ImGui::SetCursorPos(ImVec2(50.f, POPUP_SIZE.y - (22.f + BUTTON_SIZE.y)));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
                    if (ImGui::Button("Cancel", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg->keyboard_button_circle))
                        popup.clear();
                    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - 50.f - BUTTON_SIZE.x, POPUP_SIZE.y - (22.f + BUTTON_SIZE.y)));
                    if (ImGui::Button("Ok", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg->keyboard_button_cross)) {
                        if (selected == host.cfg->theme_content_id) {
                            host.cfg->user_start_background.clear();
                            host.cfg->theme_content_id = "default";
                            init_apps_icon(gui, host, gui.app_selector.sys_apps);
                            if (init_theme(gui, host, "default"))
                                host.cfg->use_theme_background = true;
                            else
                                host.cfg->use_theme_background = false;
                            init_theme_start_background(gui, host, "default");
                            config::serialize_config(*host.cfg, host.cfg->config_path);
                        }
                        fs::remove_all(fs::path{ host.pref_path } / "ux0/theme" / selected);
                        if (host.app_title_id == "NPXS10026")
                            get_contents_size(gui, host);
                        delete_theme = selected;
                        popup.clear();
                        selected.clear();
                        set_scroll_pos = true;
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar(2);
                    ImGui::End();
                } else if (popup == "information") {
                    if (gui.themes_preview[selected].find("home") != gui.themes_preview[selected].end()) {
                        ImGui::SetCursorPos(ImVec2(119.f * SCAL.x, 4.f * SCAL.y));
                        ImGui::Image(gui.themes_preview[selected]["home"], SIZE_MINI_PREVIEW);
                    }
                    if (gui.themes_preview[selected].find("start") != gui.themes_preview[selected].end()) {
                        ImGui::SetCursorPos(ImVec2(SIZE_LIST.x / 2.f + (15.f * SCAL.y), 4.f * SCAL.y));
                        ImGui::Image(gui.themes_preview[selected]["start"], SIZE_MINI_PREVIEW);
                    }
                    const auto INFO_POS = ImVec2(280.f * SCAL.x, 30.f * SCAL.y);
                    ImGui::SetWindowFontScale(0.94f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                    ImGui::TextColored(GUI_COLOR_TEXT, "Name");
                    ImGui::SameLine();
                    ImGui::PushTextWrapPos(SIZE_LIST.x - (30.f * SCAL.x));
                    ImGui::SetCursorPosX(INFO_POS.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[selected].title.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                    ImGui::TextColored(GUI_COLOR_TEXT, "Provider");
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(INFO_POS.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[selected].provided.c_str());
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                    ImGui::TextColored(GUI_COLOR_TEXT, "Updated");
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(INFO_POS.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[selected].updated.c_str());
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                    ImGui::TextColored(GUI_COLOR_TEXT, "Size");
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(INFO_POS.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%zu KB", themes_info[selected].size);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                    ImGui::TextColored(GUI_COLOR_TEXT, "Version");
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(INFO_POS.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[selected].version.c_str());
                }
            }
        } else if (menu == "start") {
            if (start.empty()) {
                title = "Start Screen";
                ImGui::SetWindowFontScale(0.72f);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
                const auto PACKAGE_POS_Y = (SIZE_LIST.y / 2.f) - (SIZE_PACKAGE.y / 2.f) - (72.f * SCAL.y);
                const auto is_not_default = !host.cfg->theme_content_id.empty() && (host.cfg->theme_content_id != "default");
                if (is_not_default) {
                    const auto THEME_POS = ImVec2(15.f * SCAL.x, PACKAGE_POS_Y);
                    if (gui.themes_preview[host.cfg->theme_content_id].find("package") != gui.themes_preview[host.cfg->theme_content_id].end()) {
                        ImGui::SetCursorPos(THEME_POS);
                        ImGui::Image(gui.themes_preview[host.cfg->theme_content_id]["package"], SIZE_PACKAGE);
                    }
                    ImGui::SetCursorPos(THEME_POS);
                    ImGui::SetWindowFontScale(1.8f);
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                    if (ImGui::Selectable(host.cfg->start_background == "theme" ? "V" : "##theme", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                        start = "theme";
                    ImGui::PopStyleColor();
                    ImGui::SetWindowFontScale(0.72f);
                    ImGui::SetCursorPosX(15.f * SCAL.x);
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + SIZE_PACKAGE.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[host.cfg->theme_content_id].title.c_str());
                    ImGui::PopTextWrapPos();
                }
                const auto IMAGE_POS = ImVec2(is_not_default ? (SIZE_LIST.x / 2.f) - (SIZE_PACKAGE.x / 2.f) : 15.f * SCAL.x, PACKAGE_POS_Y);
                if ((host.cfg->start_background == "image") && gui.start_background) {
                    ImGui::SetCursorPos(IMAGE_POS);
                    ImGui::Image(gui.start_background, SIZE_PACKAGE);
                }
                ImGui::SetCursorPos(IMAGE_POS);
                if (host.cfg->start_background == "image")
                    ImGui::SetWindowFontScale(1.8f);
                ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                if (ImGui::Selectable(host.cfg->start_background == "image" ? "V" : "Add Image", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                    start = "image";
                ImGui::PopStyleColor();
                ImGui::SetWindowFontScale(0.72f);
                ImGui::SetCursorPosX(IMAGE_POS.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "Image");
                const auto DEFAULT_POS = ImVec2(is_not_default ? (SIZE_LIST.x / 2.f) + (SIZE_PACKAGE.x / 2.f) + (30.f * SCAL.y) : (SIZE_LIST.x / 2.f) - (SIZE_PACKAGE.x / 2.f), PACKAGE_POS_Y);
                if (gui.themes_preview["default"].find("package") != gui.themes_preview["default"].end()) {
                    ImGui::SetCursorPos(DEFAULT_POS);
                    ImGui::Image(gui.themes_preview["default"]["package"], SIZE_PACKAGE);
                    ImGui::SetCursorPos(DEFAULT_POS);
                    ImGui::SetWindowFontScale(1.8f);
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                    if (ImGui::Selectable(host.cfg->start_background == "default" ? "V" : "##default", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                        start = "default";
                    ImGui::PopStyleColor();
                    ImGui::SetWindowFontScale(0.72f);
                    ImGui::SetCursorPosX(DEFAULT_POS.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "Default");
                }
                ImGui::PopStyleVar();
                ImGui::SetWindowFontScale(0.90f);
            } else {
                const auto START_PREVIEW_POS = ImVec2((SIZE_LIST.x / 2.f) - (SIZE_PREVIEW.x / 2.f), (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (72.f * SCAL.y));
                const auto SELECT_BUTTON_POS = ImVec2((SIZE_LIST.x / 2.f) - (BUTTON_SIZE.x / 2.f), (SIZE_LIST.y - 82.f) - BUTTON_SIZE.y);
                if (start == "theme") {
                    title = themes_info[host.cfg->theme_content_id].title;
                    if (gui.themes_preview[host.cfg->theme_content_id].find("start") != gui.themes_preview[host.cfg->theme_content_id].end()) {
                        ImGui::SetCursorPos(START_PREVIEW_POS);
                        ImGui::Image(gui.themes_preview[host.cfg->theme_content_id]["start"], SIZE_PREVIEW);
                    }
                    ImGui::SetCursorPos(SELECT_BUTTON_POS);
                    if ((host.cfg->start_background != "theme") && (ImGui::Button("Select", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg->keyboard_button_cross))) {
                        host.cfg->user_start_background.clear();
                        host.cfg->start_background = "theme";
                        init_theme_start_background(gui, host, host.cfg->theme_content_id);
                        config::serialize_config(*host.cfg, host.cfg->config_path);
                        start.clear();
                    }
                } else if (start == "image") {
                    nfdchar_t *image_path;
                    nfdresult_t result = NFD_OpenDialog("bmp,gif,jpg,png,tif", nullptr, &image_path);

                    if ((result == NFD_OKAY) && init_user_start_background(gui, image_path)) {
                        host.cfg->user_start_background = image_path;
                        host.cfg->start_background = "image";
                        config::serialize_config(*host.cfg, host.cfg->config_path);
                    }
                    start.clear();
                } else if (start == "default") {
                    title = "Default";
                    if (gui.themes_preview["default"].find("start") != gui.themes_preview["default"].end()) {
                        ImGui::SetCursorPos(START_PREVIEW_POS);
                        ImGui::Image(gui.themes_preview["default"]["start"], SIZE_PREVIEW);
                    }
                    ImGui::SetCursorPos(SELECT_BUTTON_POS);
                    if ((host.cfg->start_background != "default") && (ImGui::Button("Select", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg->keyboard_button_cross))) {
                        host.cfg->user_start_background.clear();
                        init_theme_start_background(gui, host, "default");
                        host.cfg->start_background = "default";
                        config::serialize_config(*host.cfg, host.cfg->config_path);
                        start.clear();
                    }
                }
            }
        } else if (menu == "background") {
            title = "Background";

            // Delete user background
            if (!delete_user_background.empty()) {
                const auto bg = std::find(host.cfg->user_backgrounds.begin(), host.cfg->user_backgrounds.end(), delete_user_background);
                host.cfg->user_backgrounds.erase(bg);
                gui.user_backgrounds.erase(delete_user_background);
                if (host.cfg->user_backgrounds.size())
                    gui.current_user_bg = 0;
                else if (!gui.theme_backgrounds.empty())
                    host.cfg->use_theme_background = true;
                config::serialize_config(*host.cfg, host.cfg->config_path);
                delete_user_background.clear();
            }

            ImGui::SetWindowFontScale(0.90f);
            ImGui::Columns(3, nullptr, false);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 1.0f));
            for (const auto &background : host.cfg->user_backgrounds) {
                const auto IMAGE_POS = ImGui::GetCursorPosY();
                ImGui::Image(gui.user_backgrounds[background], SIZE_PACKAGE);
                ImGui::SetCursorPosY(IMAGE_POS);
                ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                ImGui::PushID(background.c_str());
                if (ImGui::Selectable("Delete Image", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                    delete_user_background = background;
                ImGui::PopID();
                ImGui::PopStyleColor();
                ImGui::NextColumn();
            }
            if (ImGui::Selectable("Add Image", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                add_user_image_background(gui, host);
            ImGui::PopStyleVar();
            ImGui::Columns(1);
        }
    }
    ImGui::EndChild();

    ImGui::SetWindowFontScale(1.00f);
    ImGui::SetCursorPos(ImVec2(6.f, display_size.y - (84.f * SCAL.y)));
    if (ImGui::Button("Back", ImVec2(64.f * SCAL.x, 40.f * SCAL.y))) {
        if (!menu.empty()) {
            if (!selected.empty()) {
                if (!popup.empty())
                    popup.clear();
                else {
                    selected.clear();
                    set_scroll_pos = true;
                }
            } else if (!start.empty())
                start.clear();
            else
                menu.clear();
        } else {
            if (host.app_title_id == "NPXS10026") {
                gui.live_area.content_manager = true;
            } else {
                if (!gui.apps_list_opened.empty() && gui.apps_list_opened[gui.current_app_selected] == "NPXS10015")
                    gui.live_area.live_area_screen = true;
                else
                    gui.live_area.app_selector = true;
            }
            gui.live_area.theme_background = false;
        }
    }

    if (!selected.empty() && (selected != "default")) {
        ImGui::SetCursorPos(ImVec2(display_size.x - (70.f * SCAL.x), display_size.y - (84.f * SCAL.y)));
        if ((popup != "information") && ImGui::Button("...", ImVec2(64.f * SCAL.x, 40.f * SCAL.y)) || ImGui::IsKeyPressed(host.cfg->keyboard_button_triangle))
            ImGui::OpenPopup("...");
        if (ImGui::BeginPopup("...")) {
            if (ImGui::MenuItem("Information"))
                popup = "information";
            if (ImGui::MenuItem("Delete"))
                popup = "delete";
            ImGui::EndPopup();
        }
    }
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace gui
