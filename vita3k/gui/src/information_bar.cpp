// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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
#include <display/state.h>
#include <gui/functions.h>
#include <io/state.h>
#include <util/safe_time.h>

#include <SDL3/SDL_power.h>

namespace gui {

void draw_information_bar(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

    const ImVec2 INFO_BAR_SIZE(VIEWPORT_SIZE.x, 32.f * SCALE.y);

    constexpr ImU32 DEFAULT_INDICATOR_BLACK_COLOR = 0xFF010101; // Black
    constexpr ImU32 DEFAULT_INDICATOR_WHITE_COLOR = 0xFFE3E3E3; // White

    const auto is_12_hour_format = emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR;
    const auto is_notif_pos = !gui.vita_area.start_screen && gui.vita_area.home_screen || gui.vita_area.live_area_screen ? 78.f * SCALE.x : 0.f;
    const auto is_theme_color = gui.vita_area.home_screen || gui.vita_area.live_area_screen || gui.vita_area.start_screen;

    const auto BAR_COLOR = is_theme_color ? gui.information_bar_color.bar : DEFAULT_INDICATOR_BLACK_COLOR;
    const auto INDICATOR_COLOR = is_theme_color ? gui.information_bar_color.indicator : DEFAULT_INDICATOR_WHITE_COLOR;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, VIEWPORT_POS.y + INFO_BAR_SIZE.y), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::Begin("##information_bar", &gui.vita_area.information_bar, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(3);

    const ImVec2 INFO_BAR_POS_MAX(VIEWPORT_POS.x + INFO_BAR_SIZE.x, VIEWPORT_POS.y + INFO_BAR_SIZE.y);
    const auto draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(VIEWPORT_POS, INFO_BAR_POS_MAX, BAR_COLOR, 0.f, ImDrawFlags_RoundCornersAll);

    if (gui.vita_area.home_screen) {
        const auto HOME_ICON_POS_CENTER = VIEWPORT_POS.x + (INFO_BAR_SIZE.x / 2.f) - (32.f * (static_cast<float>(gui.live_area_current_open_apps_list.size()) / 2.f)) * SCALE.x;
        const auto APP_IS_OPEN = gui.live_area_app_current_open >= 0;

        // Draw Home Icon
        const std::vector<ImVec2> HOME_UP_POS = {
            ImVec2(HOME_ICON_POS_CENTER - (13.f * SCALE.x), VIEWPORT_POS.y + (16.f * SCALE.y)),
            ImVec2(HOME_ICON_POS_CENTER, VIEWPORT_POS.y + (6.f * SCALE.y)),
            ImVec2(HOME_ICON_POS_CENTER + (13.f * SCALE.x), VIEWPORT_POS.y + (16.f * SCALE.y))
        };
        const auto HOME_DOWN_POS_MINI = ImVec2(HOME_ICON_POS_CENTER - (8.f * SCALE.x), VIEWPORT_POS.y + (16.f * SCALE.y));
        const auto HOME_DOWN_POS_MAX = ImVec2(HOME_ICON_POS_CENTER + (8.f * SCALE.x), HOME_DOWN_POS_MINI.y + (10.f * SCALE.y));

        draw_list->AddTriangleFilled(HOME_UP_POS[0], HOME_UP_POS[1], HOME_UP_POS[2], INDICATOR_COLOR);
        draw_list->AddRectFilled(HOME_DOWN_POS_MINI, HOME_DOWN_POS_MAX, INDICATOR_COLOR);
        if (APP_IS_OPEN) {
            draw_list->AddTriangleFilled(HOME_UP_POS[0], HOME_UP_POS[1], HOME_UP_POS[2], IM_COL32(0.f, 0.f, 0.f, 100.f));
            draw_list->AddRectFilled(HOME_DOWN_POS_MINI, HOME_DOWN_POS_MAX, IM_COL32(0.f, 0.f, 0.f, 100.f));
        }
        draw_list->AddRectFilled(ImVec2(HOME_ICON_POS_CENTER - (3.f * SCALE.x), VIEWPORT_POS.y + (18.5f * SCALE.y)), ImVec2(HOME_ICON_POS_CENTER + (3.f * SCALE.x), VIEWPORT_POS.y + (26.f * SCALE.y)), BAR_COLOR);

        // Draw App Icon
        const float decal_app_icon_pos = 34.f * ((static_cast<float>(gui.live_area_current_open_apps_list.size()) - 2) / 2.f);
        const auto ICON_SIZE_SCALE = 28.f * SCALE.x;

        for (auto a = 0; a < gui.live_area_current_open_apps_list.size(); a++) {
            const ImVec2 ICON_POS_MIN(VIEWPORT_POS.x + (INFO_BAR_SIZE.x / 2.f) - (14.f * SCALE.x) - (decal_app_icon_pos * SCALE.x) + (a * (34 * SCALE.x)), VIEWPORT_POS.y + (2.f * SCALE.y));
            const ImVec2 ICON_POS_MAX(ICON_POS_MIN.x + ICON_SIZE_SCALE, ICON_POS_MIN.y + ICON_SIZE_SCALE);
            const ImVec2 ICON_CENTER_POS(ICON_POS_MIN.x + (ICON_SIZE_SCALE / 2.f), ICON_POS_MIN.y + (ICON_SIZE_SCALE / 2.f));
            const auto &APPS_OPENED = gui.live_area_current_open_apps_list[a];
            auto &APP_ICON_TYPE = APPS_OPENED.starts_with("NPXS") && (APPS_OPENED != "NPXS10007") ? gui.app_selector.sys_apps_icon : gui.app_selector.user_apps_icon;

            // Check if icon exist
            if (APP_ICON_TYPE.contains(APPS_OPENED))
                draw_list->AddImageRounded(APP_ICON_TYPE[APPS_OPENED], ICON_POS_MIN, ICON_POS_MAX, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, ICON_SIZE_SCALE, ImDrawFlags_RoundCornersAll);
            else
                draw_list->AddCircleFilled(ICON_CENTER_POS, ICON_SIZE_SCALE / 2.f, IM_COL32_WHITE);

            // hide Icon no opened
            if (!APP_IS_OPEN || (gui.live_area_current_open_apps_list[gui.live_area_app_current_open] != APPS_OPENED))
                draw_list->AddCircleFilled(ICON_CENTER_POS, ICON_SIZE_SCALE / 2.f, IM_COL32(0.f, 0.f, 0.f, 140.f));
        }
    }

    constexpr auto PIX_FONT_SCALE = 19.2f / 24.f;
    const auto DEFAULT_FONT_SCALE = ImGui::GetFontSize() / (19.2f * emuenv.manual_dpi_scale);
    const auto CLOCK_DEFAULT_FONT_SCALE = (24.f * emuenv.manual_dpi_scale) * DEFAULT_FONT_SCALE;
    const auto DAY_MOMENT_DEFAULT_FONT_SCALE = (18.f * emuenv.manual_dpi_scale) * DEFAULT_FONT_SCALE;
    const auto CLOCK_FONT_SIZE_SCALE = CLOCK_DEFAULT_FONT_SCALE / ImGui::GetFontSize();
    const auto DAY_MOMENT_FONT_SIZE_SCALE = DAY_MOMENT_DEFAULT_FONT_SCALE / ImGui::GetFontSize();

    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);

