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

#include <ctrl/ctrl.h>
#include <gui/functions.h>

#include <config/state.h>
#include <dialog/state.h>
#include <util/string_utils.h>
#include <util/vector_utils.h>

#include <SDL3/SDL_timer.h>

namespace gui {
static void draw_ime_dialog(DialogState &common_dialog, float FONT_SCALE) {
    ImGui::SetNextWindowSize(ImVec2(0, 0));
    ImGui::Begin("##ime_dialog", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SetWindowFontScale(FONT_SCALE);
    TextColoredCentered(GUI_COLOR_TEXT_TITLE, common_dialog.ime.title);
    ImGui::Spacing();
    // TODO: setting the bufsize to max_length + 1 is not correct (except when using only 1-byte UTF-8 characters)
    // the reason being that the max_length is the number of characters allowed but the parameter given to ImGui::InputTextMultiline/ImGui::InputText
    // is the size of the buffer, which includes the ending 0 (+1). However, as characters can have a variable size using UTF-8,
    // this will not necessarily match the max_length (but be at most it)
    if (common_dialog.ime.multiline) {
        ImGui::InputTextMultiline(
            "##ime_dialog_multiline",
            common_dialog.ime.text,
            common_dialog.ime.max_length + 1);
    } else {
        ImGui::InputText(
            "##ime_dialog_singleline",
            common_dialog.ime.text,
            common_dialog.ime.max_length + 1);
    }
    ImGui::SameLine();
    auto &common = common_dialog.lang.common;
    if (ImGui::Button(common["submit"].c_str())) {
        common_dialog.ime.status = SCE_IME_DIALOG_BUTTON_ENTER;
        common_dialog.status = SCE_COMMON_DIALOG_STATUS_FINISHED;
        common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
        const std::u16string result16 = string_utils::utf8_to_utf16(common_dialog.ime.text);
        memcpy(common_dialog.ime.result, result16.c_str(), (result16.length() + 1) * sizeof(uint16_t));
    }
    if (common_dialog.ime.cancelable) {
        ImGui::SameLine();
        if (ImGui::Button(common["cancel"].c_str())) {
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
    ImGui::SetWindowFontScale(FONT_SCALE);
    TextCentered(common_dialog.msg.message.c_str(), 50.f * SCALE.x);
    if (common_dialog.msg.has_progress_bar) {
        ImGui::Spacing();
        const char dummy_buf[32] = "";
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, GUI_PROGRESS_BAR_BG);
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() / 2 - PROGRESS_BAR_SIZE.x / 2);
        ImGui::ProgressBar(common_dialog.msg.bar_percent / 100.f, PROGRESS_BAR_SIZE, dummy_buf);
        ImGui::PopStyleColor(2);
        std::string progress = std::to_string(static_cast<int>(common_dialog.msg.bar_percent)).append("%");
        TextCentered(progress.c_str());
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
        ImGui::BeginChild("##preparing_app_child", WINDOW_SIZE, ImGuiChildFlags_None, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        const auto str = common_dialog.lang.trophy["preparing_start_app"].c_str();
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

static std::string get_save_date_time(SceSystemParamDateFormat date_format, const SceDateTime &date_time) {
    std::string date_str;
    switch (date_format) {
    case SCE_SYSTEM_PARAM_DATE_FORMAT_YYYYMMDD:
        date_str = fmt::format("{}/{}/{}", date_time.year, date_time.month, date_time.day);
        break;
    case SCE_SYSTEM_PARAM_DATE_FORMAT_DDMMYYYY:
        date_str = fmt::format("{}/{}/{}", date_time.day, date_time.month, date_time.year);
        break;
    case SCE_SYSTEM_PARAM_DATE_FORMAT_MMDDYYYY:
        date_str = fmt::format("{}/{}/{}", date_time.month, date_time.day, date_time.year);
        break;
    default: break;
    }

    return date_str + fmt::format(" {}:{:0>2d}", date_time.hour, date_time.minute);
}

enum SaveDataListType {
    CANCEL,
    SLOT,
    INFO,
    BACK,
};

static SaveDataListType save_data_list_type_selected = SLOT;
static int32_t first_visible_save_data_slot = -1, current_selected_save_data_slot = -1;
static std::vector<uint32_t> save_data_slot_list, save_data_slot_list_visible;
static int32_t current_save_data_dialog_button_id = 0;

void browse_save_data_dialog(GuiState &gui, EmuEnvState &emuenv, const uint32_t button) {
    // When user press a button, enable navigation by buttons
    if (!gui.is_nav_button) {
        gui.is_nav_button = true;

        // When the current selected save data slot have not any selected, set it to the first visible
        if (current_selected_save_data_slot < 0)
            current_selected_save_data_slot = first_visible_save_data_slot;

        return;
    }

    const auto confirm = [&]() {
        emuenv.common_dialog.savedata.button_id = emuenv.common_dialog.savedata.btn_val[current_save_data_dialog_button_id];
        emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
        emuenv.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
        current_save_data_dialog_button_id = 0;
    };

    switch (emuenv.common_dialog.savedata.mode_to_display) {
    case SCE_SAVEDATA_DIALOG_MODE_FIXED:
        switch (button) {
        case SCE_CTRL_RIGHT:
            current_save_data_dialog_button_id = std::min<int32_t>(current_save_data_dialog_button_id + 1, emuenv.common_dialog.savedata.btn_num - 1);
            break;
        case SCE_CTRL_LEFT:
            current_save_data_dialog_button_id = std::max(current_save_data_dialog_button_id - 1, 0);
            break;
        case SCE_CTRL_CIRCLE:
            if (emuenv.cfg.sys_button == 0)
                confirm();
            break;
        case SCE_CTRL_CROSS:
            if (emuenv.cfg.sys_button == 1)
                confirm();
            break;
        default: break;
        }
        break;
    case SCE_SAVEDATA_DIALOG_MODE_LIST: {
        const auto save_data_slot_list_size = static_cast<int32_t>(save_data_slot_list.size() - 1);

        // Find current selected save data slot in save data slot list
        const int32_t list_index = vector_utils::find_index(save_data_slot_list, current_selected_save_data_slot);
        if (!emuenv.common_dialog.savedata.draw_info_window && (list_index == -1)) {
            if (save_data_slot_list.empty()) {
                switch (button) {
                case SCE_CTRL_CROSS:
                case SCE_CTRL_CIRCLE:
                    emuenv.common_dialog.savedata.button_id = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
                    emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
                    emuenv.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
                    break;
                }
            } else
                current_selected_save_data_slot = first_visible_save_data_slot;
            return;
        }

        const auto prev_save_data_slot = save_data_slot_list[std::max(list_index - 1, 0)];
        const auto next_save_data_slot = save_data_slot_list[std::min(list_index + 1, save_data_slot_list_size)];
        const auto is_save_exist = (current_selected_save_data_slot >= 0 && current_selected_save_data_slot < emuenv.common_dialog.savedata.slot_info.size() && emuenv.common_dialog.savedata.slot_info[current_selected_save_data_slot].isExist == 1);

        const auto confirm = [&]() {
            switch (save_data_list_type_selected) {
            case CANCEL:
                emuenv.common_dialog.savedata.button_id = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
                emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
                emuenv.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
                emuenv.common_dialog.savedata.draw_info_window = false;
                save_data_list_type_selected = SLOT;
                break;
            case SLOT:
                emuenv.common_dialog.savedata.selected_save = current_selected_save_data_slot;
                emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
                emuenv.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
                emuenv.common_dialog.savedata.mode_to_display = SCE_SAVEDATA_DIALOG_MODE_FIXED;
                break;
            case INFO:
                emuenv.common_dialog.savedata.draw_info_window = true;
                emuenv.common_dialog.savedata.selected_save = current_selected_save_data_slot;
                save_data_list_type_selected = BACK;
                break;
            case BACK:
                save_data_list_type_selected = INFO;
                emuenv.common_dialog.savedata.draw_info_window = false;
                break;
            default: break;
            }
        };

        const auto cancel = [&]() {
            if (emuenv.common_dialog.savedata.draw_info_window) {
                save_data_list_type_selected = INFO;
                emuenv.common_dialog.savedata.draw_info_window = false;
            } else {
                emuenv.common_dialog.savedata.button_id = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
                emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
                emuenv.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
                save_data_list_type_selected = SLOT;
            }
        };

        switch (button) {
        case SCE_CTRL_UP:
            if (emuenv.common_dialog.savedata.draw_info_window || (save_data_slot_list.front() == current_selected_save_data_slot))
                save_data_list_type_selected = CANCEL;
            else
                current_selected_save_data_slot = prev_save_data_slot;
            break;
        case SCE_CTRL_DOWN:
            if (save_data_list_type_selected == CANCEL) {
                if (emuenv.common_dialog.savedata.draw_info_window)
                    save_data_list_type_selected = BACK;
                else {
                    save_data_list_type_selected = SLOT;
                    current_selected_save_data_slot = save_data_slot_list.front();
                }
            } else
                current_selected_save_data_slot = next_save_data_slot;
            break;
        case SCE_CTRL_LEFT:
            if (!emuenv.common_dialog.savedata.draw_info_window)
                save_data_list_type_selected = static_cast<SaveDataListType>(std::max(static_cast<int32_t>(save_data_list_type_selected) - 1, 0));
            break;
        case SCE_CTRL_RIGHT:
            if (!emuenv.common_dialog.savedata.draw_info_window && (save_data_list_type_selected == CANCEL) || ((save_data_list_type_selected == SLOT) && is_save_exist))
                save_data_list_type_selected = static_cast<SaveDataListType>(std::min(static_cast<int32_t>(save_data_list_type_selected) + 1, 2));
            break;
        case SCE_CTRL_CIRCLE:
            if (emuenv.cfg.sys_button == 1)
                cancel();
            else
                confirm();
            break;
        case SCE_CTRL_CROSS:
            if (emuenv.cfg.sys_button == 1)
                confirm();
            else
                cancel();
            break;
        default: break;
        }
        break;
    }
    default: break;
    }
}

static ImTextureID check_and_init_icon_texture(GuiState &gui, EmuEnvState &emuenv, const uint32_t index) {
    auto &icon_texture = emuenv.common_dialog.savedata.icon_texture[index];
    if (!icon_texture && !emuenv.common_dialog.savedata.icon_buffer[index].empty()) {
        icon_texture = load_image(gui, emuenv.common_dialog.savedata.icon_buffer[index].data(),
            static_cast<int>(emuenv.common_dialog.savedata.icon_buffer[index].size()));
    }

    return icon_texture;
}

static void draw_save_info(GuiState &gui, EmuEnvState &emuenv, const ImVec2 WINDOW_SIZE, float FONT_SCALE, ImVec2 SCALE, const ImTextureID icon_texture) {
    const ImVec2 THUMBNAIL_SIZE = ImVec2(160.f * SCALE.x, 90.f * SCALE.y);
    auto &lang = emuenv.common_dialog.lang.save_data.info;

    const ImVec2 ICON_POS(100.f * SCALE.x, 16.f * SCALE.y);
    if (icon_texture) {
        ImGui::SetCursorPos(ICON_POS);
        ImGui::Image(icon_texture, THUMBNAIL_SIZE);
    }

    ImGui::SetWindowFontScale(1.5f * FONT_SCALE);
    ImGui::SetCursorPos(ImVec2(292 * SCALE.x, (16.f * SCALE.y) + (THUMBNAIL_SIZE.y / 2.f) - (ImGui::GetFontSize() / 2.f)));
    ImGui::Text("%s", emuenv.common_dialog.savedata.title[emuenv.common_dialog.savedata.selected_save].c_str());
    ImGui::SetCursorPos(ImVec2(ICON_POS.x, ICON_POS.y + THUMBNAIL_SIZE.y + (20.f * SCALE.y)));
    ImGui::Text("%s", lang["updated"].c_str());
    const auto INFO_POS_WIDTH = 324.f * SCALE.x;
    ImGui::SameLine(INFO_POS_WIDTH);
    ImGui::Text("%s", get_save_date_time((SceSystemParamDateFormat)emuenv.cfg.sys_date_format, emuenv.common_dialog.savedata.date[emuenv.common_dialog.savedata.selected_save]).c_str());
    ImGui::SetCursorPos(ImVec2(ICON_POS.x, ImGui::GetCursorPosY() + (46.f * SCALE.y)));
    ImGui::Text("%s", lang["details"].c_str());
    ImGui::SameLine(INFO_POS_WIDTH);
    const auto &DETAILS_STR = !emuenv.common_dialog.savedata.details[emuenv.common_dialog.savedata.selected_save].empty() ? emuenv.common_dialog.savedata.details[emuenv.common_dialog.savedata.selected_save] : emuenv.common_dialog.savedata.subtitle[emuenv.common_dialog.savedata.selected_save];
    ImGui::Text("%s", DETAILS_STR.c_str());
    const ImVec2 BUTTON_SIZE = ImVec2(64 * SCALE.x, 34 * SCALE.y);
    const ImVec2 BUTTON_POS = ImVec2(6.f * SCALE.x, WINDOW_SIZE.y - (BUTTON_SIZE.y + 14 * SCALE.y));
    ImGui::SetWindowFontScale(1.f * FONT_SCALE);
    ImGui::SetCursorPos(BUTTON_POS);
    if (ImGui::Button("Back", BUTTON_SIZE))
        emuenv.common_dialog.savedata.draw_info_window = false;
    if (gui.is_nav_button) {
        ImGui::SetCursorPos(BUTTON_POS);
        ImGui::Selectable("##back info", save_data_list_type_selected == BACK, ImGuiSelectableFlags_None, BUTTON_SIZE);
    }
}

static void draw_savedata_dialog_list(GuiState &gui, EmuEnvState &emuenv, float FONT_SCALE, ImVec2 SCALE, ImVec2 WINDOW_SIZE, ImVec2 THUMBNAIL_SIZE, int loop_index, int save_index) {
    const ImVec2 save_pos = ImVec2((150.f * SCALE.x) + THUMBNAIL_SIZE.x + (20.f * SCALE.x), (save_index * THUMBNAIL_SIZE.y) + (save_index * (1.f * SCALE.y)));
    const auto SELECT_PADDING = 4.f * SCALE.x;
    const ImVec2 selectable_pos = ImVec2(30.f * SCALE.x, save_pos.y + (SELECT_PADDING / 2.f));
    const auto is_save_data_slot_selected = gui.is_nav_button && (save_data_list_type_selected == SLOT) && (current_selected_save_data_slot == loop_index);
    const auto is_save_data_info_selected = (save_data_list_type_selected == INFO) && (current_selected_save_data_slot == loop_index);
    const auto is_save_exist = emuenv.common_dialog.savedata.slot_info[loop_index].isExist == 1;

    const auto SELECTABLE_WIDTH_SIZE = WINDOW_SIZE.x - (selectable_pos.x * 2.f) - (!gui.is_nav_button && is_save_exist ? (180.f * SCALE.x) : 0);
    ImGui::SetCursorPos(selectable_pos);
    if (ImGui::Selectable("##selectable", is_save_data_slot_selected, ImGuiSelectableFlags_None, ImVec2(SELECTABLE_WIDTH_SIZE, THUMBNAIL_SIZE.y - SELECT_PADDING))) {
        emuenv.common_dialog.savedata.selected_save = loop_index;
        emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
        emuenv.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
        emuenv.common_dialog.savedata.mode_to_display = SCE_SAVEDATA_DIALOG_MODE_FIXED;
    }
    const auto item_rect_max = ImGui::GetItemRectMax().y;
    const auto item_rect_min = ImGui::GetItemRectMin().y;
    const auto WINDOW_HIGHT_POS = ImGui::GetWindowPos().y;
    const auto MAX_LIST_POS = WINDOW_HIGHT_POS + ImGui::GetWindowSize().y;

    // Determine if the element is within the visible area of the window.
    const auto element_is_within_visible_area = (item_rect_max >= ImGui::GetWindowPos().y) && (item_rect_min <= MAX_LIST_POS);
    if (element_is_within_visible_area) {
        save_data_slot_list_visible.push_back(loop_index);

        // Set the current selected save data slot to the current save data slot when the slot is hovered.
        if (!gui.is_nav_button && ImGui::IsItemHovered())
            current_selected_save_data_slot = loop_index;

    } else if (!gui.is_nav_button && (current_selected_save_data_slot == loop_index)) {
        // When the save data slot is selected but not visible, reset the current selected save data slot.
        current_selected_save_data_slot = -1;
    }

    if (is_save_data_slot_selected) {
        // Scroll to the app position when it is not visible.
        if (item_rect_min < WINDOW_HIGHT_POS)
            ImGui::SetScrollHereY(0.f);
        else if (item_rect_max > MAX_LIST_POS)
            ImGui::SetScrollHereY(1.f);
    }

    ImGui::SetWindowFontScale(1.2f * FONT_SCALE);
    ImGui::SetCursorPos(ImVec2(save_pos.x, save_pos.y + (is_save_exist ? 10.f * SCALE.y : (THUMBNAIL_SIZE.y / 2.f) - (ImGui::GetFontSize() / 2.f))));
    ImGui::BeginGroup();
    ImGui::Text("%s", emuenv.common_dialog.savedata.title[loop_index].c_str());
    ImGui::SetWindowFontScale(1.f * FONT_SCALE);
    const auto sys_date_format = (SceSystemParamDateFormat)emuenv.cfg.sys_date_format;
    switch (emuenv.common_dialog.savedata.list_style) {
    case SCE_SAVEDATA_DIALOG_LIST_ITEM_STYLE_TITLE_DATE_SUBTITLE:
        if (emuenv.common_dialog.savedata.has_date[loop_index])
            ImGui::Text("%s", get_save_date_time(sys_date_format, emuenv.common_dialog.savedata.date[loop_index]).c_str());
        if (!emuenv.common_dialog.savedata.subtitle[loop_index].empty())
            ImGui::Text("%s", emuenv.common_dialog.savedata.subtitle[loop_index].c_str());
        break;
    case SCE_SAVEDATA_DIALOG_LIST_ITEM_STYLE_TITLE_SUBTITLE_DATE:
        if (!emuenv.common_dialog.savedata.subtitle[loop_index].empty())
            ImGui::Text("%s", emuenv.common_dialog.savedata.subtitle[loop_index].c_str());
        if (emuenv.common_dialog.savedata.has_date[loop_index])
            ImGui::Text("%s", get_save_date_time(sys_date_format, emuenv.common_dialog.savedata.date[loop_index]).c_str());
        break;
    case SCE_SAVEDATA_DIALOG_LIST_ITEM_STYLE_TITLE_DATE:
        if (emuenv.common_dialog.savedata.has_date[loop_index])
            ImGui::Text("%s", get_save_date_time(sys_date_format, emuenv.common_dialog.savedata.date[loop_index]).c_str());
        break;
    }
    ImGui::EndGroup();
    ImGui::SetWindowFontScale(1.2f * FONT_SCALE);
    const ImVec2 THUMBNAIL_POS(150.f * SCALE.x, save_pos.y);
    const auto icon_texture = check_and_init_icon_texture(gui, emuenv, loop_index);
    if (icon_texture) {
        ImGui::SetCursorPos(THUMBNAIL_POS);
        ImGui::Image(icon_texture, THUMBNAIL_SIZE);
    }
    ImGui::SetCursorPos(ImVec2(THUMBNAIL_POS.x, THUMBNAIL_POS.y + THUMBNAIL_SIZE.y));
    ImGui::Separator();
    if (is_save_exist) {
        const ImVec2 INFO_SIZE(46 * SCALE.x, 46 * SCALE.y);
        const auto INFO_POS = ImVec2(WINDOW_SIZE.x - (194.f * SCALE.x), save_pos.y + (THUMBNAIL_SIZE.y / 2.f) - (INFO_SIZE.y / 2.f));
        ImGui::SetCursorPos(INFO_POS);
        if (ImGui::Button("i", INFO_SIZE)) {
            emuenv.common_dialog.savedata.draw_info_window = true;
            emuenv.common_dialog.savedata.selected_save = loop_index;
        }
        if (gui.is_nav_button) {
            ImGui::SetCursorPos(INFO_POS);
            ImGui::Selectable("##info", is_save_data_info_selected, 0, INFO_SIZE);
        }
    }
}

static void draw_savedata_dialog(GuiState &gui, EmuEnvState &emuenv, float FONT_SCALE, ImVec2 SCALE) {
    const ImVec2 THUMBNAIL_SIZE = ImVec2(160.f * SCALE.x, 90.f * SCALE.y);
    const ImVec2 VIEWPORT_SIZE = ImVec2(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 VIEWPORT_POS = ImVec2(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);

    int existing_saves_count = 0;
    auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
    if (gui.is_nav_button)
        flags |= ImGuiWindowFlags_NoMouseInputs;

    ImGui::SetNextWindowPos(ImVec2(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(VIEWPORT_SIZE);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGui::Begin("##Savedata Dialog", nullptr, flags);

    switch (emuenv.common_dialog.savedata.mode_to_display) {
    case SCE_SAVEDATA_DIALOG_MODE_LIST: {
        save_data_slot_list.clear();
        save_data_slot_list_visible.clear();

        ImGui::SetWindowFontScale(1.56f * FONT_SCALE);
        const ImVec2 CANCEL_BUTTON_POS(20.f * SCALE.x, 42.f * SCALE.y);
        ImGui::SetCursorPos(CANCEL_BUTTON_POS);
        const ImVec2 CANCEL_BUTTON_SIZE(46 * SCALE.x, 46 * SCALE.y);
        if (ImGui::Button("X", CANCEL_BUTTON_SIZE)) {
            emuenv.common_dialog.savedata.button_id = SCE_SAVEDATA_DIALOG_BUTTON_ID_INVALID;
            emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_USER_CANCELED;
            emuenv.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
        }
        if (gui.is_nav_button) {
            const auto PADDING = 4.f * SCALE.y;
            ImGui::SetCursorPos(ImVec2(CANCEL_BUTTON_POS.x, CANCEL_BUTTON_POS.y + (PADDING / 2.f)));
            ImGui::Selectable("##cancel", save_data_list_type_selected == CANCEL, 0, ImVec2(CANCEL_BUTTON_SIZE.x, CANCEL_BUTTON_SIZE.y - PADDING));
        }
        const auto WINDOW_POS = ImVec2(0.f, 96.f * SCALE.y);
        const auto MARGIN = 150.f * SCALE.x;
        const auto TEXT_SIZE = ImGui::CalcTextSize(emuenv.common_dialog.savedata.list_title.c_str());
        const ImVec2 TEXT_POS(TEXT_SIZE.x > (VIEWPORT_SIZE.x - MARGIN) ? MARGIN : (VIEWPORT_SIZE.x / 2.f) - (TEXT_SIZE.x / 2.f), WINDOW_POS.y - TEXT_SIZE.y - (14.f * SCALE.y));
        ImGui::SetCursorPos(TEXT_POS);
        ImGui::Text("%s", emuenv.common_dialog.savedata.list_title.c_str());
        ImGui::SetCursorPosY(95.f * SCALE.y);
        ImGui::Separator();
        ImGui::SetWindowFontScale(1.f);
        const auto WINDOW_SIZE = ImVec2(VIEWPORT_SIZE.x, 448.f * SCALE.y);
        ImGui::SetCursorPos(WINDOW_POS);
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::BeginChild("##selectables", WINDOW_SIZE, ImGuiChildFlags_None, flags);
        if (emuenv.common_dialog.savedata.draw_info_window)
            draw_save_info(gui, emuenv, WINDOW_SIZE, FONT_SCALE, SCALE, emuenv.common_dialog.savedata.icon_texture[emuenv.common_dialog.savedata.selected_save]);
        else {
            for (std::uint32_t i = 0; i < emuenv.common_dialog.savedata.slot_list_size; i++) {
                ImGui::PushID(i);
                if (!emuenv.common_dialog.savedata.title[i].empty()) {
                    draw_savedata_dialog_list(gui, emuenv, FONT_SCALE, SCALE, WINDOW_SIZE, THUMBNAIL_SIZE, i, existing_saves_count);
                    existing_saves_count++;
                    save_data_slot_list.push_back(i);
                }
                ImGui::PopID();
            }

            // Set the first visible save data slot when is not empty
            if (!save_data_slot_list_visible.empty())
                first_visible_save_data_slot = save_data_slot_list_visible.front();

            if (existing_saves_count == 0) {
                save_data_list_type_selected = CANCEL;
                ImGui::SetWindowFontScale(1.56f * FONT_SCALE);
                ImGui::SetCursorPosY(150.f * SCALE.y);
                TextCentered(emuenv.common_dialog.lang.save_data.load["no_saved_data"].c_str());
            }
        }
        ImGui::EndChild();
        break;
    }
    default:
    case SCE_SAVEDATA_DIALOG_MODE_FIXED: {
        const ImVec2 WINDOW_SIZE = ImVec2(760.f * SCALE.x, 440.f * SCALE.y);
        const ImVec2 HALF_WINDOW_SIZE(WINDOW_SIZE.x / 2.f, WINDOW_SIZE.y / 2.f);
        ImGui::SetNextWindowPos(ImVec2(VIEWPORT_POS.x + (VIEWPORT_SIZE.x / 2.f) - HALF_WINDOW_SIZE.x, VIEWPORT_POS.y + (VIEWPORT_SIZE.y / 2.f) - HALF_WINDOW_SIZE.y));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.f * SCALE.x);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, GUI_COMMON_DIALOG_BG);
        ImGui::BeginChild("##save_data_fixed_dialog", WINDOW_SIZE, ImGuiChildFlags_Borders, flags);
        ImGui::SetWindowFontScale(1.2f * FONT_SCALE);

        const ImVec2 ICON_POS(48 * SCALE.x, 34 * SCALE.y);

        // Draw the save data icon
        const auto icon_texture = check_and_init_icon_texture(gui, emuenv, emuenv.common_dialog.savedata.selected_save);
        if (icon_texture) {
            ImGui::SetCursorPos(ImVec2(48 * SCALE.x, 34 * SCALE.y));
            ImGui::Image(icon_texture, THUMBNAIL_SIZE);
        }

        const auto is_save_exist = emuenv.common_dialog.savedata.slot_info[emuenv.common_dialog.savedata.selected_save].isExist == 1;

        // Draw the save data icon, title and date time
        ImGui::SameLine(0, 20 * SCALE.x);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (is_save_exist ? (10.f * SCALE.x) : (THUMBNAIL_SIZE.y / 2.f) - (ImGui::GetFontSize() / 2.f)));
        ImGui::BeginGroup();
        if (!emuenv.common_dialog.savedata.title[emuenv.common_dialog.savedata.selected_save].empty()) {
            ImGui::Text("%s", emuenv.common_dialog.savedata.title[emuenv.common_dialog.savedata.selected_save].c_str());
        }
        ImGui::SetWindowFontScale(1.f * FONT_SCALE);
        if (emuenv.common_dialog.savedata.has_date[emuenv.common_dialog.savedata.selected_save]) {
            ImGui::Text("%s", get_save_date_time((SceSystemParamDateFormat)emuenv.cfg.sys_date_format, emuenv.common_dialog.savedata.date[emuenv.common_dialog.savedata.selected_save]).c_str());
        }
        if (!emuenv.common_dialog.savedata.subtitle.empty()) {
            ImGui::Text("%s", emuenv.common_dialog.savedata.subtitle[emuenv.common_dialog.savedata.selected_save].c_str());
        }
        ImGui::EndGroup();

        ImGui::SetCursorPosY(ICON_POS.y + THUMBNAIL_SIZE.y + (10.f * SCALE.y));
        ImGui::Separator();
        ImGui::SetWindowFontScale(1.25f * FONT_SCALE);
        ImGui::SetCursorPosY((WINDOW_SIZE.y / 2.f) + (20.f * SCALE.y));
        TextCentered(emuenv.common_dialog.savedata.msg.c_str(), 50.f * SCALE.x);
        if (emuenv.common_dialog.savedata.has_progress_bar) {
            ImGui::Spacing();
            const ImVec2 PROGRESS_BAR_SIZE = ImVec2(570.f * SCALE.x, 12.f * SCALE.y);
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, GUI_PROGRESS_BAR_BG);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f * SCALE.x);
            ImGui::SetCursorPosX(HALF_WINDOW_SIZE.x - (PROGRESS_BAR_SIZE.x / 2));
            ImGui::ProgressBar(emuenv.common_dialog.savedata.bar_percent / 100.f, PROGRESS_BAR_SIZE, "");
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();
            const std::string progress = std::to_string(static_cast<int>(emuenv.common_dialog.savedata.bar_percent)).append("%");
            TextCentered(progress.c_str());
        }

        if (emuenv.common_dialog.savedata.btn_num != 0) {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f * SCALE.x);
            const auto BUTTON_SIZE_HEIGHT = 46.f * SCALE.y;
            const ImVec2 buttons_size(320.f * SCALE.x, BUTTON_SIZE_HEIGHT);
            const auto button_height_pos = WINDOW_SIZE.y - BUTTON_SIZE_HEIGHT - (22 * SCALE.y);
            const auto button_width_pos = HALF_WINDOW_SIZE.x - (emuenv.common_dialog.savedata.btn_num == 2 ? (buttons_size.x + (20.f * SCALE.x)) : (buttons_size.x / 2.f));
            ImGui::BeginGroup();
            for (int i = 0; i < emuenv.common_dialog.savedata.btn_num; i++) {
                ImGui::PushID(i);
                const ImVec2 button_id_pos(button_width_pos + (i * (buttons_size.x + (40.f * SCALE.x))), button_height_pos);
                ImGui::SetCursorPos(button_id_pos);
                if (ImGui::Button(emuenv.common_dialog.savedata.btn[i].c_str(), buttons_size)) {
                    emuenv.common_dialog.savedata.button_id = emuenv.common_dialog.savedata.btn_val[i];
                    emuenv.common_dialog.result = SCE_COMMON_DIALOG_RESULT_OK;
                    emuenv.common_dialog.substatus = SCE_COMMON_DIALOG_STATUS_FINISHED;
                }
                if (gui.is_nav_button) {
                    ImGui::SetCursorPos(button_id_pos);
                    ImGui::Selectable("##button_id", (current_save_data_dialog_button_id == i), 0, buttons_size);
                }
                ImGui::PopID();
            }
            ImGui::PopStyleVar();
            ImGui::EndGroup();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        break;
    }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void draw_common_dialog(GuiState &gui, EmuEnvState &emuenv) {
    ImGui::PushFont(gui.vita_font[emuenv.current_font_level]);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    if (emuenv.common_dialog.status == SCE_COMMON_DIALOG_STATUS_RUNNING) {
        switch (emuenv.common_dialog.type) {
        case IME_DIALOG:
            draw_ime_dialog(emuenv.common_dialog, RES_SCALE.x);
            break;
        case MESSAGE_DIALOG:
            draw_message_dialog(emuenv.common_dialog, RES_SCALE.x, SCALE);
            break;
        case TROPHY_SETUP_DIALOG:
            draw_trophy_setup_dialog(emuenv.common_dialog, RES_SCALE.x, SCALE);
            break;
        case SAVEDATA_DIALOG:
            draw_savedata_dialog(gui, emuenv, RES_SCALE.x, SCALE);
            break;
        default:
            break;
        }
    }
    ImGui::PopFont();
}

} // namespace gui
