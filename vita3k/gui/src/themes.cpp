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

#include <util/log.h>

#include <io/device.h>

#include <nfd.h>
#include <pugixml.hpp>
#include <stb_image.h>

namespace gui {

static bool add_user_image_background(GuiState &gui, HostState &host) {
    nfdchar_t *image_path = nullptr;
    nfdresult_t result = NFD_OpenDialog("bmp,gif,jpg,png,tif", nullptr, &image_path);

    if ((result == NFD_OKAY) && (gui.user_backgrounds.find(image_path) == gui.user_backgrounds.end())) {
        const std::string image_path_str = static_cast<std::string>(image_path);

        if (!fs::exists(fs::path(image_path_str))) {
            LOG_WARN("Image doesn't exist: {}.", image_path_str);
            return false;
        }

        int32_t width = 0;
        int32_t height = 0;
        stbi_uc *data = stbi_load(image_path_str.c_str(), &width, &height, nullptr, STBI_rgb_alpha);

        if (!data) {
            LOG_ERROR("Invalid or corrupted image: {}.", image_path);
            return false;
        }

        gui.user_backgrounds[image_path_str].init(gui.imgui_state.get(), data, width, height);
        stbi_image_free(data);
        if (gui.user_backgrounds[image_path_str]) {
            gui.current_user_bg = 0;
            host.cfg.user_backgrounds.push_back(image_path_str);
            host.cfg.use_theme_background = false;
            return true;
        } else
            return false;

    } else
        return false;
}

static std::map<std::string, std::map<std::string, std::string>> param;
static std::map<std::string, ImVec2> date;
static std::map<std::string, float> RGB;

void init_theme_start_background(GuiState &gui, HostState &host, const std::string &content_id) {
    std::string theme_start_name;
    std::string src_start;

    const auto THEME_PATH{ fs::path(host.pref_path) / "ux0/theme" / content_id };
    RGB.clear(), param.clear();
    if (content_id != "default") {
        const auto THEME_PATH_XML{ THEME_PATH / "theme.xml" };
        pugi::xml_document doc;
        if (doc.load_file(THEME_PATH_XML.c_str())) {
            const auto theme = doc.child("theme");

            // Start Layout
            if (!theme.child("StartScreenProperty").child("m_dateColor").empty())
                param["start"]["dateColor"] = theme.child("StartScreenProperty").child("m_dateColor").text().as_string();
            if (!theme.child("StartScreenProperty").child("m_dateLayout").empty())
                param["start"]["dateLayout"] = theme.child("StartScreenProperty").child("m_dateLayout").text().as_string();

            // Theme Start
            if (!theme.child("StartScreenProperty").child("m_filePath").empty()) {
                theme_start_name = theme.child("StartScreenProperty").child("m_filePath").text().as_string();
                src_start = "theme";
            }
        }
    }

    const auto default_fw_path{ fs::path(host.pref_path) / "vs0/data/internal/keylock/keylock.png" };
    if (theme_start_name.empty()) {
        if (fs::exists(default_fw_path))
            src_start = "default";
        else
            LOG_WARN("Firmware not found for path content id: {}", THEME_PATH.string());
    }

    int32_t width = 0;
    int32_t height = 0;
    vfs::FileBuffer buffer;

    if (src_start == "default")
        vfs::read_file(VitaIoDevice::vs0, buffer, host.pref_path, "data/internal/keylock/keylock.png");
    else
        vfs::read_file(VitaIoDevice::ux0, buffer, host.pref_path, "theme/" + content_id + "/" + theme_start_name);

    if (buffer.empty()) {
        LOG_WARN("background, Name: '{}', Not found for content id: {}.", theme_start_name, content_id);
        return;
    }
    stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
    if (!data) {
        LOG_ERROR("Invalid Background for title: {}.", content_id);
        return;
    }

    gui.start_background.init(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);

    date["date"] = ImVec2(898, 186.f);
    date["clock"] = ImVec2(898.f, 146.f);
    if (!param["start"]["dateLayout"].empty()) {
        switch (std::stoi(param["start"]["dateLayout"])) {
        case 0:
            break;
        case 1:
            date["date"] = ImVec2(898, 472.f);
            date["clock"] = ImVec2(898, 432.f);
            break;
        case 2:
            date["date"] = ImVec2(460.f, 186.f);
            date["clock"] = ImVec2(460, 146.f);
            break;
        default:
            LOG_WARN("Date layout is unknown : {}", param["start"]["dateLayout"]);
            break;
        }
    }

    if (!param["start"]["dateColor"].empty()) {
        int color;
        sscanf(param["start"]["dateColor"].c_str(), "%x", &color);
        RGB["R"] = float((color >> 16) & 0xFF);
        RGB["G"] = float((color >> 8) & 0xFF);
        RGB["B"] = float((color >> 0) & 0xFF);
    } else
        RGB["R"] = 255.f, RGB["G"] = 255.f, RGB["B"] = 255.f;

    if (gui.start_background)
        host.cfg.start_background = src_start;
}

bool init_user_start_background(GuiState &gui, const std::string &image_path) {
    RGB.clear(), param.clear();
    if (!fs::exists(image_path)) {
        LOG_WARN("Image doesn't exist: {}.", image_path);
        return false;
    }

    int32_t width = 0;
    int32_t height = 0;
    stbi_uc *data = stbi_load(image_path.c_str(), &width, &height, nullptr, STBI_rgb_alpha);

    if (!data) {
        LOG_ERROR("Invalid or corrupted image: {}.", image_path);
        return false;
    }

    gui.start_background.init(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);

    date["date"] = ImVec2(898, 186.f);
    date["clock"] = ImVec2(898.f, 146.f);
    RGB["R"] = 255.f, RGB["G"] = 255.f, RGB["B"] = 255.f;

    return gui.start_background;
}

bool init_theme(GuiState &gui, HostState &host, std::string &content_id) {
    std::vector<std::string> theme_bg_name;

    gui.current_theme_bg = 0;
    gui.theme_backgrounds.clear();

    if (content_id != "default") {
        const auto theme_path{ fs::path(host.pref_path) / "ux0/theme" / content_id };

        const auto theme_path_xml{ theme_path / "theme.xml" };
        pugi::xml_document doc;

        if (doc.load_file(theme_path_xml.c_str())) {
            const auto theme = doc.child("theme");

            // Theme Backgrounds
            for (const auto &param : theme.child("HomeProperty").child("m_bgParam")) {
                if (!param.child("m_imageFilePath").text().empty()) {
                    theme_bg_name.push_back(param.child("m_imageFilePath").text().as_string());
                    gui.theme_backgrounds.push_back({});
                }
            }
        } else
            LOG_ERROR("Theme not found for Content ID: {}, in path: {}", content_id, theme_path_xml.string());
    }

    for (auto pos = 0; pos < theme_bg_name.size(); pos++) {
        int32_t width = 0;
        int32_t height = 0;
        vfs::FileBuffer buffer;

        vfs::read_file(VitaIoDevice::ux0, buffer, host.pref_path, "theme/" + content_id + "/" + theme_bg_name[pos]);

        if (buffer.empty()) {
            LOG_WARN("background, Name: '{}', Not found for content id: {}.", theme_bg_name[pos], content_id);
            continue;
        }
        stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
        if (!data) {
            LOG_ERROR("Invalid Background for title: {}.", content_id);
            continue;
        }

        gui.theme_backgrounds[pos].init(gui.imgui_state.get(), data, width, height);
        stbi_image_free(data);
    }

    return content_id != "default" ? !gui.theme_backgrounds.empty() : true;
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
    const auto theme_path{ fs::path(host.pref_path) / "ux0/theme" };
    const auto fw_theme_path{ fs::path(host.pref_path) / "vs0/data/internal/theme" };
    if ((!fs::exists(theme_path) && !fs::is_empty(theme_path)) || !fs::exists(fw_theme_path)) {
        LOG_WARN("Theme path is empty");
        return;
    }

    gui.themes_preview.clear(), themes_info.clear(), themes_list.clear();

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

                themes_info[content_id].updated = fmt::format("{}/{}/{}  {:0>2d}:{:0>2d}", updated_date->tm_mday, updated_date->tm_mon + 1, updated_date->tm_year + 1900, updated_date->tm_hour, updated_date->tm_min).c_str();

                themes_info[content_id].size = theme_size / 1024;
                themes_info[content_id].version = infomation.child("m_contentVer").text().as_string();

                themes_list.push_back({ content_id, updated });
            } else
                LOG_ERROR("theme not found for title: {}", content_id);
        }
    }

    std::sort(themes_list.begin(), themes_list.end(), [](const auto &ta, const auto &tb) {
        return ta.second > tb.second;
    });

    if (fs::exists(fs::path(host.pref_path) / "vs0/app/NPXS10015/sce_sys/pic0.png")) {
        theme_preview_name["default"]["background"] = "app/NPXS10015/sce_sys/pic0.png";

        theme_preview_name["default"]["package"] = "data/internal/theme/theme_defaultImage.png";

        theme_preview_name["default"]["home"] = "data/internal/theme/defaultTheme_homeScreen.png";
        theme_preview_name["default"]["start"] = "data/internal/theme/defaultTheme_startScreen.png";

        themes_info["default"].title = "Default";
        themes_list.push_back({ "default", {} });
    } else
        LOG_WARN("Not found fw content");

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
    const auto MENUBAR_HEIGHT = 22.f;
    const auto WINDOW_SIZE = ImVec2(display_size.x, display_size.y - MENUBAR_HEIGHT);
    const auto DATE_POS = ImVec2(display_size.x - (date["date"].x * SCAL.x), display_size.y - (date["date"].y * SCAL.y));
    const auto CLOCK_POS = ImVec2(display_size.x - (date["clock"].x * SCAL.x), display_size.y - (date["clock"].y * SCAL.y));

    ImGui::SetNextWindowPos(ImVec2(0, MENUBAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::Begin("##start_screen", &gui.theme.start_screen, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    if (gui.start_background)
        ImGui::GetForegroundDrawList()->AddImage(gui.start_background,
            ImVec2(0.f, MENUBAR_HEIGHT), display_size);

    ImGui::GetForegroundDrawList()->AddRect(ImVec2(34.f, 56.f), ImVec2(display_size.x - (34.f * SCAL.x), display_size.y - (34.f * SCAL.y)), IM_COL32(255, 255, 255, 255), 10.0f, ImDrawCornerFlags_All);

    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);
    const auto local = *localtime(&tt);

    ImGui::GetForegroundDrawList()->AddText(gui.live_area_font, 40.0f * SCAL.x, DATE_POS, IM_COL32(RGB["R"], RGB["G"], RGB["B"], 255),
        fmt::format("{} {} ({})", local.tm_mday, wmonth[local.tm_mon], wday[local.tm_wday]).c_str());
    ImGui::GetForegroundDrawList()->AddText(gui.live_area_font, 108.0f * SCAL.x, CLOCK_POS, IM_COL32(RGB["R"], RGB["G"], RGB["B"], 255),
        fmt::format("{:0>2d}:{:0>2d}", local.tm_hour, local.tm_min).c_str());

    if (ImGui::IsMouseClicked(0))
        gui.theme.start_screen = false;

    ImGui::End();
}

static std::string popup, menu, selected, title, start, delete_user_background;
static ImGuiTextFilter search_bar;

void draw_themes_selection(GuiState &gui, HostState &host) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const auto SCAL = ImVec2(display_size.x / 960.0f, display_size.y / 544.0f);

    const auto BUTTON_SIZE = ImVec2(310.f * SCAL.x, 46.f * SCAL.y);
    const auto ICON_SIZE = ImVec2(100.f * SCAL.x, 100.f * SCAL.y);
    const auto MENUBAR_HEIGHT = 22.f;
    const auto WINDOW_SIZE = ImVec2(display_size.x, display_size.y - MENUBAR_HEIGHT);
    const auto SIZE_PREVIEW = ImVec2(360.f * SCAL.x, 204.f * SCAL.y);
    const auto SIZE_MINI_PREVIEW = ImVec2(256.f * SCAL.x, 145.f * SCAL.y);
    const auto SIZE_LIST = ImVec2(780 * SCAL.x, 430.f * SCAL.y);
    const auto SIZE_PACKAGE = ImVec2(226.f * SCAL.x, 128.f * SCAL.y);
    const auto SIZE_MINI_PACKAGE = ImVec2(170.f * SCAL.x, 96.f * SCAL.y);
    const auto POPUP_SIZE = ImVec2(756.0f * SCAL.x, 436.0f * SCAL.y);

    // Themes dialog
    ImGui::SetNextWindowPos(ImVec2(0, MENUBAR_HEIGHT), ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    if (gui.themes_preview["default"].find("background") == gui.themes_preview["default"].end())
        ImGui::SetNextWindowBgAlpha(0.999f);
    ImGui::Begin("##themes", &gui.theme.theme_background, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    if (gui.themes_preview["default"].find("background") != gui.themes_preview["default"].end())
        ImGui::GetWindowDrawList()->AddImage(gui.themes_preview["default"]["background"], ImVec2(0.f, MENUBAR_HEIGHT), display_size);
    ImGui::SetWindowFontScale(1.6f * SCAL.x);
    const auto theme_str = ImGui::CalcTextSize(title.c_str(), 0, false, SIZE_LIST.x);
    ImGui::PushTextWrapPos(((display_size.x - SIZE_LIST.x) / 2.f) + SIZE_LIST.x);
    ImGui::SetCursorPos(ImVec2((display_size.x / 2.f) - (theme_str.x / 2.f), (35.f * SCAL.y) - (theme_str.y / 2.f)));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", title.c_str());
    ImGui::PopTextWrapPos();
    if ((menu == "theme") && selected.empty()) {
        ImGui::SetWindowFontScale(1.2f * SCAL.x);
        const auto search_size = ImGui::CalcTextSize("Search");
        ImGui::SetCursorPos(ImVec2(display_size.x - 220.f - search_size.x, (35.f * SCAL.y) - (search_size.y / 2.f)));
        ImGui::TextColored(GUI_COLOR_TEXT, "Search");
        ImGui::SameLine();
        search_bar.Draw("##search_bar", 200);
        ImGui::SetWindowFontScale(1.6f * SCAL.x);
    }
    ImGui::SetCursorPosY(70.0f * SCAL.y);
    ImGui::Separator();
    ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, 102.0f * SCAL.y), ImGuiCond_Always, ImVec2(0.5f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
    ImGui::BeginChild("##themes_child", SIZE_LIST, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    if (menu.empty()) {
        title = "Theme & Background";
        ImGui::SetWindowFontScale(1.2f);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
        if (!themes_info.empty()) {
            if (ImGui::Selectable("Theme", false, ImGuiSelectableFlags_None, ImVec2(0.f, 80.f * SCAL.y)))
                menu = "theme";
            if (!host.cfg.theme_content_id.empty()) {
                ImGui::SetWindowFontScale(0.74f);
                const auto CALC_TITLE = ImGui::CalcTextSize(themes_info[host.cfg.theme_content_id].title.c_str(), 0, false, 200.f * SCAL.x).y;
                ImGui::SameLine(500.f * SCAL.x);
                ImGui::SetCursorPosY((50.f * SCAL.y) + ((CALC_TITLE / 2.f) - CALC_TITLE));
                ImGui::PushTextWrapPos(display_size.x - (150.f * SCAL.x));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[host.cfg.theme_content_id].title.c_str());
                ImGui::PopTextWrapPos();
                ImGui::SetWindowFontScale(1.2f);
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
    } else if (menu == "theme") {
        if (selected.empty()) {
            title = "Theme";
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
                if (ImGui::Selectable(host.cfg.theme_content_id == theme.first ? "V" : "##preview", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
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
            title = themes_info[selected].title;
            if (popup.empty()) {
                if (gui.themes_preview[selected].find("home") != gui.themes_preview[selected].end()) {
                    ImGui::SetCursorPos(ImVec2(15.f * SCAL.x, (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (50.f * SCAL.y)));
                    ImGui::Image(gui.themes_preview[selected]["home"], SIZE_PREVIEW);
                }
                if (gui.themes_preview[selected].find("start") != gui.themes_preview[selected].end()) {
                    ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) + (15.f * SCAL.y), (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (50.f * SCAL.y)));
                    ImGui::Image(gui.themes_preview[selected]["start"], SIZE_PREVIEW);
                }
                ImGui::SetWindowFontScale(1.2f);
                ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) - (BUTTON_SIZE.x / 2.f), (SIZE_LIST.y - 60.f) - BUTTON_SIZE.y));
                if (ImGui::Button("Select", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross)) {
                    if (init_theme(gui, host, selected)) {
                        host.cfg.theme_content_id = selected;
                        init_theme_start_background(gui, host, selected);
                        if (!gui.theme_backgrounds.empty())
                            host.cfg.use_theme_background = true;
                        selected.clear();
                        if (host.cfg.overwrite_config)
                            config::serialize_config(host.cfg, host.cfg.config_path);
                    }
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
                if (ImGui::Button("Cancel", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_circle))
                    popup.clear();
                ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - 50.f - BUTTON_SIZE.x, POPUP_SIZE.y - (22.f + BUTTON_SIZE.y)));
                if (ImGui::Button("Ok", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross)) {
                    if (selected == host.cfg.theme_content_id) {
                        if (host.cfg.start_background == "theme") {
                            host.cfg.start_background.clear();
                            gui.start_background = {};
                        }
                        host.cfg.theme_content_id.clear();
                        gui.theme_backgrounds.clear();
                        config::serialize_config(host.cfg, host.cfg.config_path);
                    }
                    fs::remove_all(fs::path{ host.pref_path } / "ux0/theme" / selected);
                    get_themes_list(gui, host);
                    popup.clear();
                    selected.clear();
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
            if (!host.cfg.theme_content_id.empty() && (host.cfg.theme_content_id != "default")) {
                const auto THEME_POS = ImVec2(15.f * SCAL.x, (SIZE_LIST.y / 2.f) - (SIZE_PACKAGE.y / 2.f) - (50.f * SCAL.y));
                if (gui.themes_preview[host.cfg.theme_content_id].find("package") != gui.themes_preview[host.cfg.theme_content_id].end()) {
                    ImGui::SetCursorPos(THEME_POS);
                    ImGui::Image(gui.themes_preview[host.cfg.theme_content_id]["package"], SIZE_PACKAGE);
                }
                ImGui::SetCursorPos(THEME_POS);
                ImGui::SetWindowFontScale(1.8f);
                ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                if (ImGui::Selectable(host.cfg.start_background == "theme" ? "V" : "##theme", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                    start = "theme";
                ImGui::PopStyleColor();
                ImGui::SetWindowFontScale(0.72f);
                ImGui::SetCursorPosX(15.f * SCAL.x);
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + SIZE_PACKAGE.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[host.cfg.theme_content_id].title.c_str());
                ImGui::PopTextWrapPos();
            }
            const auto IMAGE_POS = ImVec2((SIZE_LIST.x / 2.f) - (SIZE_PACKAGE.x / 2.f), (SIZE_LIST.y / 2.f) - (SIZE_PACKAGE.y / 2.f) - (50.f * SCAL.y));
            if ((host.cfg.start_background == "image") && gui.start_background) {
                ImGui::SetCursorPos(IMAGE_POS);
                ImGui::Image(gui.start_background, SIZE_PACKAGE);
            }
            ImGui::SetCursorPos(IMAGE_POS);
            if (host.cfg.start_background == "image")
                ImGui::SetWindowFontScale(1.8f);
            ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
            if (ImGui::Selectable(host.cfg.start_background == "image" ? "V" : "Add Image", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                start = "image";
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(0.72f);
            ImGui::SetCursorPosX(IMAGE_POS.x);
            ImGui::TextColored(GUI_COLOR_TEXT, "Image");
            const auto DEFAULT_POS = ImVec2((SIZE_LIST.x / 2.f) + (SIZE_PACKAGE.x / 2.f) + (30.f * SCAL.y), (SIZE_LIST.y / 2.f) - (SIZE_PACKAGE.y / 2.f) - (50.f * SCAL.y));
            if (gui.themes_preview["default"].find("package") != gui.themes_preview["default"].end()) {
                ImGui::SetCursorPos(DEFAULT_POS);
                ImGui::Image(gui.themes_preview["default"]["package"], SIZE_PACKAGE);
                ImGui::SetCursorPos(DEFAULT_POS);
                ImGui::SetWindowFontScale(1.8f);
                ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                if (ImGui::Selectable(host.cfg.start_background == "default" ? "V" : "##default", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                    start = "default";
                ImGui::PopStyleColor();
                ImGui::SetWindowFontScale(0.72f);
                ImGui::SetCursorPosX(DEFAULT_POS.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "Default");
            }
            ImGui::PopStyleVar();
            ImGui::SetWindowFontScale(0.90f);
        } else if (start == "theme") {
            title = themes_info[host.cfg.theme_content_id].title;
            if (gui.themes_preview[host.cfg.theme_content_id].find("start") != gui.themes_preview[selected].end()) {
                ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) - (SIZE_PREVIEW.x / 2.f), (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (50.f * SCAL.y)));
                ImGui::Image(gui.themes_preview[host.cfg.theme_content_id]["start"], SIZE_PREVIEW);
            }
            ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) - (BUTTON_SIZE.x / 2.f), (SIZE_LIST.y - 60.f) - BUTTON_SIZE.y));
            if (ImGui::Button("Select", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross)) {
                init_theme_start_background(gui, host, host.cfg.theme_content_id);
                host.cfg.start_background = "theme";
                start.clear();
                if (host.cfg.overwrite_config)
                    config::serialize_config(host.cfg, host.cfg.config_path);
            }
        } else if (start == "image") {
            nfdchar_t *image_path = nullptr;
            nfdresult_t result = NFD_OpenDialog("bmp,gif,jpg,png,tif", nullptr, &image_path);

            if (result == NFD_OKAY) {
                const std::string image_path_str = static_cast<std::string>(image_path);

                if (init_user_start_background(gui, image_path)) {
                    host.cfg.user_start_background = image_path_str;
                    host.cfg.start_background = "image";
                    if (host.cfg.overwrite_config)
                        config::serialize_config(host.cfg, host.cfg.config_path);
                    start.clear();
                } else
                    start.clear();
            } else
                start.clear();
        } else if (start == "default") {
            title = "Default";
            if (gui.themes_preview["default"].find("start") != gui.themes_preview[selected].end()) {
                ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) - (SIZE_PREVIEW.x / 2.f), (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (50.f * SCAL.y)));
                ImGui::Image(gui.themes_preview["default"]["start"], SIZE_PREVIEW);
            }
            ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) - (BUTTON_SIZE.x / 2.f), (SIZE_LIST.y - 60.f) - BUTTON_SIZE.y));
            if (ImGui::Button("Select", BUTTON_SIZE) || ImGui::IsKeyPressed(host.cfg.keyboard_button_cross)) {
                init_theme_start_background(gui, host, "default");
                host.cfg.start_background = "default";
                start.clear();
                if (host.cfg.overwrite_config)
                    config::serialize_config(host.cfg, host.cfg.config_path);
            }
        }
    } else if (menu == "background") {
        title = "Background";
        if (!delete_user_background.empty()) {
            gui.user_backgrounds.erase(delete_user_background);
            delete_user_background.clear();
        }
        ImGui::SetWindowFontScale(0.90f);
        ImGui::Columns(3, nullptr, false);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 1.0f));
        for (const auto &background : host.cfg.user_backgrounds) {
            const auto IMAGE_POS = ImGui::GetCursorPosY();
            ImGui::Image(gui.user_backgrounds[background], SIZE_PACKAGE);
            ImGui::SetCursorPosY(IMAGE_POS);
            ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
            ImGui::PushID(background.c_str());
            if (ImGui::Selectable("Delete Image", false, ImGuiSelectableFlags_None, SIZE_PACKAGE)) {
                const auto b = std::find(host.cfg.user_backgrounds.begin(), host.cfg.user_backgrounds.end(), background);
                delete_user_background = background;
                host.cfg.user_backgrounds.erase(b);
                if (gui.current_user_bg == host.cfg.user_backgrounds.size())
                    gui.current_user_bg = 0;
                if (host.cfg.overwrite_config)
                    config::serialize_config(host.cfg, host.cfg.config_path);
            }
            ImGui::PopID();
            ImGui::PopStyleColor();
            ImGui::NextColumn();
        }
        if (ImGui::Selectable("Add Image", false, ImGuiSelectableFlags_None, SIZE_PACKAGE) && add_user_image_background(gui, host)) {
            if (host.cfg.overwrite_config)
                config::serialize_config(host.cfg, host.cfg.config_path);
        }
        ImGui::PopStyleVar();
        ImGui::Columns(1);
    }
    ImGui::EndChild();

    ImGui::SetCursorPos(ImVec2(5.f, display_size.y - (87.f * SCAL.y)));
    if (ImGui::Button("Back", ImVec2(80.f * SCAL.x, 50.f * SCAL.y))) {
        if (!menu.empty()) {
            if (!selected.empty()) {
                if (!popup.empty())
                    popup.clear();
                else {
                    selected = -1;
                    selected.clear();
                }
            } else if (!start.empty())
                start.clear();
            else
                menu.clear();
        } else
            gui.theme.theme_background = false;
    }

    if (!selected.empty() && (selected != "default")) {
        ImGui::SetCursorPos(ImVec2(display_size.x - (60.f * SCAL.x), display_size.y - (85.f * SCAL.y)));
        if ((popup != "information") && ImGui::Button("...", ImVec2(60.f * SCAL.x, 50.f * SCAL.y)) || ImGui::IsKeyPressed(host.cfg.keyboard_button_triangle))
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
}

} // namespace gui