    tm local = {};
    SAFE_LOCALTIME(&tt, &local);

    auto DATE_TIME = get_date_time(gui, emuenv, local);
    const auto CALC_CLOCK_SIZE = ImGui::CalcTextSize(DATE_TIME[DateTime::CLOCK].c_str());
    const auto CLOCK_SIZE_SCALE = ImVec2((CALC_CLOCK_SIZE.x * CLOCK_FONT_SIZE_SCALE) * RES_SCALE.x, (CALC_CLOCK_SIZE.y * CLOCK_FONT_SIZE_SCALE * PIX_FONT_SCALE) * RES_SCALE.y);
    const auto CALC_DAY_MOMENT_SIZE = ImGui::CalcTextSize(DATE_TIME[DateTime::DAY_MOMENT].c_str());
    const auto DAY_MOMENT_SIZE_SCALE = emuenv.io.user_id.empty() || is_12_hour_format ? ImVec2((CALC_DAY_MOMENT_SIZE.x * DAY_MOMENT_FONT_SIZE_SCALE) * RES_SCALE.y, (CALC_DAY_MOMENT_SIZE.y * DAY_MOMENT_FONT_SIZE_SCALE * PIX_FONT_SCALE) * RES_SCALE.y) : ImVec2(0.f, 0.f);

