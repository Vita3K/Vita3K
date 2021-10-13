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

#include <gui/imgui_impl_sdl.h>
#include <util/string_utils.h>

#include <SDL.h>

namespace gui {
static void draw_ime_dialog(DialogState &common_dialog, float FONT_SCALE) {
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin("##ime_dialog", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SetWindowFontScale(FONT_SCALE);
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (ImGui::CalcTextSize(common_dialog.ime.title).x / 2.f));
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", common_dialog.ime.title);
    ImGui::Spacing();
    if (common_dialog.ime.multiline) {
        ImGui::InputTextMultiline(
            "",
            common_dialog.ime.text,
            common_dialog.ime.max_length);
    } else {
        ImGui::InputText(
            "",
            common_dialog.ime.text,
            common_dialog.ime.max_length);
    }
    ImGui::SameLine();
    if (ImGui::Button("Submit")) {
        common_dialog.ime.status = SCE_IME_DIALOG_BUTTON_ENTER;
        common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
        common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
        const std::u16string result16 = string_utils::utf8_to_utf16(common_dialog.ime.text);
        memcpy(common_dialog.ime.result, result16.c_str(), result16.length() * sizeof(uint16_t) + 1);
    }
    if (common_dialog.ime.cancelable) {
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            common_dialog.ime.status = SCE_IME_DIALOG_BUTTON_CLOSE;
            common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
            common_dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
        }
    }
    ImGui::End();
}

static void draw_message_dialog(DialogState &common_dialog, float FONT_SCALE, ImVec2 SCALE) {
    const auto display_size = ImGui::GetIO().DisplaySize;
    const ImVec2 BUTTON_SIZE = ImVec2(200.f * SCALE.x, 35.f * SCALE.y);
    const ImVec2 WINDOW_SIZE = ImVec2(display_size.x / 1.7f, display_size.y / 1.5f);
    const ImVec2 PROGRESS_BAR_SIZE = ImVec2(WINDOW_SIZE.x - 120.f * SCALE.x, 7.f * SCALE.y);

    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(WINDOW_SIZE);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, GUI_COMMON_DIALOG_BG);
    ImGui::Begin("##Message Dialog", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::SetCursorPosY(WINDOW_SIZE.y / 2 - 40.f * SCALE.y);
    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(WINDOW_SIZE.x - 50.f * SCALE.x);
    ImGui::SetWindowFontScale(FONT_SCALE);
    ImGui::SetCursorPosX(WINDOW_SIZE.x / 2 - ImGui::CalcTextSize(common_dialog.msg.message.c_str(), 0, false, WINDOW_SIZE.x - 100.f * SCALE.x).x / 2);
    ImGui::Text("%s", common_dialog.msg.message.c_str());
    ImGui::PopTextWrapPos();
    if (common_dialog.msg.has_progress_bar) {
        ImGui::Spacing();
        const char dummy_buf[32] = "";
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, GUI_PROGRESS_BAR_BG);
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - PROGRESS_BAR_SIZE.x / 2);
        ImGui::ProgressBar(common_dialog.msg.bar_percent / 100.f, PROGRESS_BAR_SIZE, dummy_buf);
        ImGui::PopStyleColor(2);
        std::string progress = std::to_string(static_cast<int>(common_dialog.msg.bar_percent)).append("%");
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - ImGui::CalcTextSize(progress.c_str()).x / 2);
        ImGui::Text("%s", progress.c_str());
    }
    ImGui::EndGroup();
    if (common_dialog.msg.btn_num != 0) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
        const auto buttons_width = common_dialog.msg.btn_num == 2 ? BUTTON_SIZE.x : BUTTON_SIZE.x / 2;
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x / 2 - buttons_width, WINDOW_SIZE.y - 50 * SCALE.y));
        ImGui::BeginGroup();
        for (int i = 0; i < common_dialog.msg.btn_num; i++) {
            if (ImGui::Button(common_dialog.msg.btn[i].c_str(), BUTTON_SIZE)) {
                common_dialog.msg.status = common_dialog.msg.btn_val[i];
                common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
                common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
            }
            ImGui::SameLine();
        }
        ImGui::PopStyleVar();
        ImGui::EndGroup();
    }
    ImGui::End();
    ImGui::PopStyleColor();
}

