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
#include <io/device.h>
#include <util/safe_time.h>
#include <util/string_utils.h>

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
            buffer = init_default_icon(gui, host);
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
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, INFORMATION_BAR_HEIGHT), display_size, IM_COL32(43.f, 44.f, 47.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);

    ImGui::GetForegroundDrawList()->AddRect(ImVec2(32.f * SCALE.x, 64.f * SCALE.y), ImVec2(display_size.x - (32.f * SCALE.x), display_size.y - (32.f * SCALE.y)), IM_COL32(255.f, 255.f, 255.f, 255.f), 20.0f * SCALE.x, ImDrawFlags_RoundCornersAll);

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
    const auto DATE_INIT_SCALE = ImVec2(date["date"].x * SCALE.x, date["date"].y * SCALE.y);
    const auto DATE_SIZE = ImVec2(CALC_DATE_SIZE.x * SCAL_DATE_FONT_SIZE, CALC_DATE_SIZE.y * SCAL_DATE_FONT_SIZE * SCAL_PIX_DATE_FONT);
    const auto DATE_POS = ImVec2(display_size.x - (start_param["dateLayout"] == "2" ? DATE_INIT_SCALE.x + (DATE_SIZE.x * RES_SCALE.x) : DATE_INIT_SCALE.x), display_size.y - DATE_INIT_SCALE.y);
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
        CLOCK_POS.x -= (CLOCK_SIZE.x * RES_SCALE.x) + (DAY_MOMENT_SIZE.x * RES_SCALE.x);
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

} // namespace gui