    const ImVec2 CLOCK_POS(VIEWPORT_POS.x + INFO_BAR_SIZE.x - (64.f * SCALE.x) - CLOCK_SIZE_SCALE.x - DAY_MOMENT_SIZE_SCALE.x - is_notif_pos, VIEWPORT_POS.y + (INFO_BAR_SIZE.y / 2.f) - (CLOCK_SIZE_SCALE.y / 2.f));
    const auto DAY_MOMENT_POS = ImVec2(CLOCK_POS.x + CLOCK_SIZE_SCALE.x + (6.f * SCALE.x), CLOCK_POS.y + (CLOCK_SIZE_SCALE.y - DAY_MOMENT_SIZE_SCALE.y));

    // Draw clock
    draw_list->AddText(gui.vita_font[emuenv.current_font_level], CLOCK_DEFAULT_FONT_SCALE * RES_SCALE.x, CLOCK_POS, INDICATOR_COLOR, DATE_TIME[DateTime::CLOCK].c_str());
    if (emuenv.io.user_id.empty() || is_12_hour_format)
        draw_list->AddText(gui.vita_font[emuenv.current_font_level], DAY_MOMENT_DEFAULT_FONT_SCALE * RES_SCALE.x, DAY_MOMENT_POS, INDICATOR_COLOR, DATE_TIME[DateTime::DAY_MOMENT].c_str());

    // Set full size and position of battery
    const auto FULL_BATTERY_SIZE = 38.f * SCALE.x;
    const auto BATTERY_HEIGHT_MIN_POS = VIEWPORT_POS.y + (5.f * SCALE.y);
    const ImVec2 BATTERY_MAX_POS(VIEWPORT_POS.x + INFO_BAR_SIZE.x - (12.f * SCALE.x) - is_notif_pos, BATTERY_HEIGHT_MIN_POS + (22 * SCALE.y));
    const ImVec2 BATTERY_BASE_MIN_POS(BATTERY_MAX_POS.x - FULL_BATTERY_SIZE - (4.f * SCALE.x), VIEWPORT_POS.y + (12.f * SCALE.y));
    const ImVec2 BATTERY_BASE_MAX_POS(BATTERY_BASE_MIN_POS.x + (4.f * SCALE.x), BATTERY_BASE_MIN_POS.y + (8 * SCALE.y));

    // Draw battery background
    constexpr ImU32 BATTERY_BG_COLOR = IM_COL32(128.f, 124.f, 125.f, 255.f);
    draw_list->AddRectFilled(BATTERY_BASE_MIN_POS, BATTERY_BASE_MAX_POS, BATTERY_BG_COLOR);
    draw_list->AddRectFilled(ImVec2(BATTERY_MAX_POS.x - FULL_BATTERY_SIZE, BATTERY_HEIGHT_MIN_POS), BATTERY_MAX_POS, BATTERY_BG_COLOR, 2.f * SCALE.x, ImDrawFlags_RoundCornersAll);

    // Set default battery size
    auto battery_size = FULL_BATTERY_SIZE;

    // Get battery level and adjust size accordingly to the battery level
    int32_t res;
    SDL_GetPowerInfo(NULL, &res);
    if ((res >= 0) && (res <= 75)) {
        if (res <= 25)
            battery_size *= 25.f;
        else if (res <= 50)
            battery_size *= 50.f;
        else
            battery_size *= 75.f;

        battery_size /= 100.f;
    }

    // Set battery color depending on battery level: red for levels less than or equal to 25% and green for levels above this threshold.
    const ImU32 BATTERY_COLOR = (res >= 0) && (res <= 25) ? IM_COL32(225.f, 50.f, 50.f, 255.f) : IM_COL32(90.f, 200.f, 30.f, 255.f);

    // Draw battery level
    if (battery_size == FULL_BATTERY_SIZE)
        draw_list->AddRectFilled(BATTERY_BASE_MIN_POS, BATTERY_BASE_MAX_POS, BATTERY_COLOR);
    draw_list->AddRectFilled(ImVec2(BATTERY_MAX_POS.x - battery_size, BATTERY_HEIGHT_MIN_POS), BATTERY_MAX_POS, BATTERY_COLOR, 2.f * SCALE.x, ImDrawFlags_RoundCornersAll);

    if (emuenv.display.imgui_render && !gui.vita_area.start_screen && !gui.vita_area.live_area_screen && !gui.vita_area.user_management && !gui.configuration_menu.settings_dialog && !gui.configuration_menu.custom_settings_dialog && !gui.help_menu.vita3k_update && get_sys_apps_state(gui) && (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) || ImGui::IsItemClicked(0)))
        gui.vita_area.information_bar = false;

    ImGui::End();
}

} // namespace gui