static void draw_trophy_setup_dialog(DialogState &common_dialog, float FONT_SCALE, ImVec2 SCALE) {
    int timer = (static_cast<int64_t>(common_dialog.trophy.tick) - static_cast<int64_t>(SDL_GetTicks())) / 1000;
    if (timer > 0) {
        const auto display_size = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
        ImGui::Begin("##preparing_app", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        const auto WINDOW_SIZE = ImVec2(764.f * SCALE.x, 440.f * SCALE.y);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
        ImGui::SetNextWindowPos(ImVec2(display_size.x / 2.f, display_size.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::BeginChild("##preparing_app_child", WINDOW_SIZE, false, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        const auto str = !common_dialog.lang.trophy["preparing_start_app"].empty() ? common_dialog.lang.trophy["preparing_start_app"].c_str() : "Preparing to start the application...";
        ImGui::SetWindowFontScale(1.2f * FONT_SCALE);
        const auto str_size = ImGui::CalcTextSize(str);
        const auto STR_POS = ImVec2((WINDOW_SIZE.x / 2.f) - (str_size.x / 2.f), (WINDOW_SIZE.y / 2.f) - (str_size.y / 2.f));
        ImGui::SetCursorPos(STR_POS);
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", str);
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::End();
    } else {
        common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
        common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
    }
}

static std::string get_save_date_time(GuiState &gui, HostState &host, const SceDateTime &date_time) {
    std::string date_str;
    switch (gui.users[host.io.user_id].date_format) {
    case DateFormat::YYYY_MM_DD:
        date_str = fmt::format("{}/{}/{}", date_time.year, date_time.month, date_time.day);
        break;
    case DateFormat::DD_MM_YYYY:
        date_str = fmt::format("{}/{}/{}", date_time.day, date_time.month, date_time.year);
        break;
    case DateFormat::MM_DD_YYYY:
        date_str = fmt::format("{}/{}/{}", date_time.month, date_time.day, date_time.year);
        break;
    default: break;
    }

    return date_str + fmt::format(" {}:{:0>2d}", date_time.hour, date_time.minute);
}

static void draw_save_info(GuiState &gui, HostState &host, float FONT_SCALE, ImVec2 SCALE, int loop_index, const ImTextureID &texture) {
    const ImVec2 THUMBNAIL_SIZE = ImVec2(160.f * SCALE.x, 90.f * SCALE.y);
    const auto display_size = ImGui::GetIO().DisplaySize;
    const ImVec2 WINDOW_SIZE = ImVec2(display_size.x / 1.7f, display_size.y / 1.5f);
    auto lang = host.common_dialog.lang.save_data;
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(WINDOW_SIZE);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, GUI_COMMON_DIALOG_BG);
    ImGui::Begin("##Save Info Dialog", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::SetWindowFontScale(1.2f * FONT_SCALE);
    if (ImGui::Button("X", ImVec2(40 * SCALE.x, 30 * SCALE.y))) {
        host.common_dialog.savedata.button_id = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
        host.common_dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
        host.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
        host.common_dialog.savedata.draw_info_window = false;
    }
    ImGui::SameLine();
    ImGui::SetCursorPosX(WINDOW_SIZE.x / 2 - ImGui::CalcTextSize(host.common_dialog.savedata.list_title.c_str()).x / 2);
    ImGui::Text("%s", host.common_dialog.savedata.list_title.c_str());

    if (!host.common_dialog.savedata.icon_buffer[loop_index].empty()) {
        ImGui::SetCursorPos(ImVec2(50 * SCALE.x, THUMBNAIL_SIZE.y / 2 + 20 * SCALE.y));
        ImGui::Image(texture, THUMBNAIL_SIZE);
    }
    ImGui::SameLine();
    ImGui::Text("%s", host.common_dialog.savedata.title[host.common_dialog.savedata.selected_save].c_str());
    ImGui::SetCursorPosY(WINDOW_SIZE.y / 2);
    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(WINDOW_SIZE.x - 75 * SCALE.x);
    ImGui::SetCursorPosX(50 * SCALE.x);
    ImGui::Text("%s", !lang["updated"].empty() ? lang["updated"].c_str() : "Updated");
    ImGui::SameLine();
    ImGui::SetCursorPosX(WINDOW_SIZE.x / 2 - 85 * SCALE.x);
    ImGui::Text("%s", get_save_date_time(gui, host, host.common_dialog.savedata.date[host.common_dialog.savedata.selected_save]).c_str());
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::SetCursorPosX(50 * SCALE.x);
    ImGui::Text("%s", !lang["details"].empty() ? lang["details"].c_str() : "Details");
    ImGui::SameLine();
    ImGui::SetCursorPosX(WINDOW_SIZE.x / 2 - 85 * SCALE.x);
    ImGui::Text("%s", host.common_dialog.savedata.details[host.common_dialog.savedata.selected_save].c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
    ImGui::SetCursorPosY(WINDOW_SIZE.y - 41 * SCALE.y);
    if (ImGui::Button("Back", ImVec2(62 * SCALE.x, 30 * SCALE.y))) {
        host.common_dialog.savedata.draw_info_window = false;
    }
    ImGui::End();
}

static void draw_savedata_dialog_list(GuiState &gui, HostState &host, float FONT_SCALE, ImVec2 SCALE, ImVec2 WINDOW_SIZE, ImVec2 THUMBNAIL_SIZE, int loop_index, int save_index, std::vector<ImTextureID> &thumbnails_textures) {
    char selectable_buffer[32];
    char info_button_buffer[32];
    sprintf(selectable_buffer, "###New Saved Data %d", loop_index);

    ImGui::SetCursorPos(ImVec2(15 * SCALE.x, (save_index * THUMBNAIL_SIZE.y) + 2 * SCALE.y));
    if (ImGui::Selectable(selectable_buffer, false, ImGuiSelectableFlags_None, ImVec2(WINDOW_SIZE.x - 120 * SCALE.x, THUMBNAIL_SIZE.y - 5 * SCALE.y))) {
        host.common_dialog.savedata.selected_save = loop_index;
        host.common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
        host.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
        host.common_dialog.savedata.mode_to_display = SCE_SAVEDATA_DIALOG_MODE_FIXED;
    }
    ImGui::SameLine();
    ImGui::SetWindowFontScale(1.2f * FONT_SCALE);
    if (!host.common_dialog.savedata.title[loop_index].empty()) {
        ImGui::SetCursorPosX(THUMBNAIL_SIZE.x + 15 * SCALE.x);
        ImGui::Text("%s", host.common_dialog.savedata.title[loop_index].c_str());
    }
    ImGui::SetWindowFontScale(1.f * FONT_SCALE);
    switch (host.common_dialog.savedata.list_style) {
    case SCE_SAVEDATA_DIALOG_LIST_ITEM_STYLE_TITLE_DATE_SUBTITLE:
        if (host.common_dialog.savedata.has_date[loop_index]) {
            ImGui::SetCursorPos(ImVec2(THUMBNAIL_SIZE.x + 15 * SCALE.x, (save_index * THUMBNAIL_SIZE.y) + 28 * SCALE.y));
            ImGui::Text("%s", get_save_date_time(gui, host, host.common_dialog.savedata.date[loop_index]).c_str());
        }
        if (!host.common_dialog.savedata.subtitle[loop_index].empty()) {
            ImGui::SetCursorPos(ImVec2(THUMBNAIL_SIZE.x + 15 * SCALE.x, (save_index * THUMBNAIL_SIZE.y) + 50 * SCALE.y));
            ImGui::Text("%s", host.common_dialog.savedata.subtitle[loop_index].c_str());
        }
        break;
    case SCE_SAVEDATA_DIALOG_LIST_ITEM_STYLE_TITLE_SUBTITLE_DATE:
        if (!host.common_dialog.savedata.subtitle[loop_index].empty()) {
            ImGui::SetCursorPos(ImVec2(THUMBNAIL_SIZE.x + 15 * SCALE.x, (save_index * THUMBNAIL_SIZE.y) + 28 * SCALE.y));
            ImGui::Text("%s", host.common_dialog.savedata.subtitle[loop_index].c_str());
        }
        if (host.common_dialog.savedata.has_date[loop_index]) {
            ImGui::SetCursorPos(ImVec2(THUMBNAIL_SIZE.x + 15 * SCALE.x, (save_index * THUMBNAIL_SIZE.y) + 50 * SCALE.y));
            ImGui::Text("%s", get_save_date_time(gui, host, host.common_dialog.savedata.date[loop_index]).c_str());
        }
        break;
    case SCE_SAVEDATA_DIALOG_LIST_ITEM_STYLE_TITLE_DATE:
        if (host.common_dialog.savedata.has_date[loop_index]) {
            ImGui::SetCursorPos(ImVec2(THUMBNAIL_SIZE.x + 15 * SCALE.x, (save_index * THUMBNAIL_SIZE.y) + 28 * SCALE.y));
            ImGui::Text("%s", get_save_date_time(gui, host, host.common_dialog.savedata.date[loop_index]).c_str());
        }
        break;
    }
    ImGui::SetWindowFontScale(1.2f * FONT_SCALE);
    ImGui::SameLine();
    if (host.common_dialog.savedata.icon_loaded[loop_index]) {
        thumbnails_textures[loop_index] = load_image(gui, (const char *)host.common_dialog.savedata.icon_buffer[loop_index].data(),
            static_cast<int>(host.common_dialog.savedata.icon_buffer[loop_index].size()));
        host.common_dialog.savedata.icon_loaded[loop_index] = false;
    }
    if (!host.common_dialog.savedata.icon_buffer[loop_index].empty()) {
        ImGui::SetCursorPos(ImVec2(10 * SCALE.x, save_index * THUMBNAIL_SIZE.y));
        ImGui::Image(thumbnails_textures[loop_index], THUMBNAIL_SIZE);
    }
    ImGui::SameLine();
    if (host.common_dialog.savedata.slot_info[loop_index].isExist == 1) {
        sprintf(info_button_buffer, "##info %d", loop_index);
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - 74 * SCALE.x, (save_index * THUMBNAIL_SIZE.y) + 25 * SCALE.y));
        if (ImGui::Button(info_button_buffer, ImVec2(20 * SCALE.x, 25 * SCALE.y))) {
            host.common_dialog.savedata.draw_info_window = true;
            host.common_dialog.savedata.selected_save = loop_index;
        }
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - 66 * SCALE.x, (save_index * THUMBNAIL_SIZE.y) + 26 * SCALE.y));
        ImGui::Text("i");
    }
}

static void draw_savedata_dialog(GuiState &gui, HostState &host, float FONT_SCALE, ImVec2 SCALE) {
    const ImVec2 BUTTON_SIZE = ImVec2(200.f * SCALE.x, 35.f * SCALE.y);
    const ImVec2 THUMBNAIL_SIZE = ImVec2(160.f * SCALE.x, 90.f * SCALE.y);
    const ImVec2 PROGRESS_BAR_SIZE = ImVec2(300.f * SCALE.x, 7.f * SCALE.y);
    const ImVec2 WINDOW_SIZE = ImVec2(ImGui::GetIO().DisplaySize.x / 1.7f, ImGui::GetIO().DisplaySize.y / 1.5f);

    int existing_saves_count = 0;
    static std::vector<ImTextureID> thumbnails_textures{ nullptr };

    switch (host.common_dialog.savedata.mode_to_display) {
    case SCE_SAVEDATA_DIALOG_MODE_LIST:
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(WINDOW_SIZE);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, GUI_COMMON_DIALOG_BG);
        ImGui::Begin("##Savedata Dialog", nullptr, ImGuiWindowFlags_NoDecoration);
        ImGui::SetWindowFontScale(1.2f * FONT_SCALE);
        if (ImGui::Button("X", ImVec2(40 * SCALE.x, 30 * SCALE.y))) {
            host.common_dialog.savedata.button_id = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
            host.common_dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
            host.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
        }
        ImGui::SameLine();
        ImGui::SetCursorPosX(WINDOW_SIZE.x / 2 - ImGui::CalcTextSize(host.common_dialog.savedata.list_title.c_str()).x / 2);
        ImGui::Text("%s", host.common_dialog.savedata.list_title.c_str());
        ImGui::SetWindowFontScale(0.9f);
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::BeginChild("##Selectables", ImVec2(0, 0), false, ImGuiWindowFlags_NoDecoration);
        thumbnails_textures.resize(host.common_dialog.savedata.slot_list_size);
        for (std::uint32_t i = 0; i < host.common_dialog.savedata.slot_list_size; i++) {
            switch (host.common_dialog.savedata.display_type) {
            case SCE_SAVEDATA_DIALOG_TYPE_SAVE:
                draw_savedata_dialog_list(gui, host, FONT_SCALE, SCALE, WINDOW_SIZE, THUMBNAIL_SIZE, i, i, thumbnails_textures);
                break;
            case SCE_SAVEDATA_DIALOG_TYPE_LOAD:
            case SCE_SAVEDATA_DIALOG_TYPE_DELETE:
                if (host.common_dialog.savedata.slot_info[i].isExist == 1) {
                    draw_savedata_dialog_list(gui, host, FONT_SCALE, SCALE, WINDOW_SIZE, THUMBNAIL_SIZE, i, existing_saves_count, thumbnails_textures);
                    existing_saves_count++;
                }
                break;
            }
        }
        if (host.common_dialog.savedata.draw_info_window) {
            draw_save_info(gui, host, FONT_SCALE, SCALE, host.common_dialog.savedata.selected_save, thumbnails_textures[host.common_dialog.savedata.selected_save]);
            return;
        }
        if (host.common_dialog.savedata.display_type != SCE_SAVEDATA_DIALOG_TYPE_SAVE) {
            if (existing_saves_count == 0) {
                ImGui::SetWindowFontScale(1.2f);
                const auto no_save_data = !host.common_dialog.lang.save_data["no_saved_data"].empty() ? host.common_dialog.lang.save_data["no_saved_data"].c_str() : "There is no saved data.";
                ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x / 2 - ImGui::CalcTextSize(no_save_data).x / 2 - 10, WINDOW_SIZE.y / 2 - ImGui::CalcTextSize(no_save_data).y / 2 - 25));
                ImGui::Text(no_save_data);
            }
        }
        ImGui::EndChild();
        ImGui::End();
        ImGui::PopStyleColor();
        break;
    default:
    case SCE_SAVEDATA_DIALOG_MODE_FIXED:
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(WINDOW_SIZE);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, GUI_COMMON_DIALOG_BG);
        ImGui::Begin("##Savedata Dialog", nullptr, ImGuiWindowFlags_NoDecoration);
        ImGui::SetWindowFontScale(1.2f * FONT_SCALE);

        static ImTextureID THUMBNAIL_TEXTURE = nullptr;
        if (host.common_dialog.savedata.icon_loaded[0]) {
            THUMBNAIL_TEXTURE = load_image(gui, (const char *)host.common_dialog.savedata.icon_buffer[host.common_dialog.savedata.selected_save].data(),
                static_cast<int>(host.common_dialog.savedata.icon_buffer[host.common_dialog.savedata.selected_save].size()));
            host.common_dialog.savedata.icon_loaded[0] = false;
        }

        if (!host.common_dialog.savedata.icon_buffer[host.common_dialog.savedata.selected_save].empty()) {
            ImGui::SetCursorPos(ImVec2(36 * SCALE.x, 25 * SCALE.y));
            ImGui::Image(THUMBNAIL_TEXTURE, THUMBNAIL_SIZE);
        }
        ImGui::SameLine();
        ImGui::BeginGroup();
        if (!host.common_dialog.savedata.title[host.common_dialog.savedata.selected_save].empty()) {
            ImGui::Text("%s", host.common_dialog.savedata.title[host.common_dialog.savedata.selected_save].c_str());
        }
        ImGui::SetWindowFontScale(1.f * FONT_SCALE);
        if (host.common_dialog.savedata.has_date[host.common_dialog.savedata.selected_save]) {
            ImGui::Text("%s", get_save_date_time(gui, host, host.common_dialog.savedata.date[host.common_dialog.savedata.selected_save]).c_str());
        }
        if (!host.common_dialog.savedata.subtitle.empty()) {
            ImGui::Text("%s", host.common_dialog.savedata.subtitle[host.common_dialog.savedata.selected_save].c_str());
        }
        ImGui::EndGroup();
        ImGui::SetWindowFontScale(1.2f * FONT_SCALE);
        ImGui::PushTextWrapPos(WINDOW_SIZE.x - 50.f * SCALE.x);
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x / 2 - ImGui::CalcTextSize(host.common_dialog.savedata.msg.c_str(), 0, false, WINDOW_SIZE.x - 100.f).x / 2, WINDOW_SIZE.y / 2 + 10));
        ImGui::Text("%s", host.common_dialog.savedata.msg.c_str());
        ImGui::PopTextWrapPos();
        if (host.common_dialog.savedata.has_progress_bar) {
            ImGui::Spacing();
            const char dummy_buf[32] = "";
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, GUI_PROGRESS_BAR_BG);
            ImGui::SetCursorPosX(WINDOW_SIZE.x / 2 - PROGRESS_BAR_SIZE.x / 2);
            ImGui::ProgressBar(host.common_dialog.savedata.bar_percent / 100.f, PROGRESS_BAR_SIZE, dummy_buf);
            ImGui::PopStyleColor(2);
            std::string progress = std::to_string(static_cast<int>(host.common_dialog.savedata.bar_percent)).append("%");
            ImGui::SetCursorPosX(WINDOW_SIZE.x / 2 - ImGui::CalcTextSize(progress.c_str()).x / 2);
            ImGui::Text("%s", progress.c_str());
        }

        if (host.common_dialog.savedata.btn_num != 0) {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
            const auto buttons_width = host.common_dialog.savedata.btn_num == 2 ? BUTTON_SIZE.x : BUTTON_SIZE.x / 2;
            ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x / 2 - buttons_width, WINDOW_SIZE.y - 50 * SCALE.y));
            ImGui::BeginGroup();
            for (int i = 0; i < host.common_dialog.savedata.btn_num; i++) {
                if (ImGui::Button(host.common_dialog.savedata.btn[i].c_str(), BUTTON_SIZE)) {
                    host.common_dialog.savedata.button_id = host.common_dialog.savedata.btn_val[i];
                    host.common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
                    host.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
                }
                ImGui::SameLine();
            }
            ImGui::PopStyleVar();
            ImGui::EndGroup();
        }
        ImGui::End();
        ImGui::PopStyleColor();
        break;
    }
}

void draw_common_dialog(GuiState &gui, HostState &host) {
    ImGui::PushFont(gui.vita_font);
    const auto RES_SCALE = ImVec2(ImGui::GetIO().DisplaySize.x / host.res_width_dpi_scale, ImGui::GetIO().DisplaySize.y / host.res_height_dpi_scale);
    const auto SCALE = ImVec2(RES_SCALE.x * host.dpi_scale, RES_SCALE.y * host.dpi_scale);
    if (host.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING) {
        switch (host.common_dialog.type) {
        case IME_DIALOG:
            draw_ime_dialog(host.common_dialog, RES_SCALE.x);
            break;
        case MESSAGE_DIALOG:
            draw_message_dialog(host.common_dialog, RES_SCALE.x, SCALE);
            break;
        case TROPHY_SETUP_DIALOG:
            draw_trophy_setup_dialog(host.common_dialog, RES_SCALE.x, SCALE);
            break;
        case SAVEDATA_DIALOG:
            draw_savedata_dialog(gui, host, RES_SCALE.x, SCALE);
            break;
        default:
            break;
        }
    }
    ImGui::PopFont();
}

} // namespace gui
