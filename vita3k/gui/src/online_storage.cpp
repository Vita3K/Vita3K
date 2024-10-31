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

#include <v3kn/account.h>
#include <v3kn/state.h>
#include <v3kn/storage.h>

#include <config/state.h>
#include <dialog/state.h>

#include <gui/functions.h>
#include <util/safe_time.h>

namespace gui {

static std::string get_remaining_str(LangState &lang, const uint64_t remaining) {
    if (remaining > 60)
        return fmt::format(fmt::runtime(lang.online_storage["minutes_left"]), remaining / 60);
    else
        return fmt::format(fmt::runtime(lang.online_storage["seconds_left"]), remaining);
}

void draw_online_storage(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    const auto INDICATOR_SIZE = ImVec2(VIEWPORT_SIZE.x, 32.f * SCALE.y);
    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INDICATOR_SIZE.y);
    const ImVec2 WINDOW_POS(VIEWPORT_POS.x, VIEWPORT_POS.y + INDICATOR_SIZE.y);

    const auto &app_path = emuenv.cfg.show_live_area_screen && (gui.live_area_app_current_open >= 0) ? gui.live_area_current_open_apps_list[gui.live_area_app_current_open] : emuenv.app_path;
    const auto APP_INDEX = get_app_index(gui, app_path);
    const auto &app_titleid = APP_INDEX->title_id;
    const auto &app_title = APP_INDEX->title;
    const auto &app_icon = gui.app_selector.user_apps_icon[app_path];

    auto &storage_state = emuenv.v3kn.storage_state;
    auto &lang = gui.lang.online_storage;
    auto &common_lang = emuenv.common_dialog.lang.common;
    const auto is_12_hour_format = emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::Begin("##cloud_save", &gui.vita_area.online_storage, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    const auto &draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(ImVec2(VIEWPORT_POS.x, VIEWPORT_POS.y + INDICATOR_SIZE.y), ImVec2(VIEWPORT_POS.x + VIEWPORT_SIZE.x, VIEWPORT_POS.y + VIEWPORT_SIZE.y + INDICATOR_SIZE.y), IM_COL32(0, 55, 145, 255));

    const ImVec2 CLOSE_BUTTON_SIZE(46.f * SCALE.x, 46.f * SCALE.y);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.f * SCALE.x);

    ImGui::SetCursorPos(ImVec2(10.f * SCALE.x, 10.f * SCALE.y));
    if (ImGui::Button("X", CLOSE_BUTTON_SIZE)) {
        gui.vita_area.online_storage = false;
        if (gui.live_area_app_current_open >= 0)
            gui.vita_area.live_area_screen = true;
        else
            gui.vita_area.home_screen = true;
    }
    ImGui::PopStyleVar();

