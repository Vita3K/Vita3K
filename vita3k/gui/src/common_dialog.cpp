// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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
static void draw_ime_dialog(DialogState &common_dialog) {
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin(common_dialog.ime.title.c_str());
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
        std::string result = common_dialog.ime.text;
        std::u16string result16 = string_utils::utf8_to_utf16(result);
        memcpy(common_dialog.ime.result, result16.c_str(), result16.size() * sizeof(uint16_t));
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

static void draw_message_dialog(DialogState &common_dialog) {
    const ImVec2 BUTTON_SIZE = ImVec2(200.f, 35.f);
    const ImVec2 WINDOW_SIZE = ImVec2(ImGui::GetIO().DisplaySize.x / 1.7f, ImGui::GetIO().DisplaySize.y / 1.5f);
    const ImVec2 PROGRESS_BAR_SIZE = ImVec2(WINDOW_SIZE.x - 120.f, 7.f);

    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(WINDOW_SIZE);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, GUI_COMMON_DIALOG_BG);
    ImGui::Begin("##Message Dialog", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::SetCursorPosY(WINDOW_SIZE.y / 2 - 40);
    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(WINDOW_SIZE.x - 50.f);
    ImGui::SetCursorPosX(WINDOW_SIZE.x / 2 - ImGui::CalcTextSize(common_dialog.msg.message.c_str(), 0, false, WINDOW_SIZE.x - 100.f).x / 2);
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
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
        const auto buttons_width = common_dialog.msg.btn_num == 2 ? BUTTON_SIZE.x : BUTTON_SIZE.x / 2;
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x / 2 - buttons_width, WINDOW_SIZE.y - 50));
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

static void draw_trophy_setup_dialog(DialogState &common_dialog) {
    int timer = (static_cast<int64_t>(common_dialog.trophy.tick) - static_cast<int64_t>(SDL_GetTicks())) / 1000;
    if (timer > 0) {
        ImGui::SetNextWindowPos(ImVec2(30, 30));
        ImGui::SetNextWindowSize(ImVec2(0, 0));
        ImGui::Begin("Trophy system ready");
        ImGui::Text("Trophy dialog setup correctly");
        std::string closeup_text = "This dialog will close automatically in ";
        closeup_text += std::to_string(timer);
        closeup_text += " seconds.";
        ImGui::Text("%s", closeup_text.c_str());
        ImGui::End();
    } else {
        common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
        common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
    }
}

static void draw_save_info(DialogState &common_dialog, GuiState &gui, int loop_index, const ImTextureID &texture) {
    const ImVec2 THUMBNAIL_SIZE = ImVec2(160.f, 90.f);
    const ImVec2 WINDOW_SIZE = ImVec2(ImGui::GetIO().DisplaySize.x / 1.7f, ImGui::GetIO().DisplaySize.y / 1.5f);
    const auto date = common_dialog.savedata.date[common_dialog.savedata.selected_save];

    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(WINDOW_SIZE);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, GUI_COMMON_DIALOG_BG);
    ImGui::Begin("##Save Info Dialog", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::SetWindowFontScale(1.2f);
    if (ImGui::Button("X", ImVec2(40, 30))) {
        common_dialog.savedata.button_id = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
        common_dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
        common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
        common_dialog.savedata.draw_info_window = false;
    }
    ImGui::SameLine();
    ImGui::SetCursorPosX(WINDOW_SIZE.x / 2 - ImGui::CalcTextSize(common_dialog.savedata.list_title.c_str()).x / 2);
    ImGui::Text("%s", common_dialog.savedata.list_title.c_str());

    if (!common_dialog.savedata.icon_buffer[loop_index].empty()) {
        ImGui::SetCursorPos(ImVec2(50, THUMBNAIL_SIZE.y / 2 + 20));
        ImGui::Image(texture, THUMBNAIL_SIZE);
    }
    ImGui::SameLine();
    ImGui::Text("%s", common_dialog.savedata.title[common_dialog.savedata.selected_save].c_str());
    ImGui::SetCursorPosY(WINDOW_SIZE.y / 2);
    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(WINDOW_SIZE.x - 75);
    ImGui::SetCursorPosX(50);
    ImGui::Text("Updated");
    ImGui::SameLine();
    ImGui::SetCursorPosX(WINDOW_SIZE.x / 2 - 85);
    ImGui::Text("%d/%02d/%02d   %02d:%02d", date.year, date.month, date.day, date.hour, date.minute);
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::SetCursorPosX(50);
    ImGui::Text("Details");
    ImGui::SameLine();
    ImGui::SetCursorPosX(WINDOW_SIZE.x / 2 - 85);
    ImGui::Text("%s", common_dialog.savedata.details[common_dialog.savedata.selected_save].c_str());
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
    ImGui::SetCursorPosY(WINDOW_SIZE.y - 41);
    if (ImGui::Button("Back", ImVec2(62, 30))) {
        common_dialog.savedata.draw_info_window = false;
    }
    ImGui::End();
}

