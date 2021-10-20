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

#include <config/functions.h>
#include <gui/functions.h>
#include <ime/functions.h>
#include <io/device.h>
#include <util/log.h>
#include <util/safe_time.h>
#include <util/string_utils.h>

#include <nfd.h>
#include <pugixml.hpp>
#include <stb_image.h>

namespace gui {

bool init_user_background(GuiState &gui, HostState &host, const std::string &user_id, const std::string &background_path) {
    gui.user_backgrounds[background_path] = {};
    const std::wstring background_path_wstr = string_utils::utf_to_wide(background_path);

    if (!fs::exists(fs::path(background_path_wstr))) {
        LOG_WARN("Background doesn't exist: {}.", background_path);
        return false;
    }

    int32_t width = 0;
    int32_t height = 0;

#ifdef _WIN32
    FILE *f = _wfopen(background_path_wstr.c_str(), L"rb");
#else
    FILE *f = fopen(background_path.c_str(), "rb");
#endif

    stbi_uc *data = stbi_load_from_file(f, &width, &height, nullptr, STBI_rgb_alpha);

    if (!data) {
        LOG_ERROR("Invalid or corrupted background: {}.", background_path);
        return false;
    }

    gui.user_backgrounds[background_path].init(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);
    fclose(f);

    gui.current_user_bg = 0;

    return gui.user_backgrounds.find(background_path) != gui.user_backgrounds.end();
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

    date["date"] = ImVec2(900.f, 186.f);
    date["clock"] = ImVec2(880.f, 146.f);
    if (!start_param["dateLayout"].empty()) {
        switch (std::stoi(start_param["dateLayout"])) {
        case 0:
            break;
        case 1:
            date["date"].y = 468.f;
            date["clock"].y = 426.f;
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

    date["date"] = ImVec2(900.f, 186.f);
    date["clock"] = ImVec2(880.f, 146.f);

    date_color = 4294967295; // White

    return gui.start_background;
}

bool init_theme(GuiState &gui, HostState &host, const std::string &content_id) {
    std::vector<std::string> theme_bg_name;
    std::map<std::string, std::string> bar_param;
    std::map<std::string, std::string> theme_icon_name = {
        { "NPXS10008", {} },
        { "NPXS10015", {} },
        { "NPXS10026", {} }
    };

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

    for (const auto &icon : theme_icon_name) {
        int32_t width = 0;
        int32_t height = 0;
        vfs::FileBuffer buffer;

        const auto title_id = icon.first;
        const auto name = icon.second;
        if (name.empty())
            vfs::read_file(VitaIoDevice::vs0, buffer, host.pref_path, "app/" + title_id + "/sce_sys/icon0.png");
        else
            vfs::read_file(VitaIoDevice::ux0, buffer, host.pref_path, "theme/" + content_id + "/" + name);

        if (buffer.empty()) {
            buffer = init_default_icon(gui, host, title_id);
            if (buffer.empty()) {
                LOG_WARN("Name: '{}', Not found icon for system App: {}.", name, content_id);
                continue;
            } else
                LOG_INFO("Default icon found for system App {}.", title_id);
        }
        stbi_uc *data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
        if (!data) {
            LOG_ERROR("Name: '{}', Invalid icon for content id: {}.", name, content_id);
            continue;
        }

        gui.app_selector.sys_apps_icon[title_id].init(gui.imgui_state.get(), data, width, height);
        stbi_image_free(data);
    }

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
    tm updated;
    size_t size;
    std::string version;
};

static std::map<std::string, Theme> themes_info;
static std::vector<std::pair<std::string, time_t>> themes_list;

void init_themes(GuiState &gui, HostState &host) {
    gui.themes_preview.clear(), themes_info.clear(), themes_list.clear();

    const auto theme_path{ fs::path(host.pref_path) / "ux0/theme" };
    const auto fw_theme_path{ fs::path(host.pref_path) / "vs0/data/internal/theme" };
    if ((!fs::exists(fw_theme_path) || fs::is_empty(fw_theme_path)) && (!fs::exists(theme_path) || fs::is_empty(theme_path))) {
        LOG_WARN("Theme path is empty");
        return;
    }

    std::string user_lang;
    const auto sys_lang = static_cast<SceSystemParamLang>(host.cfg.sys_lang);
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

                // Title
                const auto title = infomation.child("m_title");
                if (!title.child("m_param").child(user_lang.c_str()).text().empty())
                    themes_info[content_id].title = title.child("m_param").child(user_lang.c_str()).text().as_string();
                else
                    themes_info[content_id].title = title.child("m_default").text().as_string();

                // Provider
                const auto provider = infomation.child("m_provider");
                if (!provider.child("m_param").child(user_lang.c_str()).text().empty())
                    themes_info[content_id].provided = provider.child("m_param").child(user_lang.c_str()).text().as_string();
                else
                    themes_info[content_id].provided = provider.child("m_default").text().as_string();

                size_t theme_size = 0;
                for (const auto &theme : fs::recursive_directory_iterator(theme_path / content_id))
                    if (fs::is_regular_file(theme.path()))
                        theme_size += fs::file_size(theme.path());

                const auto updated = fs::last_write_time(theme_path / content_id);
                SAFE_LOCALTIME(&updated, &themes_info[content_id].updated);

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

        themes_info["default"].title = !gui.lang.settings.empty() ? gui.lang.settings["default"] : "Default";
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

void draw_start_screen(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);
    const auto INFORMATION_BAR_HEIGHT = 32.f * SCALE.y;

    ImGui::SetNextWindowPos(ImVec2(0.f, INFORMATION_BAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::Begin("##start_screen", &gui.live_area.start_screen, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    if (gui.start_background)
        ImGui::GetBackgroundDrawList()->AddImage(gui.start_background, ImVec2(0.f, INFORMATION_BAR_HEIGHT), display_size);
    else
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, INFORMATION_BAR_HEIGHT), display_size, IM_COL32(43.f, 44.f, 47.f, 255.f), 0.f, ImDrawCornerFlags_All);

    ImGui::GetForegroundDrawList()->AddRect(ImVec2(32.f * SCALE.x, 64.f * SCALE.y), ImVec2(display_size.x - (32.f * SCALE.x), display_size.y - (32.f * SCALE.y)), IM_COL32(255.f, 255.f, 255.f, 255.f), 20.0f * SCALE.x, ImDrawCornerFlags_All);

    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    tm local = {};

    SAFE_LOCALTIME(&tt, &local);

    ImGui::PushFont(gui.vita_font);
    const auto DEFAULT_FONT_SCALE = ImGui::GetFontSize() / (19.2f * host.dpi_scale);
    const auto SCAL_PIX_DATE_FONT = 34.f / 28.f;
    const auto DATE_FONT_SIZE = (34.f * host.dpi_scale) * DEFAULT_FONT_SCALE;
    const auto SCAL_DATE_FONT_SIZE = DATE_FONT_SIZE / ImGui::GetFontSize();

    auto DATE_TIME = get_date_time(gui, host, const_cast<tm &>(local));
    const auto DATE_STR = DATE_TIME["date-start"];
    const auto CALC_DATE_SIZE = ImGui::CalcTextSize(DATE_STR.c_str());
    const auto DATE_SIZE = ImVec2(CALC_DATE_SIZE.x * SCAL_DATE_FONT_SIZE, CALC_DATE_SIZE.y * SCAL_DATE_FONT_SIZE * SCAL_PIX_DATE_FONT);
    const auto DATE_POS = ImVec2(display_size.x - (start_param["dateLayout"] == "2" ? date["date"].x + DATE_SIZE.x : date["date"].x) * SCALE.x, display_size.y - (date["date"].y * SCALE.y));
    ImGui::GetForegroundDrawList()->AddText(gui.vita_font, DATE_FONT_SIZE * RES_SCALE.x, DATE_POS, date_color, DATE_STR.c_str());
    ImGui::PopFont();

    ImGui::PushFont(gui.large_font);
    const auto DEFAULT_LARGE_FONT_SCALE = ImGui::GetFontSize() / (116.f * host.dpi_scale);
    const auto LARGE_FONT_SIZE = (116.f * host.dpi_scale) * DEFAULT_FONT_SCALE;
    const auto PIX_LARGE_FONT_SCALE = (96.f * host.dpi_scale) / ImGui::GetFontSize();

    const auto CLOCK_STR = DATE_TIME["clock"];
    const auto CALC_CLOCK_SIZE = ImGui::CalcTextSize(CLOCK_STR.c_str());
    const auto CLOCK_SIZE = ImVec2(CALC_CLOCK_SIZE.x, CALC_CLOCK_SIZE.y * PIX_LARGE_FONT_SCALE);

    const auto DAY_MOMENT_STR = DATE_TIME["day-moment"];
    const auto CALC_DAY_MOMENT_SIZE = ImGui::CalcTextSize(DAY_MOMENT_STR.c_str());
    const auto DAY_MOMENT_LARGE_FONT_SIZE = (56.f * host.dpi_scale) * DEFAULT_LARGE_FONT_SCALE;
    const auto LARGE_FONT_DAY_MOMENT_SCALE = DAY_MOMENT_LARGE_FONT_SIZE / ImGui::GetFontSize();
    const auto DAY_MOMENT_SIZE = gui.users[host.io.user_id].clock_12_hour ? ImVec2(CALC_DAY_MOMENT_SIZE.x * LARGE_FONT_DAY_MOMENT_SCALE, CALC_DAY_MOMENT_SIZE.y * LARGE_FONT_DAY_MOMENT_SCALE * PIX_LARGE_FONT_SCALE) : ImVec2(0.f, 0.f);

    auto CLOCK_POS = ImVec2(display_size.x - (date["clock"].x * SCALE.x), display_size.y - (date["clock"].y * SCALE.y));
    if (start_param["dateLayout"] == "2")
        CLOCK_POS.x -= (CLOCK_SIZE.x * SCALE.x) + (DAY_MOMENT_SIZE.x * SCALE.x);
    else if (std::stoi(DATE_TIME["hour"]) < 10)
        CLOCK_POS.x += ImGui::CalcTextSize("0").x * RES_SCALE.x;

    ImGui::GetForegroundDrawList()->AddText(gui.large_font, LARGE_FONT_SIZE * RES_SCALE.x, CLOCK_POS, date_color, CLOCK_STR.c_str());
    if (gui.users[host.io.user_id].clock_12_hour) {
        const auto DAY_MOMENT_POS = ImVec2(CLOCK_POS.x + ((CLOCK_SIZE.x + (6.f * SCALE.x)) * RES_SCALE.x), CLOCK_POS.y + ((CLOCK_SIZE.y - DAY_MOMENT_SIZE.y) * RES_SCALE.y));
        ImGui::GetForegroundDrawList()->AddText(gui.large_font, DAY_MOMENT_LARGE_FONT_SIZE * RES_SCALE.x, DAY_MOMENT_POS, date_color, DAY_MOMENT_STR.c_str());
    }
    ImGui::PopFont();

    if (ImGui::IsMouseClicked(0) || ImGui::IsKeyPressed(host.cfg.keyboard_button_circle)) {
        gui.live_area.start_screen = false;
        gui.live_area.app_selector = true;
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

static std::string popup, settings_menu, menu, sub_menu, selected, title, delete_user_background, delete_theme;
static ImGuiTextFilter search_bar;

static std::string get_date_format_sting(GuiState &gui, DateFormat &date_format) {
    std::string date_format_str;
    const auto is_lang = !gui.lang.settings.empty();
    auto settings = gui.lang.settings;
    switch (date_format) {
    case YYYY_MM_DD: date_format_str = is_lang ? settings["yyyy_mm_dd"] : "YYYY/MM/DD"; break;
    case DD_MM_YYYY: date_format_str = is_lang ? settings["dd_mm_yyyy"] : "DD/MM/YYYY"; break;
    case MM_DD_YYYY: date_format_str = is_lang ? settings["mm_dd_yyyy"] : "MM/DD/YYYY"; break;
    default: break;
    }

    return date_format_str;
}

static const std::vector<std::string> LIST_SYS_LANG = {
    u8"日本語", "English (United States)", u8"Français", u8"Español", "Deutsch", "Italiano", "Nederlands", u8"Português (Portugal)",
    u8"Русский", "Korean", "Chinese - Traditional", "Chinese - Simplified", "Suomi", "Svenska",
    "Dansk", "Norsk", "Polskis", u8"Português (Brasil)", "English (United Kingdom)", u8"Türkçe"
};

static float set_scroll_pos, current_scroll_pos, max_scroll_pos;

void draw_settings(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto RES_SCALE = ImVec2(display_size.x / host.res_width_dpi_scale, display_size.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);

    const auto BUTTON_SIZE = ImVec2(310.f * SCALE.x, 46.f * SCALE.y);
    const auto ICON_SIZE = ImVec2(100.f * SCALE.x, 100.f * SCALE.y);
    const auto INFORMATION_BAR_HEIGHT = 32.f * SCALE.y;
    const auto WINDOW_SIZE = ImVec2(display_size.x, display_size.y - INFORMATION_BAR_HEIGHT);
    const auto SIZE_PREVIEW = ImVec2(360.f * SCALE.x, 204.f * SCALE.y);
    const auto SIZE_MINI_PREVIEW = ImVec2(256.f * SCALE.x, 145.f * SCALE.y);
    const auto SIZE_LIST = ImVec2(780 * SCALE.x, 414.f * SCALE.y);
    const auto SIZE_PACKAGE = ImVec2(226.f * SCALE.x, 128.f * SCALE.y);
    const auto SIZE_MINI_PACKAGE = ImVec2(170.f * SCALE.x, 96.f * SCALE.y);
    const auto POPUP_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);

    const auto is_background = gui.apps_background.find("NPXS10015") != gui.apps_background.end();
    auto lang = gui.lang.settings;
    const auto is_lang = !lang.empty();
    auto common = host.common_dialog.lang.common;

    ImGui::SetNextWindowPos(ImVec2(0, INFORMATION_BAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::Begin("##settings", &gui.live_area.settings, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    if (is_background)
        ImGui::GetBackgroundDrawList()->AddImage(gui.apps_background["NPXS10015"], ImVec2(0.f, 0.f), display_size);
    else
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, 0.f), display_size, IM_COL32(36.f, 120.f, 12.f, 255.f), 0.f, ImDrawCornerFlags_All);

    ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);
    const auto title_size_str = ImGui::CalcTextSize(title.c_str(), 0, false, SIZE_LIST.x);
    ImGui::PushTextWrapPos(((display_size.x - SIZE_LIST.x) / 2.f) + SIZE_LIST.x);
    ImGui::SetCursorPos(ImVec2((display_size.x / 2.f) - (title_size_str.x / 2.f), (35.f * SCALE.y) - (title_size_str.y / 2.f)));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", title.c_str());
    ImGui::PopTextWrapPos();

    if (settings_menu == "theme_background") {
        // Search Bar
        if ((menu == "theme") && selected.empty()) {
            ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
            const auto search_size = ImGui::CalcTextSize("Search");
            ImGui::SetCursorPos(ImVec2(display_size.x - (220.f * SCALE.x) - search_size.x, (35.f * SCALE.y) - (search_size.y / 2.f)));
            ImGui::TextColored(GUI_COLOR_TEXT, "Search");
            ImGui::SameLine();
            search_bar.Draw("##search_bar", 200 * SCALE.x);
            ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);

            // Draw Scroll Arrow
            const auto ARROW_SIZE = ImVec2(50.f * SCALE.x, 60.f * SCALE.y);
            const ImU32 ARROW_COLOR = 0xFFFFFFFF; // White
            if (current_scroll_pos) {
                const auto ARROW_UPP_CENTER = ImVec2(display_size.x - (45.f * SCALE.x), 135.f * SCALE.y);
                ImGui::GetWindowDrawList()->AddTriangleFilled(
                    ImVec2(ARROW_UPP_CENTER.x - (20.f * SCALE.x), ARROW_UPP_CENTER.y + (16.f * SCALE.y)),
                    ImVec2(ARROW_UPP_CENTER.x, ARROW_UPP_CENTER.y - (16.f * SCALE.y)),
                    ImVec2(ARROW_UPP_CENTER.x + (20.f * SCALE.x), ARROW_UPP_CENTER.y + (16.f * SCALE.y)), ARROW_COLOR);
                ImGui::SetCursorPos(ImVec2(ARROW_UPP_CENTER.x - (ARROW_SIZE.x / 2.f), ARROW_UPP_CENTER.y - ARROW_SIZE.y));
                if ((ImGui::Selectable("##upp", false, ImGuiSelectableFlags_None, ARROW_SIZE))
                    || ImGui::IsKeyPressed(host.cfg.keyboard_leftstick_up) || ImGui::IsKeyPressed(host.cfg.keyboard_button_up))
                    set_scroll_pos = current_scroll_pos - (340 * SCALE.y);
            }
            if (current_scroll_pos < max_scroll_pos) {
                const auto ARROW_DOWN_CENTER = ImVec2(display_size.x - (45.f * SCALE.x), display_size.y - (30.f * SCALE.y));
                ImGui::GetWindowDrawList()->AddTriangleFilled(
                    ImVec2(ARROW_DOWN_CENTER.x + (20.f * SCALE.x), ARROW_DOWN_CENTER.y - (16.f * SCALE.y)),
                    ImVec2(ARROW_DOWN_CENTER.x, ARROW_DOWN_CENTER.y + (16.f * SCALE.y)),
                    ImVec2(ARROW_DOWN_CENTER.x - (20.f * SCALE.x), ARROW_DOWN_CENTER.y - (16.f * SCALE.y)), ARROW_COLOR);
                ImGui::SetCursorPos(ImVec2(ARROW_DOWN_CENTER.x - (ARROW_SIZE.x / 2.f), ARROW_DOWN_CENTER.y - ARROW_SIZE.y));
                if ((ImGui::Selectable("##down", false, ImGuiSelectableFlags_None, ARROW_SIZE))
                    || ImGui::IsKeyPressed(host.cfg.keyboard_leftstick_down) || ImGui::IsKeyPressed(host.cfg.keyboard_button_down))
                    set_scroll_pos = current_scroll_pos + (340 * SCALE.y);
            }
        }
    }

    ImGui::SetCursorPosY(64.0f * SCALE.y);
    ImGui::Separator();
    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, (settings_menu == "theme_background" && !menu.empty() ? 118.f : 96.0f) * SCALE.y), ImGuiCond_Always, ImVec2(0.5f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
    ImGui::BeginChild("##settings_child", SIZE_LIST, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    const auto SIZE_SELECT = 80.f * SCALE.y;
    const auto SIZE_PUPUP_SELECT = 70.f * SCALE.y;

    // Settings
    if (settings_menu.empty()) {
        title = is_lang ? lang["settings"] : "Settings";
        ImGui::SetWindowFontScale(1.2f);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
        if (ImGui::Selectable(is_lang ? lang["theme_background"].c_str() : "Theme & Background", false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT))) {
            init_themes(gui, host);
            settings_menu = "theme_background";
        }
        ImGui::Separator();
        if (ImGui::Selectable(is_lang ? lang["date_time"].c_str() : "Date & Time", false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT)))
            settings_menu = "date&time";
        ImGui::Separator();
        if (ImGui::Selectable(is_lang ? lang["language"].c_str() : "Language", false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT)))
            settings_menu = "language";
        ImGui::PopStyleVar();
        ImGui::Separator();
    } else if (settings_menu == "theme_background") {
        // Themes & Backgrounds
        const auto select = !common["select"].empty() ? common["select"].c_str() : "Select";
        if (menu.empty()) {
            title = is_lang ? lang["theme_background"] : "Theme & Background";
            ImGui::SetWindowFontScale(1.2f);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
            if (!themes_info.empty()) {
                if (ImGui::Selectable(is_lang ? lang["theme"].c_str() : "Theme", false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT)))
                    menu = "theme";
                ImGui::SetWindowFontScale(0.74f);
                const auto CALC_TITLE = ImGui::CalcTextSize(themes_info[gui.users[host.io.user_id].theme_id].title.c_str(), 0, false, 260.f * SCALE.x).y / 2.f;
                ImGui::SameLine(0, 420.f * SCALE.x);
                const auto CALC_POS_TITLE = (SIZE_SELECT / 2.f) - CALC_TITLE;
                ImGui::SetCursorPosY(CALC_POS_TITLE);
                ImGui::PushTextWrapPos(SIZE_LIST.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[gui.users[host.io.user_id].theme_id].title.c_str());
                ImGui::PopTextWrapPos();
                ImGui::SetWindowFontScale(1.2f);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (CALC_POS_TITLE > 0 ? CALC_POS_TITLE : -CALC_POS_TITLE));
                ImGui::Separator();
            }
            if (ImGui::Selectable(is_lang ? lang["start_screen"].c_str() : "Start Screen", false, ImGuiSelectableFlags_None, ImVec2(0.f, 80.f * SCALE.y)))
                menu = "start";
            ImGui::Separator();
            if (ImGui::Selectable(is_lang ? lang["home_screen_backgrounds"].c_str() : "Home Screen Backgrounds", false, ImGuiSelectableFlags_None, ImVec2(0.f, 80.f * SCALE.y)))
                menu = "background";
            ImGui::PopStyleVar();
            ImGui::Separator();
        } else {
            if (menu == "theme") {
                // Theme List
                if (selected.empty()) {
                    title = is_lang ? lang["theme"].c_str() : "Theme";

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
                    if (set_scroll_pos != -1) {
                        ImGui::SetScrollY(set_scroll_pos);
                        set_scroll_pos = -1;
                    }
                    current_scroll_pos = ImGui::GetScrollY();
                    max_scroll_pos = ImGui::GetScrollMaxY();

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
                        if (ImGui::Selectable(gui.users[host.io.user_id].theme_id == theme.first ? "V" : "##preview", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                            selected = theme.first;
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
                            ImGui::SetCursorPos(ImVec2(15.f * SCALE.x, (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (72.f * SCALE.y)));
                            ImGui::Image(gui.themes_preview[selected]["home"], SIZE_PREVIEW);
                        }
                        if (gui.themes_preview[selected].find("start") != gui.themes_preview[selected].end()) {
                            ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) + (15.f * SCALE.y), (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (72.f * SCALE.y)));
                            ImGui::Image(gui.themes_preview[selected]["start"], SIZE_PREVIEW);
                        }
                        ImGui::SetWindowFontScale(1.2f);
                        ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) - (BUTTON_SIZE.x / 2.f), (SIZE_LIST.y - 82.f) - BUTTON_SIZE.y));
                        if ((selected != gui.users[host.io.user_id].theme_id) && (ImGui::Button(select, BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross))) {
                            gui.users[host.io.user_id].start_path.clear();
                            if (init_theme(gui, host, selected)) {
                                gui.users[host.io.user_id].theme_id = selected;
                                gui.users[host.io.user_id].use_theme_bg = true;
                            } else
                                gui.users[host.io.user_id].use_theme_bg = false;
                            init_theme_start_background(gui, host, selected);
                            gui.users[host.io.user_id].start_type = (selected == "default") ? "default" : "theme";
                            save_user(gui, host, host.io.user_id);
                            set_scroll_pos = current_scroll_pos;
                            selected.clear();
                        }
                    } else if (popup == "delete") {
                        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
                        ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
                        ImGui::Begin("##delete_theme", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                        ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
                        ImGui::BeginChild("##delete_theme_popup", POPUP_SIZE, true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                        ImGui::SetCursorPos(ImVec2(48.f * SCALE.x, 28.f * SCALE.y));
                        ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);
                        ImGui::Image(gui.themes_preview[selected]["package"], SIZE_MINI_PACKAGE);
                        ImGui::SameLine(0, 22.f * SCALE.x);
                        const auto CALC_TITLE = ImGui::CalcTextSize(themes_info[selected].title.c_str(), nullptr, false, POPUP_SIZE.x - SIZE_MINI_PACKAGE.x - (70.f * SCALE.x)).y / 2.f;
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (SIZE_MINI_PACKAGE.y / 2.f) - CALC_TITLE);
                        ImGui::TextWrapped("%s", themes_info[selected].title.c_str());
                        const auto delete_str = is_lang ? lang["delete"] : "This theme will be deleted.";
                        const auto CALC_TEXT = ImGui::CalcTextSize(delete_str.c_str());
                        ImGui::SetCursorPos(ImVec2(POPUP_SIZE.x / 2 - (CALC_TEXT.x / 2.f), POPUP_SIZE.y / 2.f));
                        ImGui::TextColored(GUI_COLOR_TEXT, delete_str.c_str());
                        ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - BUTTON_SIZE.x - (20.f * SCALE.x), POPUP_SIZE.y - BUTTON_SIZE.y - (22.f * SCALE.y)));
                        if (ImGui::Button(!common["cancel"].empty() ? common["cancel"].c_str() : "Cancel", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_circle))
                            popup.clear();
                        ImGui::SameLine(0, 40.f * SCALE.x);
                        if (ImGui::Button("OK", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross)) {
                            if (selected == gui.users[host.io.user_id].theme_id) {
                                gui.users[host.io.user_id].theme_id = "default";
                                gui.users[host.io.user_id].start_path.clear();
                                if (init_theme(gui, host, "default"))
                                    gui.users[host.io.user_id].use_theme_bg = true;
                                else
                                    gui.users[host.io.user_id].use_theme_bg = false;
                                save_user(gui, host, host.io.user_id);
                                init_theme_start_background(gui, host, "default");
                            }
                            fs::remove_all(fs::path{ host.pref_path } / "ux0/theme" / selected);
                            if (host.app_path == "NPXS10026")
                                init_content_manager(gui, host);
                            delete_theme = selected;
                            popup.clear();
                            selected.clear();
                            set_scroll_pos = current_scroll_pos;
                        }
                        ImGui::EndChild();
                        ImGui::PopStyleVar();
                        ImGui::End();
                    } else if (popup == "information") {
                        if (gui.themes_preview[selected].find("home") != gui.themes_preview[selected].end()) {
                            ImGui::SetCursorPos(ImVec2(119.f * SCALE.x, 4.f * SCALE.y));
                            ImGui::Image(gui.themes_preview[selected]["home"], SIZE_MINI_PREVIEW);
                        }
                        if (gui.themes_preview[selected].find("start") != gui.themes_preview[selected].end()) {
                            ImGui::SetCursorPos(ImVec2(SIZE_LIST.x / 2.f + (15.f * SCALE.y), 4.f * SCALE.y));
                            ImGui::Image(gui.themes_preview[selected]["start"], SIZE_MINI_PREVIEW);
                        }
                        const auto INFO_POS = ImVec2(280.f * SCALE.x, 30.f * SCALE.y);
                        ImGui::SetWindowFontScale(0.94f);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                        ImGui::TextColored(GUI_COLOR_TEXT, is_lang ? lang["name"].c_str() : "Name");
                        ImGui::SameLine();
                        ImGui::PushTextWrapPos(SIZE_LIST.x - (30.f * SCALE.x));
                        ImGui::SetCursorPosX(INFO_POS.x);
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[selected].title.c_str());
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                        ImGui::TextColored(GUI_COLOR_TEXT, is_lang ? lang["provider"].c_str() : "Provider");
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(INFO_POS.x);
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[selected].provided.c_str());
                        ImGui::PopTextWrapPos();
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                        ImGui::TextColored(GUI_COLOR_TEXT, is_lang ? lang["updated"].c_str() : "Updated");
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(INFO_POS.x);
                        auto DATE_TIME = get_date_time(gui, host, themes_info[selected].updated);
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", DATE_TIME["date"].c_str(), DATE_TIME["clock"].c_str());
                        if (gui.users[host.io.user_id].clock_12_hour) {
                            ImGui::SameLine();
                            ImGui::TextColored(GUI_COLOR_TEXT, "%s", DATE_TIME["day-moment"].c_str());
                        }
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                        ImGui::TextColored(GUI_COLOR_TEXT, is_lang ? lang["size"].c_str() : "Size");
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(INFO_POS.x);
                        ImGui::TextColored(GUI_COLOR_TEXT, "%zu KB", themes_info[selected].size);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                        ImGui::TextColored(GUI_COLOR_TEXT, is_lang ? lang["version"].c_str() : "Version");
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(INFO_POS.x);
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[selected].version.c_str());
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                        ImGui::TextColored(GUI_COLOR_TEXT, "Content ID");
                        ImGui::SameLine();
                        ImGui::SetCursorPosX(INFO_POS.x);
                        ImGui::TextWrapped("%s", selected.c_str());
                    }
                }
            } else if (menu == "start") {
                if (sub_menu.empty()) {
                    title = is_lang ? lang["start_screen"].c_str() : "Start Screen";
                    ImGui::SetWindowFontScale(0.72f);
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
                    const auto PACKAGE_POS_Y = (SIZE_LIST.y / 2.f) - (SIZE_PACKAGE.y / 2.f) - (72.f * SCALE.y);
                    const auto is_not_default = gui.users[host.io.user_id].theme_id != "default";
                    if (is_not_default) {
                        const auto THEME_POS = ImVec2(15.f * SCALE.x, PACKAGE_POS_Y);
                        if (gui.themes_preview[gui.users[host.io.user_id].theme_id].find("package") != gui.themes_preview[gui.users[host.io.user_id].theme_id].end()) {
                            ImGui::SetCursorPos(THEME_POS);
                            ImGui::Image(gui.themes_preview[gui.users[host.io.user_id].theme_id]["package"], SIZE_PACKAGE);
                        }
                        ImGui::SetCursorPos(THEME_POS);
                        ImGui::SetWindowFontScale(1.8f);
                        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                        if (ImGui::Selectable(gui.users[host.io.user_id].start_type == "theme" ? "V" : "##theme", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                            sub_menu = "theme";
                        ImGui::PopStyleColor();
                        ImGui::SetWindowFontScale(0.72f);
                        ImGui::SetCursorPosX(15.f * SCALE.x);
                        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + SIZE_PACKAGE.x);
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[gui.users[host.io.user_id].theme_id].title.c_str());
                        ImGui::PopTextWrapPos();
                    }
                    const auto IMAGE_POS = ImVec2(is_not_default ? (SIZE_LIST.x / 2.f) - (SIZE_PACKAGE.x / 2.f) : 15.f * SCALE.x, PACKAGE_POS_Y);
                    if ((gui.users[host.io.user_id].start_type == "image") && gui.start_background) {
                        ImGui::SetCursorPos(IMAGE_POS);
                        ImGui::Image(gui.start_background, SIZE_PACKAGE);
                    }
                    ImGui::SetCursorPos(IMAGE_POS);
                    if (gui.users[host.io.user_id].start_type == "image")
                        ImGui::SetWindowFontScale(1.8f);
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                    if (ImGui::Selectable(gui.users[host.io.user_id].start_type == "image" ? "V" : "Add Image", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                        sub_menu = "image";
                    ImGui::PopStyleColor();
                    ImGui::SetWindowFontScale(0.72f);
                    ImGui::SetCursorPosX(IMAGE_POS.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, is_lang ? lang["image"].c_str() : "Image");
                    const auto DEFAULT_POS = ImVec2(is_not_default ? (SIZE_LIST.x / 2.f) + (SIZE_PACKAGE.x / 2.f) + (30.f * SCALE.y) : (SIZE_LIST.x / 2.f) - (SIZE_PACKAGE.x / 2.f), PACKAGE_POS_Y);
                    if (gui.themes_preview["default"].find("package") != gui.themes_preview["default"].end()) {
                        ImGui::SetCursorPos(DEFAULT_POS);
                        ImGui::Image(gui.themes_preview["default"]["package"], SIZE_PACKAGE);
                        ImGui::SetCursorPos(DEFAULT_POS);
                        ImGui::SetWindowFontScale(1.8f);
                        ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                        if (ImGui::Selectable(gui.users[host.io.user_id].start_type == "default" ? "V" : "##default", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                            sub_menu = "default";
                        ImGui::PopStyleColor();
                        ImGui::SetWindowFontScale(0.72f);
                        ImGui::SetCursorPosX(DEFAULT_POS.x);
                        ImGui::TextColored(GUI_COLOR_TEXT, is_lang ? lang["default"].c_str() : "Default");
                    }
                    ImGui::PopStyleVar();
                    ImGui::SetWindowFontScale(0.90f);
                } else {
                    const auto START_PREVIEW_POS = ImVec2((SIZE_LIST.x / 2.f) - (SIZE_PREVIEW.x / 2.f), (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (72.f * SCALE.y));
                    const auto SELECT_BUTTON_POS = ImVec2((SIZE_LIST.x / 2.f) - (BUTTON_SIZE.x / 2.f), (SIZE_LIST.y - 82.f) - BUTTON_SIZE.y);
                    if (sub_menu == "theme") {
                        title = themes_info[gui.users[host.io.user_id].theme_id].title;
                        if (gui.themes_preview[gui.users[host.io.user_id].theme_id].find("start") != gui.themes_preview[gui.users[host.io.user_id].theme_id].end()) {
                            ImGui::SetCursorPos(START_PREVIEW_POS);
                            ImGui::Image(gui.themes_preview[gui.users[host.io.user_id].theme_id]["start"], SIZE_PREVIEW);
                        }
                        ImGui::SetCursorPos(SELECT_BUTTON_POS);
                        if ((gui.users[host.io.user_id].start_type != "theme") && (ImGui::Button(select, BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross))) {
                            gui.users[host.io.user_id].start_path.clear();
                            gui.users[host.io.user_id].start_type = "theme";
                            init_theme_start_background(gui, host, gui.users[host.io.user_id].theme_id);
                            save_user(gui, host, host.io.user_id);
                            sub_menu.clear();
                        }
                    } else if (sub_menu == "image") {
                        nfdchar_t *image_path;
                        nfdresult_t result = NFD_OpenDialog("bmp,gif,jpg,png,tif", nullptr, &image_path);

                        if ((result == NFD_OKAY) && init_user_start_background(gui, image_path)) {
                            gui.users[host.io.user_id].start_path = image_path;
                            gui.users[host.io.user_id].start_type = "image";
                            save_user(gui, host, host.io.user_id);
                        }
                        sub_menu.clear();
                    } else if (sub_menu == "default") {
                        title = is_lang ? lang["default"].c_str() : "Default";
                        if (gui.themes_preview["default"].find("start") != gui.themes_preview["default"].end()) {
                            ImGui::SetCursorPos(START_PREVIEW_POS);
                            ImGui::Image(gui.themes_preview["default"]["start"], SIZE_PREVIEW);
                        }
                        ImGui::SetCursorPos(SELECT_BUTTON_POS);
                        if ((gui.users[host.io.user_id].start_type != "default") && (ImGui::Button(select, BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross))) {
                            gui.users[host.io.user_id].start_path.clear();
                            init_theme_start_background(gui, host, "default");
                            gui.users[host.io.user_id].start_type = "default";
                            save_user(gui, host, host.io.user_id);
                            sub_menu.clear();
                        }
                    }
                }
            } else if (menu == "background") {
                title = is_lang ? lang["home_screen_backgrounds"] : "Home Screen Backgrounds";

                // Delete user background
                if (!delete_user_background.empty()) {
                    const auto bg = std::find(gui.users[host.io.user_id].backgrounds.begin(), gui.users[host.io.user_id].backgrounds.end(), delete_user_background);
                    gui.users[host.io.user_id].backgrounds.erase(bg);
                    gui.user_backgrounds.erase(delete_user_background);
                    if (gui.users[host.io.user_id].backgrounds.size())
                        gui.current_user_bg = 0;
                    else if (!gui.theme_backgrounds.empty())
                        gui.users[host.io.user_id].use_theme_bg = true;
                    save_user(gui, host, host.io.user_id);
                    delete_user_background.clear();
                }

                ImGui::SetWindowFontScale(0.90f);
                ImGui::Columns(3, nullptr, false);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 1.0f));
                for (const auto &background : gui.users[host.io.user_id].backgrounds) {
                    const auto IMAGE_POS = ImGui::GetCursorPosY();
                    ImGui::Image(gui.user_backgrounds[background], SIZE_PACKAGE);
                    ImGui::SetCursorPosY(IMAGE_POS);
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                    ImGui::PushID(background.c_str());
                    if (ImGui::Selectable("Delete Background", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                        delete_user_background = background;
                    ImGui::PopID();
                    ImGui::PopStyleColor();
                    ImGui::NextColumn();
                }
                if (ImGui::Selectable("Add Background", false, ImGuiSelectableFlags_None, SIZE_PACKAGE)) {
                    nfdchar_t *background_path;
                    nfdresult_t result = NFD_OpenDialog("bmp,gif,jpg,png,tif", nullptr, &background_path);

                    if ((result == NFD_OKAY) && (gui.user_backgrounds.find(background_path) == gui.user_backgrounds.end())) {
                        if (init_user_background(gui, host, host.io.user_id, background_path)) {
                            gui.users[host.io.user_id].backgrounds.push_back(background_path);
                            gui.users[host.io.user_id].use_theme_bg = false;
                            save_user(gui, host, host.io.user_id);
                        }
                    }
                }
                ImGui::PopStyleVar();
                ImGui::Columns(1);
            }
        }
    } else if (settings_menu == "date&time") {
        // Language
        title = is_lang ? lang["date_time"] : "Date & Time";
        ImGui::SetWindowFontScale(1.2f);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
        if (ImGui::Selectable(is_lang ? lang["date_format"].c_str() : "Date Format", menu == "date_format", ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT)))
            menu = "date_format";
        ImGui::Separator();
        if (ImGui::Selectable(is_lang ? lang["time_format"].c_str() : "Time Format", menu == "time_format", ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT)))
            menu = "time_format";
        ImGui::Separator();
        ImGui::PopStyleVar();
        if (!menu.empty()) {
            const auto WINDOW_TIME_SIZE = ImVec2(WINDOW_SIZE.x - 70.f * host.dpi_scale, WINDOW_SIZE.y);
            ImGui::SetNextWindowPos(ImVec2(70.f * SCALE.x, INFORMATION_BAR_HEIGHT), ImGuiCond_Always);
            ImGui::SetNextWindowSize(WINDOW_TIME_SIZE, ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
            ImGui::Begin("##time", &gui.live_area.settings, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            const auto TIME_SELECT_SIZE = 336.f * SCALE.x;
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
            ImGui::SetNextWindowPos(ImVec2(WINDOW_SIZE.x - TIME_SELECT_SIZE, INFORMATION_BAR_HEIGHT), ImGuiCond_Always, ImVec2(0.f, 0.f));
            ImGui::BeginChild("##time_select", ImVec2(TIME_SELECT_SIZE, WINDOW_SIZE.y), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 30.f * SCALE.x);
            ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);
            if (menu == "date_format") {
                for (auto f = 0; f < 3; f++) {
                    auto date_format_value = DateFormat(f);
                    const auto date_format_str = get_date_format_sting(gui, date_format_value);
                    ImGui::PushID(date_format_str.c_str());
                    ImGui::SetCursorPosY((display_size.y / 2.f) - INFORMATION_BAR_HEIGHT - (SIZE_PUPUP_SELECT * 1.5f) + (SIZE_PUPUP_SELECT * f));
                    if (ImGui::Selectable(gui.users[host.io.user_id].date_format == date_format_value ? "V" : "##date_format", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT))) {
                        if (gui.users[host.io.user_id].date_format != date_format_value) {
                            gui.users[host.io.user_id].date_format = date_format_value;
                            save_user(gui, host, host.io.user_id);
                        }
                        menu.clear();
                    }
                    ImGui::NextColumn();
                    ImGui::SetCursorPosY((display_size.y / 2.f) - INFORMATION_BAR_HEIGHT - (SIZE_PUPUP_SELECT * 1.5f) + (SIZE_PUPUP_SELECT * f));
                    ImGui::Selectable(date_format_str.c_str(), false, ImGuiSelectableFlags_None, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT));
                    ImGui::PopID();
                    ImGui::NextColumn();
                }
            } else if (menu == "time_format") {
                ImGui::SetCursorPosY((display_size.y / 2.f) - SIZE_PUPUP_SELECT - INFORMATION_BAR_HEIGHT);
                if (ImGui::Selectable(gui.users[host.io.user_id].clock_12_hour ? "V" : "##time_format", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT))) {
                    if (!gui.users[host.io.user_id].clock_12_hour) {
                        gui.users[host.io.user_id].clock_12_hour = true;
                        save_user(gui, host, host.io.user_id);
                    }
                    menu.clear();
                }
                ImGui::NextColumn();
                ImGui::SetCursorPosY((display_size.y / 2.f) - SIZE_PUPUP_SELECT - INFORMATION_BAR_HEIGHT);
                ImGui::Selectable(is_lang ? lang["12_hour_clock"].c_str() : "12-Hour Clock", false, ImGuiSelectableFlags_None, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT));
                ImGui::NextColumn();
                if (ImGui::Selectable(!gui.users[host.io.user_id].clock_12_hour ? "V" : "##time_format", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT))) {
                    if (gui.users[host.io.user_id].clock_12_hour) {
                        gui.users[host.io.user_id].clock_12_hour = false;
                        save_user(gui, host, host.io.user_id);
                    }
                    menu.clear();
                }
                ImGui::NextColumn();
                ImGui::Selectable(is_lang ? lang["24_hour_clock"].c_str() : "24-Hour Clock", false, ImGuiSelectableFlags_None, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT));
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::PopStyleVar();
            ImGui::EndChild();
            ImGui::PopStyleVar();
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                menu.clear();
            ImGui::End();
            ImGui::PopStyleVar();
        }
    } else if (settings_menu == "language") {
        // Language
        if (menu.empty()) {
            title = is_lang ? lang["language"] : "Language";
            const auto current_sys_lang = LIST_SYS_LANG[host.cfg.sys_lang];
            ImGui::Columns(2, nullptr, false);
            ImGui::SetWindowFontScale(1.2f);
            const auto sys_lang_str = is_lang ? lang["system_language"].c_str() : "System Language";
            const auto sys_lang_str_size = ImGui::CalcTextSize(sys_lang_str).x;
            ImGui::SetColumnWidth(0, sys_lang_str_size + 40.f);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
            if (ImGui::Selectable(sys_lang_str, false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT)))
                popup = "select_sys_lang";
            ImGui::PopStyleVar();
            ImGui::NextColumn();
            ImGui::SetWindowFontScale(0.8f);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(1.f, 0.5f));
            ImGui::Selectable(current_sys_lang.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT));
            ImGui::PopStyleVar();
            ImGui::SetWindowFontScale(1.2f);
            ImGui::Separator();
            ImGui::NextColumn();
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
            if (ImGui::Selectable(is_lang ? lang["input_language"].c_str() : "Input Languages", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT)))
                menu = "select_input_lang";
            ImGui::PopStyleVar();
            ImGui::NextColumn();
            ImGui::SetWindowFontScale(0.8f);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(1.f, 0.5f));
            ImGui::Selectable(">", false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT));
            ImGui::SetWindowFontScale(1.2f);
            ImGui::Separator();
            ImGui::NextColumn();
            ImGui::Columns(1);
            ImGui::PopStyleVar();
            if (popup == "select_sys_lang") {
                const auto WINDOW_LANG_LIST_SIZE = ImVec2(WINDOW_SIZE.x - 70.f, WINDOW_SIZE.y);
                ImGui::SetNextWindowPos(ImVec2(70.f, INFORMATION_BAR_HEIGHT), ImGuiCond_Always);
                ImGui::SetNextWindowSize(WINDOW_LANG_LIST_SIZE, ImGuiCond_Always);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
                ImGui::Begin("##system_language", &gui.live_area.settings, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                const auto SYS_LANG_SIZE = WINDOW_SIZE.x / 2.f;
                ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
                ImGui::SetNextWindowPos(ImVec2(WINDOW_SIZE.x - SYS_LANG_SIZE, INFORMATION_BAR_HEIGHT), ImGuiCond_Always, ImVec2(0.f, 0.f));
                ImGui::BeginChild("##system_language_select", ImVec2(SYS_LANG_SIZE, WINDOW_SIZE.y), false, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 30.f * SCALE.x);
                ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
                for (const auto &sys_lang : LIST_SYS_LANG) {
                    ImGui::PushID(sys_lang.c_str());
                    if (ImGui::Selectable(current_sys_lang == sys_lang ? "V" : "##lang", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(SYS_LANG_SIZE, SIZE_PUPUP_SELECT))) {
                        const auto lang_id = uint32_t(std::distance(LIST_SYS_LANG.begin(), std::find(LIST_SYS_LANG.begin(), LIST_SYS_LANG.end(), sys_lang)));
                        if (lang_id != host.cfg.sys_lang) {
                            host.cfg.sys_lang = lang_id;
                            config::serialize_config(host.cfg, host.base_path);
                            init_lang(gui, host);
                            get_sys_apps_title(gui, host);
                            std::sort(gui.app_selector.sys_apps.begin(), gui.app_selector.sys_apps.end(), [](const App &lhs, const App &rhs) {
                                return string_utils::toupper(lhs.title) < string_utils::toupper(rhs.title);
                            });
                            get_user_apps_title(gui, host);
                            std::sort(gui.app_selector.user_apps.begin(), gui.app_selector.user_apps.end(), [](const App &lhs, const App &rhs) {
                                return string_utils::toupper(lhs.title) < string_utils::toupper(rhs.title);
                            });
                            gui.apps_list_opened.clear();
                            gui.live_area_contents.clear();
                            gui.live_items.clear();
                            update_apps_list_opened(gui, host, "NPXS10015");
                            init_notice_info(gui, host);
                            init_live_area(gui, host);
                        }
                        popup.clear();
                    }
                    ImGui::NextColumn();
                    ImGui::Selectable(sys_lang.c_str(), false, ImGuiSelectableFlags_None, ImVec2(SYS_LANG_SIZE, SIZE_PUPUP_SELECT));
                    ImGui::PopID();
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);
                ImGui::PopStyleVar();
                ImGui::EndChild();
                ImGui::PopStyleVar();
                if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    popup.clear();
                ImGui::End();
                ImGui::PopStyleVar();
            }
        } else {
            // Input Languages
            const auto keyboards_str = is_lang ? lang["keyboards"].c_str() : "Keyboards";
            if (sub_menu.empty()) {
                title = is_lang ? lang["input_language"] : "Input Languages";
                ImGui::SetWindowFontScale(1.2f);
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 600.f * host.dpi_scale);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                if (ImGui::Selectable(keyboards_str, false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT)))
                    sub_menu = "select_keyboards";
                ImGui::PopStyleVar();
                ImGui::NextColumn();
                ImGui::SetWindowFontScale(1.f);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(1.f, 0.5f));
                ImGui::Selectable(">", false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT));
                ImGui::PopStyleVar();
                ImGui::Separator();
                ImGui::NextColumn();
                ImGui::Columns(1);
            } else {
                if (selected.empty()) {
                    title = keyboards_str;
                    ImGui::Columns(3, nullptr, false);
                    ImGui::SetColumnWidth(0, 40.f * host.dpi_scale);
                    ImGui::SetColumnWidth(1, 560.f * host.dpi_scale);
                    for (const auto &lang : get_list_ime_lang(host.ime)) {
                        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                        ImGui::PushID(lang.first);
                        ImGui::SetWindowFontScale(1.f);
                        const auto is_lang_enable = std::find(host.cfg.ime_langs.begin(), host.cfg.ime_langs.end(), lang.first) != host.cfg.ime_langs.end();
                        if (ImGui::Selectable(is_lang_enable ? "V" : "##lang", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT)))
                            selected = std::to_string(lang.first);
                        ImGui::NextColumn();
                        ImGui::SetWindowFontScale(1.2f);
                        ImGui::Selectable(lang.second.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT));
                        ImGui::PopStyleVar();
                        ImGui::NextColumn();
                        ImGui::SetWindowFontScale(1.f);
                        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(1.f, 0.5f));
                        ImGui::Selectable(">", false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT));
                        ImGui::Separator();
                        ImGui::PopStyleVar();
                        ImGui::PopID();
                        ImGui::NextColumn();
                    }
                    ImGui::Columns(1);
                } else {
                    const auto lang_select = SceImeLanguage(std::stoi(selected));
                    title = get_ime_lang_index(host.ime, lang_select)->second;
                    ImGui::SetWindowFontScale(1.2f);
                    ImGui::Columns(2, nullptr, false);
                    ImGui::SetColumnWidth(0, 650.f * SCALE.x);
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                    ImGui::Selectable(title.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT));
                    ImGui::PopStyleVar();
                    ImGui::NextColumn();
                    auto ime_lang_cfg_index = std::find(host.cfg.ime_langs.begin(), host.cfg.ime_langs.end(), lang_select);
                    auto is_ime_lang_enable = ime_lang_cfg_index != host.cfg.ime_langs.end();
                    ImGui::SetWindowFontScale(1.4f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (16.f * SCALE.y));
                    if (ImGui::Checkbox("##lang", &is_ime_lang_enable)) {
                        if (ime_lang_cfg_index != host.cfg.ime_langs.end()) {
                            host.cfg.ime_langs.erase(ime_lang_cfg_index);
                            if (host.cfg.ime_langs.empty())
                                host.cfg.current_ime_lang = SCE_IME_LANGUAGE_ENGLISH_US;
                            else if (host.cfg.current_ime_lang == lang_select)
                                host.cfg.current_ime_lang = host.cfg.ime_langs.back();
                        } else {
                            host.cfg.ime_langs.push_back(lang_select);

                            // Sort Ime lang in good order
                            if (host.cfg.ime_langs.size() > 1) {
                                std::vector<uint64_t> cfg_ime_langs_temp;
                                for (const auto &lang : get_list_ime_lang(host.ime)) {
                                    if (std::find(host.cfg.ime_langs.begin(), host.cfg.ime_langs.end(), (uint64_t)lang.first) != host.cfg.ime_langs.end())
                                        cfg_ime_langs_temp.push_back(lang.first);
                                }
                                host.cfg.ime_langs = cfg_ime_langs_temp;
                            }
                        }
                        config::serialize_config(host.cfg, host.base_path);
                    }
                    ImGui::NextColumn();
                    ImGui::Columns(1);
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Back
    ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
    ImGui::SetCursorPos(ImVec2(6.f * SCALE.x, display_size.y - (84.f * SCALE.y)));
    if (ImGui::Button("Back", ImVec2(64.f * SCALE.x, 40.f * SCALE.y))) {
        if (!settings_menu.empty()) {
            if (!menu.empty()) {
                if (!selected.empty()) {
                    if (!popup.empty())
                        popup.clear();
                    else {
                        selected.clear();
                        set_scroll_pos = current_scroll_pos;
                    }
                } else if (!sub_menu.empty())
                    sub_menu.clear();

                else
                    menu.clear();
            } else if (!popup.empty())
                popup.clear();
            else
                settings_menu.clear();
        } else {
            if (host.app_path == "NPXS10026") {
                gui.live_area.content_manager = true;
            } else {
                if (!gui.apps_list_opened.empty() && gui.apps_list_opened[gui.current_app_selected] == "NPXS10015")
                    gui.live_area.live_area_screen = true;
                else
                    gui.live_area.app_selector = true;
            }
            gui.live_area.settings = false;
        }
    }

    if ((settings_menu == "theme_background") && !selected.empty() && (selected != "default")) {
        ImGui::SetCursorPos(ImVec2(display_size.x - (70.f * SCALE.x), display_size.y - (84.f * SCALE.y)));
        if ((popup != "information") && ImGui::Button("...", ImVec2(64.f * SCALE.x, 40.f * SCALE.y)) || ImGui::IsKeyPressed(host.cfg.keyboard_button_triangle))
            ImGui::OpenPopup("...");
        if (ImGui::BeginPopup("...")) {
            if (ImGui::MenuItem(is_lang ? lang["information"].c_str() : "Information"))
                popup = "information";
            if (ImGui::MenuItem(!common["delete"].empty() ? common["delete"].c_str() : "Delete"))
                popup = "delete";
            ImGui::EndPopup();
        }
    }
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace gui