    ImGui::SetCursorPosX((WINDOW_SIZE.x - CLOSE_BUTTON_SIZE.x) / 2.f);
    ImGui::SetWindowFontScale(1.85f * RES_SCALE.y);
    const std::string window_title = lang["online_storage"];
    const auto window_title_width = ImGui::CalcTextSize(window_title.c_str()).x;

    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x - window_title_width) / 2.f, ((64.f * SCALE.y) - ImGui::GetFontSize()) / 2.f));
    ImGui::Text("%s", window_title.c_str());
    const auto SEPARATOR_CLOUD_POS_Y = 64.f * SCALE.y;
    const auto LOCATION_SECTION_SIZE_Y = 92.f * SCALE.y;
    ImGui::SetCursorPos(ImVec2(0.f, SEPARATOR_CLOUD_POS_Y));
    ImGui::Separator();
    ImGui::SetCursorPos(ImVec2(54.f * SCALE.x, SEPARATOR_CLOUD_POS_Y + ((LOCATION_SECTION_SIZE_Y - ImGui::GetFontSize()) / 2.f)));

    ImGui::Text("Cloud");

    const auto has_cloud_save = storage_state.savedata_info.contains(SAVE_DATA_TYPE_CLOUD);
    const auto has_local_save = storage_state.savedata_info.contains(SAVE_DATA_TYPE_LOCAL);

    const auto begin_text_pos_x = 182.f * SCALE.x;

    ImGui::SetWindowFontScale(1.68f * RES_SCALE.y);
    const auto app_title_height_size = ImGui::CalcTextSize(app_title.c_str()).y;
    if (has_cloud_save) {
        ImGui::SetCursorPos(ImVec2(begin_text_pos_x, SEPARATOR_CLOUD_POS_Y + (LOCATION_SECTION_SIZE_Y / 2.f) - app_title_height_size));
        ImGui::Text("%s", app_title.c_str());
        const auto &cloud_info = storage_state.savedata_info[SAVE_DATA_TYPE_CLOUD];
        ImGui::SetCursorPos(ImVec2(begin_text_pos_x, SEPARATOR_CLOUD_POS_Y + (LOCATION_SECTION_SIZE_Y / 2.f) + (10.f * SCALE.y)));
        tm updated_at_tm = {};
        SAFE_LOCALTIME(&cloud_info.last_updated, &updated_at_tm);
        auto UPDATED_AT = get_date_time(gui, emuenv, updated_at_tm);
        ImGui::SetWindowFontScale(1.12f * RES_SCALE.y);
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", fmt::format("{} {} {} {}", UPDATED_AT[DateTime::DATE_MINI], UPDATED_AT[DateTime::CLOCK], is_12_hour_format ? UPDATED_AT[DateTime::DAY_MOMENT] : "", get_unit_size(cloud_info.total_size)).c_str());
    } else {
        ImGui::SetCursorPos(ImVec2(begin_text_pos_x, SEPARATOR_CLOUD_POS_Y + ((LOCATION_SECTION_SIZE_Y - ImGui::GetFontSize()) / 2.f)));
        ImGui::Text("-");
    }

    const auto SEPARATOR_BUTTON_POS_Y = SEPARATOR_CLOUD_POS_Y + LOCATION_SECTION_SIZE_Y;

    ImGui::SetCursorPos(ImVec2(0.f, SEPARATOR_BUTTON_POS_Y));
    ImGui::Separator();

    const auto BUTTONS_SECTION_SIZE_Y = 184.f * SCALE.y;
    const auto BUTTONS_SEPARATION = 40.f * SCALE.x;

    ImGui::SetWindowFontScale(1.06f * RES_SCALE.y);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0.08f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.12f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.18f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 15.f * SCALE.x);
    const ImVec2 CLOUD_BUTTON_SIZE(284.f * SCALE.x, 116.f * SCALE.y);
    const auto BUTTONS_POS_Y = SEPARATOR_BUTTON_POS_Y + ((BUTTONS_SECTION_SIZE_Y - CLOUD_BUTTON_SIZE.y) / 2.f);
    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - CLOUD_BUTTON_SIZE.x - (BUTTONS_SEPARATION / 2.f), BUTTONS_POS_Y));
    ImGui::BeginDisabled(!has_local_save);
    if (ImGui::Button(lang["upload"].c_str(), CLOUD_BUTTON_SIZE)) {
        if (has_cloud_save) {
            storage_state.online_storage_state = ONLINE_STORAGE_UPLOAD;
        } else {
            storage_state.online_storage_state = ONLINE_STORAGE_COPYING;
            v3kn::upload_savedata(gui, emuenv, app_titleid);
        }
    }
    ImGui::EndDisabled();
    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) + (BUTTONS_SEPARATION / 2.f), BUTTONS_POS_Y));
    ImGui::BeginDisabled(!has_cloud_save);
    if (ImGui::Button(lang["download"].c_str(), CLOUD_BUTTON_SIZE)) {
        if (has_local_save) {
            storage_state.online_storage_state = ONLINE_STORAGE_DOWNLOAD;
        } else {
            storage_state.online_storage_state = ONLINE_STORAGE_COPYING;
            v3kn::download_savedata(gui, emuenv, app_titleid);
        }
    }
    ImGui::EndDisabled();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    static const auto get_timestamp_ms = []() {
        return duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    };

    if (storage_state.online_storage_state != ONLINE_STORAGE_SELECT) {
        const ImVec2 OVERLAY_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INDICATOR_SIZE.y);
        const ImVec2 OVERLAY_POS(VIEWPORT_POS.x, VIEWPORT_POS.y + INDICATOR_SIZE.y);
        ImGui::SetNextWindowPos(OVERLAY_POS, ImGuiCond_Always);
        ImGui::SetNextWindowSize(OVERLAY_SIZE, ImGuiCond_Always);
        ImGui::Begin("##cloud_save_overlay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        const auto &window_draw_list = ImGui::GetWindowDrawList();

        switch (storage_state.online_storage_state) {
        case ONLINE_STORAGE_UPLOAD:
        case ONLINE_STORAGE_DOWNLOAD: {
            const ImVec2 POPUP_SIZE(868.f * SCALE.x, 478.f * SCALE.y);
            const ImVec2 POPUP_POS(VIEWPORT_POS.x + ((VIEWPORT_SIZE.x - POPUP_SIZE.x) / 2.f), VIEWPORT_POS.y + ((VIEWPORT_SIZE.y - POPUP_SIZE.y) / 2.f));
            ImGui::SetNextWindowPos(POPUP_POS, ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 15.f * SCALE.x);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
            ImGui::BeginChild("##confirm_popup", POPUP_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));

            const ImVec2 ICON_SIZE(76.f * SCALE.x, 76.f * SCALE.y);
            const ImVec2 ICON_POS(50.f * SCALE.x, 26.f * SCALE.y);
            if (app_icon) {
                const ImVec2 ICON_POS_MIN(POPUP_POS.x + ICON_POS.x, POPUP_POS.y + ICON_POS.y);
                const ImVec2 ICON_POS_MAX(ICON_POS_MIN.x + ICON_SIZE.x, ICON_POS_MIN.y + ICON_SIZE.y);
                window_draw_list->AddImageRounded(app_icon, ICON_POS_MIN, ICON_POS_MAX, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, ICON_SIZE.x);
            }
            ImGui::SetWindowFontScale(1.52f * RES_SCALE.y);
            ImGui::SetCursorPos(ImVec2(ICON_POS.x + ICON_SIZE.x + (22.f * SCALE.x), ICON_POS.y + ((ICON_SIZE.y - app_title_height_size) / 2.f)));
            ImGui::Text("%s", app_title.c_str());

            const bool is_upload = (storage_state.online_storage_state == ONLINE_STORAGE_UPLOAD);

            const auto &src_save = is_upload ? storage_state.savedata_info[SAVE_DATA_TYPE_LOCAL] : storage_state.savedata_info[SAVE_DATA_TYPE_CLOUD];
            const auto &dst_save = is_upload ? storage_state.savedata_info[SAVE_DATA_TYPE_CLOUD] : storage_state.savedata_info[SAVE_DATA_TYPE_LOCAL];

            const auto &current_ps_system = emuenv.cfg.pstv_mode ? lang["ps_tv_system"] : lang["ps_vita_system"];
            const auto &src_title = is_upload ? current_ps_system : lang["online_storage"];
            const auto &dst_title = is_upload ? lang["online_storage"] : current_ps_system;

            const auto draw_save_info = [&](const ImVec2 &begin_pos, const std::string &title, const SaveDataInfo &save_info) {
                ImGui::SetCursorPos(begin_pos);
                ImGui::SetWindowFontScale(1.1f * RES_SCALE.y);
                ImGui::Text("%s", title.c_str());
                ImGui::SetCursorPos(ImVec2(begin_pos.x, ImGui::GetCursorPosY() + (15.f * SCALE.y)));
                tm updated_at_tm = {};
                SAFE_LOCALTIME(&save_info.last_updated, &updated_at_tm);
                auto UPDATED_AT = get_date_time(gui, emuenv, updated_at_tm);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", fmt::format("{} {} {}", UPDATED_AT[DateTime::DATE_MINI], UPDATED_AT[DateTime::CLOCK], is_12_hour_format ? UPDATED_AT[DateTime::DAY_MOMENT] : "").c_str());
                ImGui::SetCursorPos(ImVec2(begin_pos.x, ImGui::GetCursorPosY() + (5.f * SCALE.y)));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", get_unit_size(save_info.total_size).c_str());
            };

            const float INFO_START_Y = ICON_POS.y + ICON_SIZE.y + (22.f * SCALE.y);

            // Draw source save info
            const auto SRC_POS_X = 50.f * SCALE.x;
            draw_save_info(ImVec2(SRC_POS_X, INFO_START_Y), src_title, src_save);

            // Draw arrow
            const auto ARROW_COLOR = IM_COL32(161, 162, 165, 255);
            const auto ARROW_BAR_SIZE = ImVec2(42.f * SCALE.x, 8.f * SCALE.y);
            const auto ARROW_HEAD_SIZE = ImVec2(12.f * SCALE.x, 18.f * SCALE.y);
            const ImVec2 ARROW_BAR_POS_MIN(POPUP_POS.x + (POPUP_SIZE.x / 2.f) - (5.f * SCALE.x) - ARROW_BAR_SIZE.x - ARROW_HEAD_SIZE.x, POPUP_POS.y + (132.f * SCALE.y));
            const ImVec2 ARROW_BAR_POS_MAX(ARROW_BAR_POS_MIN.x + ARROW_BAR_SIZE.x, ARROW_BAR_POS_MIN.y + ARROW_BAR_SIZE.y);
            window_draw_list->AddRectFilled(ARROW_BAR_POS_MIN, ARROW_BAR_POS_MAX, ARROW_COLOR);
            const ImVec2 ARROW_HEAD_POS1(ARROW_BAR_POS_MAX.x, ARROW_BAR_POS_MIN.y - ((ARROW_HEAD_SIZE.y - ARROW_BAR_SIZE.y) / 2.f));
            const ImVec2 ARROW_HEAD_POS2(ARROW_BAR_POS_MAX.x + ARROW_HEAD_SIZE.x, ARROW_BAR_POS_MIN.y + (ARROW_BAR_SIZE.y / 2.f));
            const ImVec2 ARROW_HEAD_POS3(ARROW_BAR_POS_MAX.x, ARROW_BAR_POS_MAX.y + ((ARROW_HEAD_SIZE.y - ARROW_BAR_SIZE.y) / 2.f));
            window_draw_list->AddTriangleFilled(ARROW_HEAD_POS1, ARROW_HEAD_POS2, ARROW_HEAD_POS3, ARROW_COLOR);

            // Draw destination save info
            const auto DST_POS_X = (POPUP_SIZE.x / 2.f) + 42.f * SCALE.x;
            draw_save_info(ImVec2(DST_POS_X, INFO_START_Y), dst_title, dst_save);

            ImGui::SetCursorPosY(215.f * SCALE.y);
            ImGui::Separator();
            ImGui::SetWindowFontScale(1.38f * RES_SCALE.y);
            ImGui::SetCursorPos(ImVec2(60.f * SCALE.x, ImGui::GetCursorPosY() + (14.f * SCALE.y)));
            ImGui::Text("%s", lang["overwrite_saved_data"].c_str());
            const auto CONFIRM_BUTTON_SIZE = ImVec2(310.f * SCALE.x, 46.f * SCALE.y);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 15.f * SCALE.x);
            const auto BUTTON_SPACING = 40.f * SCALE.x;
            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - CONFIRM_BUTTON_SIZE.x - (BUTTON_SPACING / 2.f), POPUP_SIZE.y - CONFIRM_BUTTON_SIZE.y - (24.f * SCALE.y)));
            if (ImGui::Button(common_lang["no"].c_str(), CONFIRM_BUTTON_SIZE)) {
                storage_state.online_storage_state = ONLINE_STORAGE_SELECT;
            }
            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) + (BUTTON_SPACING / 2.f), POPUP_SIZE.y - CONFIRM_BUTTON_SIZE.y - (24.f * SCALE.y)));
            if (ImGui::Button(common_lang["yes"].c_str(), CONFIRM_BUTTON_SIZE)) {
                if (storage_state.online_storage_state == ONLINE_STORAGE_UPLOAD)
                    v3kn::upload_savedata(gui, emuenv, app_titleid);
                else
                    v3kn::download_savedata(gui, emuenv, app_titleid);
                storage_state.online_storage_state = ONLINE_STORAGE_COPYING;
            }
            ImGui::PopStyleVar(2);
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            break;
        }
        case ONLINE_STORAGE_COPYING: {
            const ImVec2 ICON_SIZE(64.f * SCALE.x, 64.f * SCALE.y);
            const ImVec2 POPUP_SIZE(760 * SCALE.x, 436.f * SCALE.y);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 15.f * SCALE.x);
            const auto PUPUP_POS = ImVec2(VIEWPORT_POS.x + ((VIEWPORT_SIZE.x - POPUP_SIZE.x) / 2.f), VIEWPORT_POS.y + ((VIEWPORT_SIZE.y - POPUP_SIZE.y) / 2.f));
            ImGui::SetNextWindowPos(PUPUP_POS, ImGuiCond_Always);
            ImGui::BeginChild("##copying_popup", POPUP_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            const auto &copy_str = lang["copying"];
            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x - ImGui::CalcTextSize(copy_str.c_str()).x) / 2.f, 82.f * SCALE.y));
            ImGui::Text("%s", copy_str.c_str());
            const auto ICON_POS = ImVec2(100.f * SCALE.x, 132.f * SCALE.y);
            if (app_icon) {
                const ImVec2 ICON_POS_MIN(PUPUP_POS.x + ICON_POS.x, PUPUP_POS.y + ICON_POS.y);
                const ImVec2 ICON_POS_MAX(ICON_POS_MIN.x + ICON_SIZE.x, ICON_POS_MIN.y + ICON_SIZE.y);
                window_draw_list->AddImageRounded(app_icon, ICON_POS_MIN, ICON_POS_MAX, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, ICON_SIZE.x);
            }
            ImGui::SetCursorPos(ImVec2(ICON_POS.x + ICON_SIZE.x + (12.f * SCALE.x), ICON_POS.y + ((ICON_SIZE.y - ImGui::GetFontSize()) / 2.f)));
            ImGui::Text("%s", app_title.c_str());
            const auto remaining_str = get_remaining_str(gui.lang, storage_state.remaining.load());
            ImGui::SetCursorPos(ImVec2(POPUP_SIZE.x - (100 * SCALE.x) - (ImGui::CalcTextSize(remaining_str.c_str()).x), 214.f * SCALE.y));
            ImGui::Text("%s", remaining_str.c_str());
            const float PROGRESS_BAR_WIDTH = 560.f * SCALE.x;
            ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2) - (PROGRESS_BAR_WIDTH / 2.f), 236.f * SCALE.y));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
            ImGui::ProgressBar(storage_state.progress.load(), ImVec2(PROGRESS_BAR_WIDTH, 14.f * SCALE.y), "");
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (16.f * SCALE.y));
            TextColoredCentered(GUI_COLOR_TEXT, std::to_string(static_cast<uint32_t>(storage_state.progress.load() * 100.f)).append("%").c_str());
            ImGui::PopStyleColor();
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 15.f * SCALE.x);
            const ImVec2 CANCEL_BUTTON_SIZE(320.f * SCALE.x, 46.f * SCALE.y);
            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x - CANCEL_BUTTON_SIZE.x) / 2.f, POPUP_SIZE.y - CANCEL_BUTTON_SIZE.y - (24.f * SCALE.y)));
            ImGui::BeginDisabled(storage_state.progress.load() >= 1.f);
            if (ImGui::Button(emuenv.common_dialog.lang.common["cancel"].c_str(), CANCEL_BUTTON_SIZE)) {
                std::lock_guard<std::mutex> lock(storage_state.progress_state.mutex);
                storage_state.progress_state.canceled = true;
                storage_state.online_storage_state = ONLINE_STORAGE_SELECT;
            }
            ImGui::EndDisabled();

            // Hold the dialog at 100% for a short moment to ensure the user perceives the completion state
            if (storage_state.progress.load() >= 1.f) {
                const auto now = get_timestamp_ms();

                if (storage_state.progress_done_timestamp == 0)
                    storage_state.progress_done_timestamp = now;

                if (now - storage_state.progress_done_timestamp >= 900) {
                    gui.vita_area.please_wait = true;
                    storage_state.please_wait_timestamp = now;
                    storage_state.online_storage_state = ONLINE_STORAGE_SELECT;
                }
            }

            ImGui::PopStyleVar();
            ImGui::EndChild();
            ImGui::PopStyleVar();
            break;
        }
        default:
            break;
        }
        ImGui::End();
    }

    // Hide please wait after a short delay to improve UX
    if (gui.vita_area.please_wait && !storage_state.please_wait_done.load()) {
        const auto now = get_timestamp_ms();

        if (now - storage_state.please_wait_timestamp >= 900) {
            storage_state.please_wait_done.store(true);
            {
                std::lock_guard<std::mutex> lck(storage_state.mutex_progress);
                storage_state.cv_progress.notify_all();
            }
        }
    }

    const auto END_SEPARATOR_POS_Y = SEPARATOR_BUTTON_POS_Y + BUTTONS_SECTION_SIZE_Y;
    ImGui::SetCursorPos(ImVec2(0.f, END_SEPARATOR_POS_Y));
    ImGui::Separator();

    ImGui::SetWindowFontScale(1.85f * RES_SCALE.y);
    ImGui::SetCursorPos(ImVec2(54.f * SCALE.x, END_SEPARATOR_POS_Y + ((LOCATION_SECTION_SIZE_Y - ImGui::GetFontSize()) / 2.f)));
    ImGui::Text("Local");

    ImGui::SetWindowFontScale(1.68f * RES_SCALE.y);
    if (has_local_save) {
        const auto &local_info = storage_state.savedata_info[SAVE_DATA_TYPE_LOCAL];
        ImGui::SetCursorPos(ImVec2(begin_text_pos_x, END_SEPARATOR_POS_Y + (LOCATION_SECTION_SIZE_Y / 2.f) - app_title_height_size));
        ImGui::Text("%s", app_title.c_str());
        ImGui::SetCursorPos(ImVec2(begin_text_pos_x, END_SEPARATOR_POS_Y + (LOCATION_SECTION_SIZE_Y / 2.f) + (10.f * SCALE.y)));
        tm updated_at_tm = {};
        SAFE_LOCALTIME(&local_info.last_updated, &updated_at_tm);
        auto UPDATED_AT = get_date_time(gui, emuenv, updated_at_tm);
        ImGui::SetWindowFontScale(1.12f * RES_SCALE.y);
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", fmt::format("{} {} {} {}", UPDATED_AT[DateTime::DATE_MINI], UPDATED_AT[DateTime::CLOCK], is_12_hour_format ? UPDATED_AT[DateTime::DAY_MOMENT] : "", get_unit_size(local_info.total_size)).c_str());
    } else {
        ImGui::SetCursorPos(ImVec2(begin_text_pos_x, END_SEPARATOR_POS_Y + (LOCATION_SECTION_SIZE_Y - ImGui::GetFontSize()) / 2.f));
        ImGui::Text("-");
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace gui