static void draw_savedata_dialog_list(DialogState &common_dialog, GuiState &gui, ImVec2 WINDOW_SIZE, ImVec2 THUMBNAIL_SIZE, int loop_index, int save_index, std::vector<ImTextureID> &thumbnails_textures) {
    char selectable_buffer[32];
    char info_button_buffer[32];
    sprintf(selectable_buffer, "###New Saved Data %d", loop_index);

    ImGui::SetCursorPos(ImVec2(15, (save_index * THUMBNAIL_SIZE.y) + 2));
    if (ImGui::Selectable(selectable_buffer, false, ImGuiSelectableFlags_None, ImVec2(WINDOW_SIZE.x - 120, THUMBNAIL_SIZE.y - 5))) {
        common_dialog.savedata.selected_save = loop_index;
        common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
        common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
        common_dialog.savedata.mode_to_display = SCE_SAVEDATA_DIALOG_MODE_FIXED;
    }
    ImGui::SameLine();
    ImGui::SetWindowFontScale(1.2f);
    if (!common_dialog.savedata.title[loop_index].empty()) {
        ImGui::SetCursorPosX(THUMBNAIL_SIZE.x + 10 + 5);
        ImGui::Text("%s", common_dialog.savedata.title[loop_index].c_str());
    }
    ImGui::SetWindowFontScale(1.f);
    switch (common_dialog.savedata.list_style) {
    case SCE_SAVEDATA_DIALOG_LIST_ITEM_STYLE_TITLE_DATE_SUBTITLE:
        if (common_dialog.savedata.has_date[loop_index]) {
            ImGui::SetCursorPos(ImVec2(THUMBNAIL_SIZE.x + 10 + 5, (save_index * THUMBNAIL_SIZE.y) + 28));
            ImGui::Text("%d/%02d/%02d  %02d:%02d", common_dialog.savedata.date[loop_index].year, common_dialog.savedata.date[loop_index].month,
                common_dialog.savedata.date[loop_index].day, common_dialog.savedata.date[loop_index].hour, common_dialog.savedata.date[loop_index].minute);
        }
        if (!common_dialog.savedata.subtitle[loop_index].empty()) {
            ImGui::SetCursorPos(ImVec2(THUMBNAIL_SIZE.x + 10 + 5, (save_index * THUMBNAIL_SIZE.y) + 50));
            ImGui::Text("%s", common_dialog.savedata.subtitle[loop_index].c_str());
        }
        break;
    case SCE_SAVEDATA_DIALOG_LIST_ITEM_STYLE_TITLE_SUBTITLE_DATE:
        if (!common_dialog.savedata.subtitle[loop_index].empty()) {
            ImGui::SetCursorPos(ImVec2(THUMBNAIL_SIZE.x + 10 + 5, (save_index * THUMBNAIL_SIZE.y) + 28));
            ImGui::Text("%s", common_dialog.savedata.subtitle[loop_index].c_str());
        }
        if (common_dialog.savedata.has_date[loop_index]) {
            ImGui::SetCursorPos(ImVec2(THUMBNAIL_SIZE.x + 10 + 5, (save_index * THUMBNAIL_SIZE.y) + 50));
            ImGui::Text("%d/%02d/%02d  %02d:%02d", common_dialog.savedata.date[loop_index].year, common_dialog.savedata.date[loop_index].month,
                common_dialog.savedata.date[loop_index].day, common_dialog.savedata.date[loop_index].hour, common_dialog.savedata.date[loop_index].minute);
        }
        break;
    case SCE_SAVEDATA_DIALOG_LIST_ITEM_STYLE_TITLE_DATE:
        if (common_dialog.savedata.has_date[loop_index]) {
            ImGui::SetCursorPos(ImVec2(THUMBNAIL_SIZE.x + 10 + 5, (save_index * THUMBNAIL_SIZE.y) + 28));
            ImGui::Text("%d/%02d/%02d  %02d:%02d", common_dialog.savedata.date[loop_index].year, common_dialog.savedata.date[loop_index].month,
                common_dialog.savedata.date[loop_index].day, common_dialog.savedata.date[loop_index].hour, common_dialog.savedata.date[loop_index].minute);
        }
        break;
    }
    ImGui::SetWindowFontScale(1.2f);
    ImGui::SameLine();
    if (common_dialog.savedata.icon_loaded[loop_index]) {
        thumbnails_textures[loop_index] = load_image(gui, (const char *)common_dialog.savedata.icon_buffer[loop_index].data(),
            static_cast<int>(common_dialog.savedata.icon_buffer[loop_index].size()));
        common_dialog.savedata.icon_loaded[loop_index] = false;
    }
    if (!common_dialog.savedata.icon_buffer[loop_index].empty()) {
        ImGui::SetCursorPos(ImVec2(10, save_index * THUMBNAIL_SIZE.y));
        ImGui::Image(thumbnails_textures[loop_index], THUMBNAIL_SIZE);
    }
    ImGui::SameLine();
    if (common_dialog.savedata.slot_info[loop_index].isExist == 1) {
        sprintf(info_button_buffer, "##info %d", loop_index);
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - 74, (save_index * THUMBNAIL_SIZE.y) + 25));
        if (ImGui::Button(info_button_buffer, ImVec2(20, 25))) {
            common_dialog.savedata.draw_info_window = true;
            common_dialog.savedata.selected_save = loop_index;
        }
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - 66, (save_index * THUMBNAIL_SIZE.y) + 26));
        ImGui::Text("i");
    }
}

