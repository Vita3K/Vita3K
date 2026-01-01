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

#include <config/functions.h>
#include <config/state.h>
#include <dialog/state.h>
#include <gui/functions.h>
#include <host/dialog/filesystem.h>
#include <ime/functions.h>
#include <io/VitaIoDevice.h>
#include <io/state.h>
#include <lang/functions.h>
#include <numeric>
#include <util/safe_time.h>
#include <util/string_utils.h>

#include <pugixml.hpp>
#include <stb_image.h>
#include <util/vector_utils.h>

#undef ERROR

namespace gui {

struct Theme {
    std::string title;
    std::string provided;
    tm updated;
    size_t size;
    std::string version;
};

static std::map<std::string, Theme> themes_info;
static std::vector<std::pair<std::string, time_t>> themes_list;
static time_t last_updated_themes_list = 0;

static void get_themes_list(GuiState &gui, EmuEnvState &emuenv) {
    const auto theme_path{ emuenv.pref_path / "ux0/theme" };
    const auto fw_theme_path{ emuenv.pref_path / "vs0/data/internal/theme" };
    if ((!fs::exists(fw_theme_path) || fs::is_empty(fw_theme_path)) && (!fs::exists(theme_path) || fs::is_empty(theme_path))) {
        LOG_WARN("Theme path is empty");
        return;
    }

    // Check if the themes list has been updated recently to avoid unnecessary updates
    if ((fs::last_write_time(theme_path) <= last_updated_themes_list) && (fs::last_write_time(fw_theme_path) < last_updated_themes_list))
        return;
    else if (last_updated_themes_list != 0)
        LOG_INFO("Found new update of themes list, updating...");

    // Clear all themes list
    gui.themes_preview.clear();
    themes_info.clear();
    themes_list.clear();

    std::string user_lang;
    const auto sys_lang = static_cast<SceSystemParamLang>(emuenv.cfg.sys_lang);
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

    std::map<std::string, std::map<ThemePreviewType, std::string>> theme_preview_name;
    for (const auto &theme : fs::directory_iterator(theme_path)) {
        if (!theme.path().empty() && fs::is_directory(theme.path())) {
            const auto content_id_path = theme.path().filename();
            const auto content_id = fs_utils::path_to_utf8(content_id_path);
            const auto theme_path_xml{ theme_path / content_id_path / "theme.xml" };
            pugi::xml_document doc;

            if (doc.load_file(theme_path_xml.c_str())) {
                const auto infomation = doc.child("theme").child("InfomationProperty");

                // Thumbnail theme
                theme_preview_name[content_id][PACKAGE] = infomation.child("m_packageImageFilePath").text().as_string();

                // Preview theme
                theme_preview_name[content_id][HOME] = infomation.child("m_homePreviewFilePath").text().as_string();
                theme_preview_name[content_id][LOCK] = infomation.child("m_startPreviewFilePath").text().as_string();

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

                const auto pred = [](const auto acc, const auto &theme) {
                    if (fs::is_regular_file(theme.path()))
                        return acc + fs::file_size(theme.path());
                    return acc;
                };
                const auto theme_content_ids = fs::recursive_directory_iterator(theme_path / content_id_path);
                const auto theme_size = std::accumulate(fs::begin(theme_content_ids), fs::end(theme_content_ids), boost::uintmax_t{}, pred);

                const auto updated = fs::last_write_time(theme_path / content_id_path);
                SAFE_LOCALTIME(&updated, &themes_info[content_id].updated);

                themes_info[content_id].size = theme_size / KiB(1);
                themes_info[content_id].version = infomation.child("m_contentVer").text().as_string();

                themes_list.emplace_back(content_id, updated);
            } else
                LOG_ERROR("theme not found for title: {}", content_id);
        }
    }

    std::sort(themes_list.begin(), themes_list.end(), [&](const auto &ta, const auto &tb) {
        return ta.second > tb.second;
    });

    if (fs::exists(fw_theme_path) && !fs::is_empty(fw_theme_path)) {
        theme_preview_name["default"][PACKAGE] = "data/internal/theme/theme_defaultImage.png";

        theme_preview_name["default"][HOME] = "data/internal/theme/defaultTheme_homeScreen.png";
        theme_preview_name["default"][LOCK] = "data/internal/theme/defaultTheme_startScreen.png";

        themes_info["default"].title = gui.lang.settings.theme_background.main["default"];
        themes_list.emplace_back("default", time_t{});
    } else
        LOG_WARN("Default theme not found, install firmware fix this!");

    for (const auto &[content_id, theme] : themes_info) {
        for (const auto &[preview_type, theme_name] : theme_preview_name[content_id]) {
            if (!theme_name.empty()) {
                int32_t width = 0;
                int32_t height = 0;
                vfs::FileBuffer buffer;

                if (content_id == "default")
                    vfs::read_file(VitaIoDevice::vs0, buffer, emuenv.pref_path, theme_name);
                else
                    vfs::read_file(VitaIoDevice::ux0, buffer, emuenv.pref_path, fs::path("theme") / fs_utils::utf8_to_path(content_id) / theme_name);

                if (buffer.empty()) {
                    LOG_WARN("Background, Name: '{}', Not found for title: {} [{}].", theme_name, content_id, theme.title);
                    continue;
                }
                stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
                if (!data) {
                    LOG_ERROR("Invalid Background for title: {} [{}].", content_id, theme.title);
                    continue;
                }

                gui.themes_preview[content_id][preview_type] = ImGui_Texture(gui.imgui_state.get(), data, width, height);
                stbi_image_free(data);
            }
        }
    }

    // Update last updated themes list
    last_updated_themes_list = time(0);
}

void draw_settings(GuiState &gui, EmuEnvState &emuenv) {
    static std::string selected, title, delete_user_background, delete_theme;
    static ImGuiTextFilter search_bar;

    static float set_scroll_pos, current_scroll_pos, max_scroll_pos;

    enum class Popup {
        UNDEFINED,
        DEL,
        INFORMATION,
        SELECT_SYS_LANG
    };

    static Popup popup = Popup::UNDEFINED;

    enum class SubMenu {
        UNDEFINED,
        THEME,
        IMAGE,
        DEFAULT,
        SELECT_KEYBOARDS
    };
    static SubMenu sub_menu = SubMenu::UNDEFINED;

    enum class Menu {
        UNDEFINED,
        THEME,
        START,
        BACKGROUND,
        DATE_FORMAT,
        TIME_FORMAT,
        SELECT_INPUT_LANG
    };
    static Menu menu = Menu::UNDEFINED;

    enum class SettingsMenu {
        SELECT,
        THEME_BACKGROUND,
        DATE_TIME,
        LANGUAGE
    };

    static SettingsMenu settings_menu = SettingsMenu::SELECT;

    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

    const auto BUTTON_SIZE = ImVec2(310.f * SCALE.x, 46.f * SCALE.y);
    const auto INFORMATION_BAR_HEIGHT = 32.f * SCALE.y;
    const ImVec2 WINDOW_POS(VIEWPORT_POS.x, VIEWPORT_POS.y + INFORMATION_BAR_HEIGHT);
    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INFORMATION_BAR_HEIGHT);
    const auto SIZE_PREVIEW = ImVec2(360.f * SCALE.x, 204.f * SCALE.y);
    const auto SIZE_MINI_PREVIEW = ImVec2(256.f * SCALE.x, 145.f * SCALE.y);
    const auto SIZE_LIST = ImVec2(780 * SCALE.x, 414.f * SCALE.y);
    const auto SIZE_PACKAGE = ImVec2(226.f * SCALE.x, 128.f * SCALE.y);
    const auto SIZE_MINI_PACKAGE = ImVec2(170.f * SCALE.x, 96.f * SCALE.y);
    const auto POPUP_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);

    const auto is_background = gui.apps_background.contains("NPXS10015");
    auto &common = emuenv.common_dialog.lang.common;

    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::Begin("##settings", &gui.vita_area.settings, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();

    const auto draw_list = ImGui::GetBackgroundDrawList();
    const ImVec2 BG_POS_MAX(VIEWPORT_POS.x + VIEWPORT_SIZE.x, VIEWPORT_POS.y + VIEWPORT_SIZE.y);
    if (is_background)
        draw_list->AddImage(gui.apps_background["NPXS10015"], VIEWPORT_POS, BG_POS_MAX);
    else
        draw_list->AddRectFilled(VIEWPORT_POS, BG_POS_MAX, IM_COL32(36.f, 120.f, 12.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);

    ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);
    const auto title_size_str = ImGui::CalcTextSize(title.c_str(), 0, false, SIZE_LIST.x);
    ImGui::PushTextWrapPos(((WINDOW_SIZE.x - SIZE_LIST.x) / 2.f) + SIZE_LIST.x);
    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - (title_size_str.x / 2.f), (35.f * SCALE.y) - (title_size_str.y / 2.f)));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", title.c_str());
    ImGui::PopTextWrapPos();

    auto &lang = gui.lang.settings;
    auto &theme_background = lang.theme_background;
    auto &theme = theme_background.theme;
    auto &date_time = lang.date_time;
    auto &language = lang.language;

    if (settings_menu == SettingsMenu::THEME_BACKGROUND) {
        // Search Bar
        if ((menu == Menu::THEME) && selected.empty()) {
            ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
            const auto search_size = ImGui::CalcTextSize(common["search"].c_str());
            ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - (220.f * SCALE.x) - search_size.x, (35.f * SCALE.y) - (search_size.y / 2.f)));
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", common["search"].c_str());
            ImGui::SameLine();
            search_bar.Draw("##search_bar", 200 * SCALE.x);
            ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);

            // Draw Scroll Arrow
            const auto ARROW_SIZE = ImVec2(50.f * SCALE.x, 60.f * SCALE.y);
            const ImU32 ARROW_COLOR = 0xFFFFFFFF; // White

            // Set arrow position of width
            const auto ARROW_WIDTH_POS = VIEWPORT_SIZE.x - (45.f * SCALE.x);
            const auto ARROW_DRAW_WIDTH_POS = VIEWPORT_POS.x + ARROW_WIDTH_POS;
            const auto ARROW_SELECT_WIDTH_POS = ARROW_WIDTH_POS - (ARROW_SIZE.x / 2.f);

            if (current_scroll_pos != 0.f) {
                const auto ARROW_UP_HEIGHT_POS = 135.f * SCALE.y;
                const ImVec2 ARROW_UPP_CENTER(ARROW_DRAW_WIDTH_POS, VIEWPORT_POS.y + ARROW_UP_HEIGHT_POS);
                draw_list->AddTriangleFilled(
                    ImVec2(ARROW_UPP_CENTER.x - (20.f * SCALE.x), ARROW_UPP_CENTER.y + (16.f * SCALE.y)),
                    ImVec2(ARROW_UPP_CENTER.x, ARROW_UPP_CENTER.y - (16.f * SCALE.y)),
                    ImVec2(ARROW_UPP_CENTER.x + (20.f * SCALE.x), ARROW_UPP_CENTER.y + (16.f * SCALE.y)), ARROW_COLOR);
                ImGui::SetCursorPos(ImVec2(ARROW_SELECT_WIDTH_POS, ARROW_UP_HEIGHT_POS - ARROW_SIZE.y));
                if ((ImGui::Selectable("##upp", false, ImGuiSelectableFlags_None, ARROW_SIZE))
                    || (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_leftstick_up)))
                    || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_up)))
                    set_scroll_pos = current_scroll_pos - (340 * SCALE.y);
            }
            if (current_scroll_pos < max_scroll_pos) {
                const auto ARROW_DOWN_HEIGHT_POS = VIEWPORT_SIZE.y - (30.f * SCALE.y);
                const ImVec2 ARROW_DOWN_CENTER(ARROW_DRAW_WIDTH_POS, VIEWPORT_POS.y + ARROW_DOWN_HEIGHT_POS);
                draw_list->AddTriangleFilled(
                    ImVec2(ARROW_DOWN_CENTER.x + (20.f * SCALE.x), ARROW_DOWN_CENTER.y - (16.f * SCALE.y)),
                    ImVec2(ARROW_DOWN_CENTER.x, ARROW_DOWN_CENTER.y + (16.f * SCALE.y)),
                    ImVec2(ARROW_DOWN_CENTER.x - (20.f * SCALE.x), ARROW_DOWN_CENTER.y - (16.f * SCALE.y)), ARROW_COLOR);
                ImGui::SetCursorPos(ImVec2(ARROW_SELECT_WIDTH_POS, ARROW_DOWN_HEIGHT_POS - ARROW_SIZE.y));
                if ((ImGui::Selectable("##down", false, ImGuiSelectableFlags_None, ARROW_SIZE))
                    || (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_leftstick_down)))
                    || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_down)))
                    set_scroll_pos = current_scroll_pos + (340 * SCALE.y);
            }
        }
    }

    ImGui::SetCursorPosY(64.0f * SCALE.y);
    ImGui::Separator();
    ImGui::SetNextWindowPos(ImVec2(WINDOW_POS.x + (WINDOW_SIZE.x / 2.f) - (SIZE_LIST.x / 2.f), WINDOW_POS.y + ((settings_menu == SettingsMenu::THEME_BACKGROUND) && menu != Menu::UNDEFINED ? 86.f : 64.f) * SCALE.y), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
    ImGui::BeginChild("##settings_child", SIZE_LIST, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    const auto SIZE_SELECT = 80.f * SCALE.y;
    const auto SIZE_PUPUP_SELECT = 70.f * SCALE.y;

    // Settings
    switch (settings_menu) {
    case SettingsMenu::SELECT:
        title = lang.main["title"];
        ImGui::SetWindowFontScale(1.2f);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
        if (ImGui::Selectable(theme_background.main["title"].c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT))) {
            get_themes_list(gui, emuenv);
            settings_menu = SettingsMenu::THEME_BACKGROUND;
        }
        ImGui::Separator();
        if (ImGui::Selectable(date_time.main["title"].c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT)))
            settings_menu = SettingsMenu::DATE_TIME;
        ImGui::Separator();
        if (ImGui::Selectable(language.main["title"].c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT)))
            settings_menu = SettingsMenu::LANGUAGE;
        ImGui::PopStyleVar();
        ImGui::Separator();
        break;
    case SettingsMenu::THEME_BACKGROUND: {
        // Themes & Backgrounds
        const auto select = common["select"].c_str();
        switch (menu) {
        case Menu::UNDEFINED: {
            title = theme_background.main["title"];
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
            if (!themes_info.empty()) {
                ImGui::SetWindowFontScale(0.74f);
                const auto CALC_TITLE = ImGui::CalcTextSize(themes_info[gui.users[emuenv.io.user_id].theme_id].title.c_str(), 0, false, 230.f * SCALE.x).y;
                const auto PAD_MARGIN = 40.f * SCALE.y;
                const auto THEME_SELECT_SIZE = CALC_TITLE + PAD_MARGIN > SIZE_SELECT ? CALC_TITLE + PAD_MARGIN : SIZE_SELECT;
                ImGui::SetWindowFontScale(1.2f);
                if (ImGui::Selectable(theme_background.theme.main["title"].c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, THEME_SELECT_SIZE)))
                    menu = Menu::THEME;
                ImGui::SetWindowFontScale(0.74f);
                ImGui::SameLine(0, 420.f * SCALE.x);
                const auto CALC_POS_TITLE = (THEME_SELECT_SIZE / 2.f) - (CALC_TITLE / 2.f);
                ImGui::SetCursorPosY(CALC_POS_TITLE);
                ImGui::TextWrapped("%s", themes_info[gui.users[emuenv.io.user_id].theme_id].title.c_str());
                ImGui::Separator();
            }
            ImGui::SetWindowFontScale(1.2f);
            if (ImGui::Selectable(theme_background.start_screen["title"].c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, 80.f * SCALE.y)))
                menu = Menu::START;
            ImGui::Separator();
            if (ImGui::Selectable(theme_background.home_screen_backgrounds["title"].c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, 80.f * SCALE.y)))
                menu = Menu::BACKGROUND;
            ImGui::PopStyleVar();
            ImGui::Separator();
            break;
        }
        case Menu::THEME: {
            // Theme List
            if (selected.empty()) {
                title = theme.main["title"];

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
                    if (gui.themes_preview[theme.first].contains(PACKAGE))
                        ImGui::Image(gui.themes_preview[theme.first][PACKAGE], SIZE_PACKAGE);
                    ImGui::SetCursorPosY(POS_IMAGE);
                    ImGui::PushID(theme.first.c_str());
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                    ImGui::SetWindowFontScale(1.8f);
                    if (ImGui::Selectable(gui.users[emuenv.io.user_id].theme_id == theme.first ? "V" : "##preview", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                        selected = theme.first;
                    ImGui::SetWindowFontScale(0.6f);
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar();
                    ImGui::PopID();
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + SIZE_PACKAGE.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[theme.first].title.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::NextColumn();
                }
                if (ImGui::Selectable("##download_theme", true, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                    open_path("https://psvt.ovh");
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + SIZE_PACKAGE.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", theme.main["find_a_psvita_custom_themes"].c_str());
                ImGui::PopTextWrapPos();
                ImGui::NextColumn();
                ImGui::Columns(1);
                ImGui::ScrollWhenDragging();
            } else {
                // Theme Select
                title = themes_info[selected].title;
                switch (popup) {
                case Popup::UNDEFINED: {
                    if (gui.themes_preview[selected].contains(HOME)) {
                        ImGui::SetCursorPos(ImVec2(15.f * SCALE.x, (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (72.f * SCALE.y)));
                        ImGui::Image(gui.themes_preview[selected][HOME], SIZE_PREVIEW);
                    }
                    if (gui.themes_preview[selected].contains(LOCK)) {
                        ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) + (15.f * SCALE.y), (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (72.f * SCALE.y)));
                        ImGui::Image(gui.themes_preview[selected][LOCK], SIZE_PREVIEW);
                    }
                    ImGui::SetWindowFontScale(1.2f);
                    ImGui::SetCursorPos(ImVec2((SIZE_LIST.x / 2.f) - (BUTTON_SIZE.x / 2.f), (SIZE_LIST.y - 82.f) - BUTTON_SIZE.y));
                    if ((selected != gui.users[emuenv.io.user_id].theme_id) && (ImGui::Button(select, BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross)))) {
                        gui.users[emuenv.io.user_id].start_path.clear();
                        if (init_theme(gui, emuenv, selected)) {
                            gui.users[emuenv.io.user_id].theme_id = selected;
                            gui.users[emuenv.io.user_id].use_theme_bg = true;
                        } else
                            gui.users[emuenv.io.user_id].use_theme_bg = false;
                        init_theme_start_background(gui, emuenv, selected);
                        gui.users[emuenv.io.user_id].start_type = (selected == "default") ? "default" : "theme";
                        save_user(gui, emuenv, emuenv.io.user_id);
                        set_scroll_pos = current_scroll_pos;
                        selected.clear();
                    }
                    break;
                }
                case Popup::DEL: {
                    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
                    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
                    ImGui::Begin("##delete_theme", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                    ImGui::SetNextWindowPos(ImVec2(WINDOW_SIZE.x / 2.f, WINDOW_SIZE.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
                    ImGui::BeginChild("##delete_theme_popup", POPUP_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
                    ImGui::SetCursorPos(ImVec2(48.f * SCALE.x, 28.f * SCALE.y));
                    ImGui::SetWindowFontScale(1.6f * RES_SCALE.x);
                    ImGui::Image(gui.themes_preview[selected][PACKAGE], SIZE_MINI_PACKAGE);
                    ImGui::SameLine(0, 22.f * SCALE.x);
                    const auto CALC_TITLE = ImGui::CalcTextSize(themes_info[selected].title.c_str(), nullptr, false, POPUP_SIZE.x - SIZE_MINI_PACKAGE.x - (70.f * SCALE.x)).y / 2.f;
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (SIZE_MINI_PACKAGE.y / 2.f) - CALC_TITLE);
                    ImGui::TextWrapped("%s", themes_info[selected].title.c_str());
                    ImGui::SetCursorPosY(POPUP_SIZE.y / 2.f);
                    TextColoredCentered(GUI_COLOR_TEXT, theme.main["delete"].c_str());
                    ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - BUTTON_SIZE.x - (20.f * SCALE.x), POPUP_SIZE.y - BUTTON_SIZE.y - (22.f * SCALE.y)));
                    if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle)))
                        popup = Popup::UNDEFINED;
                    ImGui::SameLine(0, 40.f * SCALE.x);
                    if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross))) {
                        if (selected == gui.users[emuenv.io.user_id].theme_id) {
                            gui.users[emuenv.io.user_id].theme_id = "default";
                            gui.users[emuenv.io.user_id].start_path.clear();
                            if (init_theme(gui, emuenv, "default"))
                                gui.users[emuenv.io.user_id].use_theme_bg = true;
                            else
                                gui.users[emuenv.io.user_id].use_theme_bg = false;
                            save_user(gui, emuenv, emuenv.io.user_id);
                            init_theme_start_background(gui, emuenv, "default");
                        }
                        fs::remove_all(emuenv.pref_path / "ux0/theme" / fs_utils::utf8_to_path(selected));
                        last_updated_themes_list = time(0);
                        if (emuenv.app_path == "NPXS10026")
                            init_content_manager(gui, emuenv);
                        delete_theme = selected;
                        popup = Popup::UNDEFINED;
                        selected.clear();
                        set_scroll_pos = current_scroll_pos;
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar();
                    ImGui::End();
                    break;
                }
                case Popup::INFORMATION: {
                    if (gui.themes_preview[selected].contains(HOME)) {
                        ImGui::SetCursorPos(ImVec2(119.f * SCALE.x, 4.f * SCALE.y));
                        ImGui::Image(gui.themes_preview[selected][HOME], SIZE_MINI_PREVIEW);
                    }
                    if (gui.themes_preview[selected].contains(LOCK)) {
                        ImGui::SetCursorPos(ImVec2(SIZE_LIST.x / 2.f + (15.f * SCALE.y), 4.f * SCALE.y));
                        ImGui::Image(gui.themes_preview[selected][LOCK], SIZE_MINI_PREVIEW);
                    }
                    const auto INFO_POS = ImVec2(280.f * SCALE.x, 30.f * SCALE.y);
                    auto &info = theme.information;
                    ImGui::SetWindowFontScale(0.94f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", info["name"].c_str());
                    ImGui::SameLine();
                    ImGui::PushTextWrapPos(SIZE_LIST.x - (30.f * SCALE.x));
                    ImGui::SetCursorPosX(INFO_POS.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[selected].title.c_str());
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", info["provider"].c_str());
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(INFO_POS.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[selected].provided.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", info["updated"].c_str());
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(INFO_POS.x);
                    auto DATE_TIME = get_date_time(gui, emuenv, themes_info[selected].updated);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s %s", DATE_TIME[DateTime::DATE_MINI].c_str(), DATE_TIME[DateTime::CLOCK].c_str());
                    if (emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR) {
                        ImGui::SameLine();
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s", DATE_TIME[DateTime::DAY_MOMENT].c_str());
                    }
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", info["size"].c_str());
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(INFO_POS.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%zu KB", themes_info[selected].size);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", info["version"].c_str());
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(INFO_POS.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[selected].version.c_str());
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + INFO_POS.y);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", info["content_id"].c_str());
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(INFO_POS.x);
                    ImGui::TextWrapped("%s", selected.c_str());
                    ImGui::ScrollWhenDragging();
                    break;
                }
                }
            }
            break;
        }
        case Menu::START: {
            if (sub_menu == SubMenu::UNDEFINED) {
                title = theme_background.start_screen["title"];
                ImGui::SetWindowFontScale(0.72f);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
                const auto PACKAGE_POS_Y = (SIZE_LIST.y / 2.f) - (SIZE_PACKAGE.y / 2.f) - (72.f * SCALE.y);
                const auto is_not_default = gui.users[emuenv.io.user_id].theme_id != "default";
                if (is_not_default) {
                    const auto THEME_POS = ImVec2(15.f * SCALE.x, PACKAGE_POS_Y);
                    if (gui.themes_preview[gui.users[emuenv.io.user_id].theme_id].contains(PACKAGE)) {
                        ImGui::SetCursorPos(THEME_POS);
                        ImGui::Image(gui.themes_preview[gui.users[emuenv.io.user_id].theme_id][PACKAGE], SIZE_PACKAGE);
                    }
                    ImGui::SetCursorPos(THEME_POS);
                    ImGui::SetWindowFontScale(1.8f);
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                    if (ImGui::Selectable(gui.users[emuenv.io.user_id].start_type == "theme" ? "V" : "##theme", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                        sub_menu = SubMenu::THEME;
                    ImGui::PopStyleColor();
                    ImGui::SetWindowFontScale(0.72f);
                    ImGui::SetCursorPosX(15.f * SCALE.x);
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + SIZE_PACKAGE.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", themes_info[gui.users[emuenv.io.user_id].theme_id].title.c_str());
                    ImGui::PopTextWrapPos();
                }
                const auto IMAGE_POS = ImVec2(is_not_default ? (SIZE_LIST.x / 2.f) - (SIZE_PACKAGE.x / 2.f) : 15.f * SCALE.x, PACKAGE_POS_Y);
                if ((gui.users[emuenv.io.user_id].start_type == "image") && gui.start_background) {
                    ImGui::SetCursorPos(IMAGE_POS);
                    ImGui::Image(gui.start_background, SIZE_PACKAGE);
                }
                ImGui::SetCursorPos(IMAGE_POS);
                if (gui.users[emuenv.io.user_id].start_type == "image")
                    ImGui::SetWindowFontScale(1.8f);
                ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                if (ImGui::Selectable(gui.users[emuenv.io.user_id].start_type == "image" ? "V" : theme_background.start_screen["add_image"].c_str(), false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                    sub_menu = SubMenu::IMAGE;
                ImGui::PopStyleColor();
                ImGui::SetWindowFontScale(0.72f);
                ImGui::SetCursorPosX(IMAGE_POS.x);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", theme_background.start_screen["image"].c_str());
                const auto DEFAULT_POS = ImVec2(is_not_default ? (SIZE_LIST.x / 2.f) + (SIZE_PACKAGE.x / 2.f) + (30.f * SCALE.y) : (SIZE_LIST.x / 2.f) - (SIZE_PACKAGE.x / 2.f), PACKAGE_POS_Y);
                if (gui.themes_preview["default"].contains(PACKAGE)) {
                    ImGui::SetCursorPos(DEFAULT_POS);
                    ImGui::Image(gui.themes_preview["default"][PACKAGE], SIZE_PACKAGE);
                    ImGui::SetCursorPos(DEFAULT_POS);
                    ImGui::SetWindowFontScale(1.8f);
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                    if (ImGui::Selectable(gui.users[emuenv.io.user_id].start_type == "default" ? "V" : "##default", false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                        sub_menu = SubMenu::DEFAULT;
                    ImGui::PopStyleColor();
                    ImGui::SetWindowFontScale(0.72f);
                    ImGui::SetCursorPosX(DEFAULT_POS.x);
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", theme_background.main["default"].c_str());
                }
                ImGui::PopStyleVar();
                ImGui::SetWindowFontScale(0.90f);
            } else {
                const auto START_PREVIEW_POS = ImVec2((SIZE_LIST.x / 2.f) - (SIZE_PREVIEW.x / 2.f), (SIZE_LIST.y / 2.f) - (SIZE_PREVIEW.y / 2.f) - (72.f * SCALE.y));
                const auto SELECT_BUTTON_POS = ImVec2((SIZE_LIST.x / 2.f) - (BUTTON_SIZE.x / 2.f), (SIZE_LIST.y - 82.f) - BUTTON_SIZE.y);
                if (sub_menu == SubMenu::THEME) {
                    title = themes_info[gui.users[emuenv.io.user_id].theme_id].title;
                    if (gui.themes_preview[gui.users[emuenv.io.user_id].theme_id].contains(LOCK)) {
                        ImGui::SetCursorPos(START_PREVIEW_POS);
                        ImGui::Image(gui.themes_preview[gui.users[emuenv.io.user_id].theme_id][LOCK], SIZE_PREVIEW);
                    }
                    ImGui::SetCursorPos(SELECT_BUTTON_POS);
                    if ((gui.users[emuenv.io.user_id].start_type != "theme") && (ImGui::Button(select, BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross)))) {
                        gui.users[emuenv.io.user_id].start_path.clear();
                        gui.users[emuenv.io.user_id].start_type = "theme";
                        init_theme_start_background(gui, emuenv, gui.users[emuenv.io.user_id].theme_id);
                        save_user(gui, emuenv, emuenv.io.user_id);
                        sub_menu = SubMenu::UNDEFINED;
                    }
                } else if (sub_menu == SubMenu::IMAGE) {
                    fs::path image_path{};
                    host::dialog::filesystem::Result result = host::dialog::filesystem::open_file(image_path, { { "Image file", { "bmp", "gif", "jpg", "jpeg", "png", "tif" } } });

                    if ((result == host::dialog::filesystem::Result::SUCCESS) && fs::exists(image_path)) {
                        if (init_user_start_background(gui, fs_utils::path_to_utf8(image_path))) {
                            gui.users[emuenv.io.user_id].start_path = fs_utils::path_to_utf8(image_path);
                            gui.users[emuenv.io.user_id].start_type = "image";
                            save_user(gui, emuenv, emuenv.io.user_id);
                        }
                    } else if (result == host::dialog::filesystem::Result::ERROR) {
                        LOG_CRITICAL("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
                    }
                    sub_menu = SubMenu::UNDEFINED;
                } else if (sub_menu == SubMenu::DEFAULT) {
                    title = theme_background.main["default"];
                    if (gui.themes_preview["default"].contains(LOCK)) {
                        ImGui::SetCursorPos(START_PREVIEW_POS);
                        ImGui::Image(gui.themes_preview["default"][LOCK], SIZE_PREVIEW);
                    }
                    ImGui::SetCursorPos(SELECT_BUTTON_POS);
                    if ((gui.users[emuenv.io.user_id].start_type != "default") && (ImGui::Button(select, BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross)))) {
                        gui.users[emuenv.io.user_id].start_path.clear();
                        init_theme_start_background(gui, emuenv, "default");
                        gui.users[emuenv.io.user_id].start_type = "default";
                        save_user(gui, emuenv, emuenv.io.user_id);
                        sub_menu = SubMenu::UNDEFINED;
                    }
                }
            }
            break;
        }
        case Menu::BACKGROUND: {
            title = theme_background.home_screen_backgrounds["title"];

            // Delete user background
            if (!delete_user_background.empty()) {
                vector_utils::erase_first(gui.users[emuenv.io.user_id].backgrounds, delete_user_background);
                gui.user_backgrounds.erase(delete_user_background);
                gui.user_backgrounds_infos.erase(delete_user_background);
                if (!gui.users[emuenv.io.user_id].backgrounds.empty())
                    gui.current_user_bg = gui.current_user_bg % gui.user_backgrounds.size();
                else if (!gui.theme_backgrounds.empty())
                    gui.users[emuenv.io.user_id].use_theme_bg = true;
                save_user(gui, emuenv, emuenv.io.user_id);
                delete_user_background.clear();
            }

            ImGui::SetWindowFontScale(0.80f);
            ImGui::Columns(3, nullptr, false);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 1.0f));
            for (const auto &background : gui.users[emuenv.io.user_id].backgrounds) {
                const auto IMAGE_POS = ImGui::GetCursorPos();
                const auto PREVIEW_POS = ImVec2(IMAGE_POS.x + (gui.user_backgrounds_infos[background].prev_pos.x * SCALE.x), IMAGE_POS.y + (gui.user_backgrounds_infos[background].prev_pos.y * SCALE.y));
                const auto PREVIEW_SIZE = ImVec2(gui.user_backgrounds_infos[background].prev_size.x * SCALE.x, gui.user_backgrounds_infos[background].prev_size.y * SCALE.y);
                ImGui::SetCursorPos(PREVIEW_POS);
                ImGui::Image(gui.user_backgrounds[background], PREVIEW_SIZE);
                ImGui::SetCursorPosY(IMAGE_POS.y);
                ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_TITLE);
                ImGui::PushID(background.c_str());
                if (ImGui::Selectable(theme_background.home_screen_backgrounds["delete_background"].c_str(), false, ImGuiSelectableFlags_None, SIZE_PACKAGE))
                    delete_user_background = background;
                ImGui::PopID();
                ImGui::PopStyleColor();
                ImGui::NextColumn();
            }
            if (ImGui::Selectable(theme_background.home_screen_backgrounds["add_background"].c_str(), false, ImGuiSelectableFlags_None, SIZE_PACKAGE)) {
                fs::path background_path{};
                host::dialog::filesystem::Result result = host::dialog::filesystem::open_file(background_path, { { "Image file", { "bmp", "gif", "jpg", "jpeg", "png", "tif" } } });
                if (result == host::dialog::filesystem::Result::SUCCESS) {
                    auto background_path_string = fs_utils::path_to_utf8(background_path);
                    if (!gui.user_backgrounds.contains(background_path_string) && fs::exists(background_path) && init_user_background(gui, emuenv, background_path_string)) {
                        gui.users[emuenv.io.user_id].backgrounds.push_back(background_path_string);
                        gui.users[emuenv.io.user_id].use_theme_bg = false;
                        save_user(gui, emuenv, emuenv.io.user_id);
                    }
                } else if (result == host::dialog::filesystem::Result::ERROR) {
                    LOG_CRITICAL("Error initializing file dialog: {}", host::dialog::filesystem::get_error());
                }
            }
            ImGui::PopStyleVar();
            ImGui::Columns(1);
            break;
        }
        }
        break;
    }
    case SettingsMenu::DATE_TIME:
        // Date & Time
        title = date_time.main["title"];
        ImGui::SetWindowFontScale(1.2f);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
        if (ImGui::Selectable(date_time.date_format["title"].c_str(), menu == Menu::DATE_FORMAT, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT))) {
            if (menu == Menu::UNDEFINED)
                menu = Menu::DATE_FORMAT;
            else
                menu = Menu::UNDEFINED;
        }
        ImGui::Separator();
        if (ImGui::Selectable(date_time.time_format["title"].c_str(), menu == Menu::TIME_FORMAT, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT))) {
            if (menu == Menu::UNDEFINED)
                menu = Menu::TIME_FORMAT;
            else
                menu = Menu::UNDEFINED;
        }
        ImGui::Separator();
        ImGui::PopStyleVar();
        if (menu != Menu::UNDEFINED) {
            const auto TIME_SELECT_SIZE = 336.f * SCALE.x;
            ImGui::SetNextWindowPos(ImVec2(WINDOW_POS.x + WINDOW_SIZE.x - TIME_SELECT_SIZE, WINDOW_POS.y), ImGuiCond_Always, ImVec2(0.f, 0.f));
            ImGui::SetNextWindowSize(ImVec2(TIME_SELECT_SIZE, WINDOW_SIZE.y), ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.16f, 0.18f, 1.f));
            ImGui::Begin("##time", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 30.f * SCALE.x);
            ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
            if (menu == Menu::DATE_FORMAT) {
                const auto get_date_format_sting = [&](SceSystemParamDateFormat date_format) {
                    std::string date_format_str;
                    auto &lang = gui.lang.settings.date_time.date_format;
                    switch (date_format) {
                    case SCE_SYSTEM_PARAM_DATE_FORMAT_YYYYMMDD: date_format_str = lang["yyyy_mm_dd"]; break;
                    case SCE_SYSTEM_PARAM_DATE_FORMAT_DDMMYYYY: date_format_str = lang["dd_mm_yyyy"]; break;
                    case SCE_SYSTEM_PARAM_DATE_FORMAT_MMDDYYYY: date_format_str = lang["mm_dd_yyyy"]; break;
                    default: break;
                    }

                    return date_format_str;
                };

                for (auto f = 0; f < 3; f++) {
                    auto date_format_value = static_cast<SceSystemParamDateFormat>(f);
                    const auto date_format_str = get_date_format_sting(date_format_value);
                    ImGui::PushID(date_format_str.c_str());
                    ImGui::SetCursorPosY((WINDOW_SIZE.y / 2.f) - INFORMATION_BAR_HEIGHT - (SIZE_PUPUP_SELECT * 1.5f) + (SIZE_PUPUP_SELECT * f));
                    if (ImGui::Selectable(emuenv.cfg.sys_date_format == date_format_value ? "V" : "##date_format", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT))) {
                        if (emuenv.cfg.sys_date_format != date_format_value) {
                            emuenv.cfg.sys_date_format = date_format_value;
                            config::serialize_config(emuenv.cfg, emuenv.config_path);
                        }
                        menu = Menu::UNDEFINED;
                    }
                    ImGui::NextColumn();
                    ImGui::SetCursorPosY((WINDOW_SIZE.y / 2.f) - INFORMATION_BAR_HEIGHT - (SIZE_PUPUP_SELECT * 1.5f) + (SIZE_PUPUP_SELECT * f));
                    ImGui::Selectable(date_format_str.c_str(), false, ImGuiSelectableFlags_None, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT));
                    ImGui::PopID();
                    ImGui::NextColumn();
                }
            } else if (menu == Menu::TIME_FORMAT) {
                ImGui::SetCursorPosY((WINDOW_SIZE.y / 2.f) - SIZE_PUPUP_SELECT - INFORMATION_BAR_HEIGHT);
                const auto is_12_hour_format = emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR;
                if (ImGui::Selectable(is_12_hour_format ? "V" : "##time_format", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT))) {
                    if (!is_12_hour_format) {
                        emuenv.cfg.sys_time_format = SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR;
                        config::serialize_config(emuenv.cfg, emuenv.config_path);
                    }
                    menu = Menu::UNDEFINED;
                }
                ImGui::NextColumn();
                ImGui::SetCursorPosY((WINDOW_SIZE.y / 2.f) - SIZE_PUPUP_SELECT - INFORMATION_BAR_HEIGHT);
                ImGui::Selectable(date_time.time_format["clock_12_hour"].c_str(), false, ImGuiSelectableFlags_None, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT));
                ImGui::NextColumn();
                if (ImGui::Selectable(!is_12_hour_format ? "V" : "##time_format", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT))) {
                    if (is_12_hour_format) {
                        emuenv.cfg.sys_time_format = SCE_SYSTEM_PARAM_TIME_FORMAT_24HOUR;
                        config::serialize_config(emuenv.cfg, emuenv.config_path);
                    }
                    menu = Menu::UNDEFINED;
                }
                ImGui::NextColumn();
                ImGui::Selectable(date_time.time_format["clock_24_hour"].c_str(), false, ImGuiSelectableFlags_None, ImVec2(TIME_SELECT_SIZE, SIZE_PUPUP_SELECT));
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::PopStyleVar();
            ImGui::End();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                menu = Menu::UNDEFINED;
        }
        break;
    case SettingsMenu::LANGUAGE:
        // Language
        if (menu == Menu::UNDEFINED) {
            title = language.main["title"];
            ImGui::Columns(2, nullptr, false);
            ImGui::SetWindowFontScale(1.2f);
            const auto sys_lang_str = language.main["system_language"].c_str();
            const auto sys_lang_str_size = ImGui::CalcTextSize(sys_lang_str).x;
            ImGui::SetColumnWidth(0, sys_lang_str_size + 40.f);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
            if (ImGui::Selectable(sys_lang_str, false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT))) {
                if (popup == Popup::UNDEFINED)
                    popup = Popup::SELECT_SYS_LANG;
                else
                    popup = Popup::UNDEFINED;
            }
            ImGui::PopStyleVar();
            ImGui::NextColumn();
            ImGui::SetWindowFontScale(0.8f);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(1.f, 0.5f));
            ImGui::Selectable(get_sys_lang_name(emuenv.cfg.sys_lang).c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT));
            ImGui::PopStyleVar();
            ImGui::SetWindowFontScale(1.2f);
            ImGui::Separator();
            ImGui::NextColumn();
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
            if (ImGui::Selectable(language.input_language["title"].c_str(), false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT)))
                menu = Menu::SELECT_INPUT_LANG;
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
            if (popup == Popup::SELECT_SYS_LANG) {
                const auto SYS_LANG_SIZE = WINDOW_SIZE.x / 2.f;
                ImGui::SetNextWindowPos(ImVec2(WINDOW_POS.x + WINDOW_SIZE.x - SYS_LANG_SIZE, WINDOW_POS.y), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(SYS_LANG_SIZE, WINDOW_SIZE.y), ImGuiCond_Always);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.16f, 0.18f, 1.f));
                ImGui::Begin("##system_language", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 30.f * SCALE.x);
                ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
                for (const auto &sys_lang : LIST_SYS_LANG) {
                    ImGui::PushID(sys_lang.first);
                    const auto is_current_lang = emuenv.cfg.sys_lang == sys_lang.first;
                    if (ImGui::Selectable(is_current_lang ? "V" : "##lang", false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(SYS_LANG_SIZE, SIZE_PUPUP_SELECT))) {
                        if (!is_current_lang) {
                            emuenv.cfg.sys_lang = sys_lang.first;
                            config::serialize_config(emuenv.cfg, emuenv.config_path);
                            lang::init_lang(gui.lang, emuenv);
                            if (sys_lang.first != gui.app_selector.apps_cache_lang) {
                                std::thread init_app([&gui, &emuenv]() {
                                    get_sys_apps_title(gui, emuenv);
                                    get_user_apps_title(gui, emuenv);
                                    init_last_time_apps(gui, emuenv);
                                });
                                init_app.detach();
                            }
                            const auto live_area_state = get_live_area_current_open_apps_list_index(gui, "NPXS10015") != gui.live_area_current_open_apps_list.end();
                            gui.live_area_current_open_apps_list.clear();
                            gui.live_area_contents.clear();
                            gui.live_items.clear();
                            init_notice_info(gui, emuenv);
                            if (live_area_state) {
                                update_live_area_current_open_apps_list(gui, emuenv, "NPXS10015");
                                init_live_area(gui, emuenv, "NPXS10015");
                            }
                        }
                        popup = Popup::UNDEFINED;
                    }
                    ImGui::NextColumn();
                    ImGui::Selectable(sys_lang.second.c_str(), false, ImGuiSelectableFlags_None, ImVec2(SYS_LANG_SIZE, SIZE_PUPUP_SELECT));
                    ImGui::PopID();
                    ImGui::NextColumn();
                    ImGui::ScrollWhenDragging();
                }
                ImGui::Columns(1);
                ImGui::PopStyleVar();
                ImGui::End();
                ImGui::PopStyleColor();
                if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    popup = Popup::UNDEFINED;
            }
        } else {
            // Input Languages
            const auto keyboards_str = language.keyboards["title"].c_str();
            if (sub_menu == SubMenu::UNDEFINED) {
                title = language.input_language["title"];
                ImGui::SetWindowFontScale(1.2f);
                ImGui::Columns(2, nullptr, false);
                ImGui::SetColumnWidth(0, 600.f * emuenv.manual_dpi_scale);
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                if (ImGui::Selectable(keyboards_str, false, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0.f, SIZE_SELECT)))
                    sub_menu = SubMenu::SELECT_KEYBOARDS;
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
                    ImGui::SetColumnWidth(0, 40.f * emuenv.manual_dpi_scale);
                    ImGui::SetColumnWidth(1, 560.f * emuenv.manual_dpi_scale);
                    for (const auto &lang : emuenv.ime.lang.ime_keyboards) {
                        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                        ImGui::PushID(lang.first);
                        ImGui::SetWindowFontScale(1.f);
                        const auto is_lang_enable = vector_utils::contains(emuenv.cfg.ime_langs, static_cast<uint64_t>(lang.first));
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
                    ImGui::ScrollWhenDragging();
                } else {
                    SceImeLanguage lang_select = static_cast<SceImeLanguage>(string_utils::stoi_def(selected, 0, "language"));
                    title = get_ime_lang_index(emuenv.ime, lang_select)->second;
                    ImGui::SetWindowFontScale(1.2f);
                    ImGui::Columns(2, nullptr, false);
                    ImGui::SetColumnWidth(0, 650.f * SCALE.x);
                    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.f, 0.5f));
                    ImGui::Selectable(title.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.f, SIZE_SELECT));
                    ImGui::PopStyleVar();
                    ImGui::NextColumn();
                    auto ime_lang_cfg_index = std::find(emuenv.cfg.ime_langs.begin(), emuenv.cfg.ime_langs.end(), lang_select);
                    auto is_ime_lang_enable = ime_lang_cfg_index != emuenv.cfg.ime_langs.end();
                    ImGui::SetWindowFontScale(1.4f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (16.f * SCALE.y));
                    if (ImGui::Checkbox("##lang", &is_ime_lang_enable)) {
                        if (ime_lang_cfg_index != emuenv.cfg.ime_langs.end()) {
                            emuenv.cfg.ime_langs.erase(ime_lang_cfg_index);
                            if (emuenv.cfg.ime_langs.empty())
                                emuenv.cfg.current_ime_lang = SCE_IME_LANGUAGE_ENGLISH_US;
                            else if (emuenv.cfg.current_ime_lang == lang_select)
                                emuenv.cfg.current_ime_lang = emuenv.cfg.ime_langs.back();
                        } else {
                            emuenv.cfg.ime_langs.push_back(lang_select);

                            // Sort Ime lang in good order
                            if (emuenv.cfg.ime_langs.size() > 1) {
                                std::vector<uint64_t> cfg_ime_langs_temp;
                                for (const auto &[lang_id, _] : emuenv.ime.lang.ime_keyboards) {
                                    if (vector_utils::contains(emuenv.cfg.ime_langs, (uint64_t)lang_id))
                                        cfg_ime_langs_temp.push_back(lang_id);
                                }
                                emuenv.cfg.ime_langs = std::move(cfg_ime_langs_temp);
                            }
                        }
                        config::serialize_config(emuenv.cfg, emuenv.config_path);
                    }
                    ImGui::NextColumn();
                    ImGui::Columns(1);
                }
            }
        }
        break;
    default:
        break;
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Back
    ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
    ImGui::SetCursorPos(ImVec2(6.f * SCALE.x, WINDOW_SIZE.y - (52.f * SCALE.y)));
    if (ImGui::Button("<<", ImVec2(64.f * SCALE.x, 40.f * SCALE.y)) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle))) {
        if (settings_menu != SettingsMenu::SELECT) {
            if (menu != Menu::UNDEFINED) {
                if (!selected.empty()) {
                    if (popup != Popup::UNDEFINED)
                        popup = Popup::UNDEFINED;
                    else {
                        selected.clear();
                        set_scroll_pos = current_scroll_pos;
                    }
                } else if (sub_menu != SubMenu::UNDEFINED)
                    sub_menu = SubMenu::UNDEFINED;

                else
                    menu = Menu::UNDEFINED;
            } else if (popup != Popup::UNDEFINED)
                popup = Popup::UNDEFINED;
            else
                settings_menu = SettingsMenu::SELECT;
        } else
            close_system_app(gui, emuenv);
    }

    if ((settings_menu == SettingsMenu::THEME_BACKGROUND) && !selected.empty() && (selected != "default")) {
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - (70.f * SCALE.x), WINDOW_SIZE.y - (52.f * SCALE.y)));
        if (((popup != Popup::INFORMATION) && ImGui::Button("...", ImVec2(64.f * SCALE.x, 40.f * SCALE.y))) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_triangle)))
            ImGui::OpenPopup("...");
        if (ImGui::BeginPopup("...")) {
            if (ImGui::MenuItem(theme.information["title"].c_str()))
                popup = Popup::INFORMATION;
            if (ImGui::MenuItem(common["delete"].c_str()))
                popup = Popup::DEL;
            ImGui::EndPopup();
        }
    }
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace gui