static void draw_savedata_dialog(DialogState &common_dialog, GuiState &gui) {
    const ImVec2 BUTTON_SIZE = ImVec2(200.f, 35.f);
    const ImVec2 THUMBNAIL_SIZE = ImVec2(160.f, 90.f);
    const ImVec2 PROGRESS_BAR_SIZE = ImVec2(300.f, 7.f);
    const ImVec2 WINDOW_SIZE = ImVec2(ImGui::GetIO().DisplaySize.x / 1.7f, ImGui::GetIO().DisplaySize.y / 1.5f);

    int existing_saves_count = 0;
    static std::vector<ImTextureID> thumbnails_textures{ nullptr };

    switch (common_dialog.savedata.mode_to_display) {
    case SCE_SAVEDATA_DIALOG_MODE_LIST:
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(WINDOW_SIZE);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, GUI_COMMON_DIALOG_BG);
        ImGui::Begin("##Savedata Dialog", nullptr, ImGuiWindowFlags_NoDecoration);
        ImGui::SetWindowFontScale(1.2f);
        if (ImGui::Button("X", ImVec2(40, 30))) {
            common_dialog.savedata.button_id = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
            common_dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
            common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
        }
        ImGui::SameLine();
        ImGui::SetCursorPosX(WINDOW_SIZE.x / 2 - ImGui::CalcTextSize(common_dialog.savedata.list_title.c_str()).x / 2);
        ImGui::Text("%s", common_dialog.savedata.list_title.c_str());
        ImGui::SetWindowFontScale(0.9f);
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::BeginChild("##Selectables", ImVec2(0, 0), false, ImGuiWindowFlags_NoDecoration);
        thumbnails_textures.resize(common_dialog.savedata.slot_list_size);
        for (std::uint32_t i = 0; i < common_dialog.savedata.slot_list_size; i++) {
            switch (common_dialog.savedata.display_type) {
            case SCE_SAVEDATA_DIALOG_TYPE_SAVE:
                draw_savedata_dialog_list(common_dialog, gui, WINDOW_SIZE, THUMBNAIL_SIZE, i, i, thumbnails_textures);
                break;
            case SCE_SAVEDATA_DIALOG_TYPE_LOAD:
            case SCE_SAVEDATA_DIALOG_TYPE_DELETE:
                if (common_dialog.savedata.slot_info[i].isExist == 1) {
                    draw_savedata_dialog_list(common_dialog, gui, WINDOW_SIZE, THUMBNAIL_SIZE, i, existing_saves_count, thumbnails_textures);
                    existing_saves_count++;
                }
                break;
            }
        }
        if (common_dialog.savedata.draw_info_window) {
            draw_save_info(common_dialog, gui, common_dialog.savedata.selected_save, thumbnails_textures[common_dialog.savedata.selected_save]);
            return;
        }
        if (common_dialog.savedata.display_type != SCE_SAVEDATA_DIALOG_TYPE_SAVE) {
            if (existing_saves_count == 0) {
                ImGui::SetWindowFontScale(1.2f);
                ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x / 2 - ImGui::CalcTextSize("There is no saved data.").x / 2 - 10, WINDOW_SIZE.y / 2 - ImGui::CalcTextSize("There is no saved data.").y / 2 - 25));
                ImGui::Text("There is no saved data.");
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
        ImGui::SetWindowFontScale(1.2f);

        static ImTextureID THUMBNAIL_TEXTURE = nullptr;
        if (common_dialog.savedata.icon_loaded[0]) {
            THUMBNAIL_TEXTURE = load_image(gui, (const char *)common_dialog.savedata.icon_buffer[common_dialog.savedata.selected_save].data(),
                static_cast<int>(common_dialog.savedata.icon_buffer[common_dialog.savedata.selected_save].size()));
            common_dialog.savedata.icon_loaded[0] = false;
        }

        if (!common_dialog.savedata.icon_buffer[common_dialog.savedata.selected_save].empty()) {
            ImGui::SetCursorPos(ImVec2(36, 25));
            ImGui::Image(THUMBNAIL_TEXTURE, THUMBNAIL_SIZE);
        }
        ImGui::SameLine();
        ImGui::BeginGroup();
        if (!common_dialog.savedata.title[common_dialog.savedata.selected_save].empty()) {
            ImGui::Text("%s", common_dialog.savedata.title[common_dialog.savedata.selected_save].c_str());
        }
        ImGui::SetWindowFontScale(1.f);
        if (common_dialog.savedata.has_date[common_dialog.savedata.selected_save]) {
            ImGui::Text("%d/%02d/%02d  %02d:%02d", common_dialog.savedata.date[common_dialog.savedata.selected_save].year, common_dialog.savedata.date[common_dialog.savedata.selected_save].month,
                common_dialog.savedata.date[common_dialog.savedata.selected_save].day, common_dialog.savedata.date[common_dialog.savedata.selected_save].hour, common_dialog.savedata.date[common_dialog.savedata.selected_save].minute);
        }
        if (!common_dialog.savedata.subtitle.empty()) {
            ImGui::Text("%s", common_dialog.savedata.subtitle[common_dialog.savedata.selected_save].c_str());
        }
        ImGui::EndGroup();
        ImGui::SetWindowFontScale(1.2f);
        ImGui::PushTextWrapPos(WINDOW_SIZE.x - 50.f);
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x / 2 - ImGui::CalcTextSize(common_dialog.savedata.msg.c_str(), 0, false, WINDOW_SIZE.x - 100.f).x / 2, WINDOW_SIZE.y / 2 + 10));
        ImGui::Text("%s", common_dialog.savedata.msg.c_str());
        ImGui::PopTextWrapPos();
        if (common_dialog.savedata.has_progress_bar) {
            ImGui::Spacing();
            const char dummy_buf[32] = "";
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, GUI_PROGRESS_BAR_BG);
            ImGui::SetCursorPosX(WINDOW_SIZE.x / 2 - PROGRESS_BAR_SIZE.x / 2);
            ImGui::ProgressBar(common_dialog.savedata.bar_percent / 100.f, PROGRESS_BAR_SIZE, dummy_buf);
            ImGui::PopStyleColor(2);
            std::string progress = std::to_string(static_cast<int>(common_dialog.savedata.bar_percent)).append("%");
            ImGui::SetCursorPosX(WINDOW_SIZE.x / 2 - ImGui::CalcTextSize(progress.c_str()).x / 2);
            ImGui::Text("%s", progress.c_str());
        }

        if (common_dialog.savedata.btn_num != 0) {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
            const auto buttons_width = common_dialog.savedata.btn_num == 2 ? BUTTON_SIZE.x : BUTTON_SIZE.x / 2;
            ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x / 2 - buttons_width, WINDOW_SIZE.y - 50));
            ImGui::BeginGroup();
            for (int i = 0; i < common_dialog.savedata.btn_num; i++) {
                if (ImGui::Button(common_dialog.savedata.btn[i].c_str(), BUTTON_SIZE)) {
                    common_dialog.savedata.button_id = common_dialog.savedata.btn_val[i];
                    common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
                    common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
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
    if (host.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING) {
        switch (host.common_dialog.type) {
        case IME_DIALOG:
            draw_ime_dialog(host.common_dialog);
            break;
        case MESSAGE_DIALOG:
            draw_message_dialog(host.common_dialog);
            break;
        case TROPHY_SETUP_DIALOG:
            draw_trophy_setup_dialog(host.common_dialog);
            break;
        case SAVEDATA_DIALOG:
            draw_savedata_dialog(host.common_dialog, gui);
            break;
        default:
            break;
        }
    }
    ImGui::PopFont();
}

} // namespace gui
