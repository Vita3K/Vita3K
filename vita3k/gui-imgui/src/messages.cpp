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

#include <gui-imgui/functions.h>

#include <v3kn/account.h>
#include <v3kn/messages.h>
#include <v3kn/state.h>

#include <dialog/state.h>
#include <util/safe_time.h>

#include <algorithm>

namespace gui {

enum class MessageScreen {
    ConversationList,
    AddRecipientMenu,
    SelectFriend,
    EnterOnlineId,
    RecipientsPreview,
    Conversation
};

static MessageScreen message_screen = MessageScreen::ConversationList;
static std::vector<std::string> selected_friends;
static std::vector<std::string> recipients_list;
static char online_id_input[32] = "";
static int selected_recipient_index = -1;
static bool open_add_recipient_popup = false;

void draw_messages(GuiState &gui, EmuEnvState &emuenv) {
    commit_friend_avatars(gui, emuenv);
    const auto &messages_lang = gui.lang.messages_app;
    const auto &friends_lang = gui.lang.friends;
    const auto &viewport = sdl::get_viewport();
    const ImVec2 VIEWPORT_SIZE(viewport.logical_width, viewport.logical_height);
    const ImVec2 VIEWPORT_POS(viewport.logical_x, viewport.logical_y);
    const auto RES_SCALE = ImVec2(viewport.scale_x, viewport.scale_y);
    const auto SCALE = ImVec2(RES_SCALE.x * viewport.manual_dpi_scale, RES_SCALE.y * viewport.manual_dpi_scale);
    const auto INDICATOR_SIZE = 32.f * SCALE.y;

    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INDICATOR_SIZE);
    const ImVec2 WINDOW_POS(VIEWPORT_POS.x, VIEWPORT_POS.y + INDICATOR_SIZE);

    const auto BG_PATH = "NPXS10014";

    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    const auto flags = ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##message", &gui.vita_area.messages, flags);
    ImGui::PopStyleVar();

    const auto bg_draw_list = ImGui::GetBackgroundDrawList();
    const ImVec2 BG_POS_MAX(VIEWPORT_POS.x + VIEWPORT_SIZE.x, VIEWPORT_POS.y + VIEWPORT_SIZE.y);
    if (gui.apps_background.contains(BG_PATH))
        bg_draw_list->AddImage(gui.apps_background[BG_PATH], VIEWPORT_POS, BG_POS_MAX);
    else
        bg_draw_list->AddRectFilled(VIEWPORT_POS, BG_POS_MAX, IM_COL32(36.f, 120.f, 12.f, 255.f), 0.f, ImDrawFlags_RoundCornersAll);

    const auto &draw_list = ImGui::GetWindowDrawList();

    const auto TITLE_BAR_HEIGHT = 64.f * SCALE.y;
    const auto BUTTON_HEIGHT = 70.f * SCALE.y;

    const auto draw_title_bar = [&](const char *title, const bool show_close, const bool show_datetime) {
        draw_list->AddRectFilled(WINDOW_POS, ImVec2(WINDOW_POS.x + WINDOW_SIZE.x, WINDOW_POS.y + TITLE_BAR_HEIGHT), IM_COL32(38, 29, 48, 255), 0.f, ImDrawFlags_RoundCornersTop);

        if (show_close) {
            ImGui::SetCursorPos(ImVec2(10.f * SCALE.x, 10.f * SCALE.y));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.f * SCALE.x);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("X", ImVec2(50.f * SCALE.x, 50.f * SCALE.y))) {
                recipients_list.clear();
                selected_friends.clear();
                selected_recipient_index = -1;
                open_add_recipient_popup = false;
                message_screen = MessageScreen::ConversationList;
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }

        ImGui::SetWindowFontScale(1.8f * RES_SCALE.y);
        const auto title_size = ImGui::CalcTextSize(title);
        ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x - title_size.x) / 2.f, (TITLE_BAR_HEIGHT - title_size.y) / 2.f));
        ImGui::Text("%s", title);

        if (show_datetime) {
            ImGui::SetWindowFontScale(1.0f * RES_SCALE.y);
            const auto now = std::time(nullptr);
            tm date_tm = {};
            SAFE_LOCALTIME(&now, &date_tm);
            const auto date_time = get_date_time(gui, emuenv, date_tm);
            const auto date_time_str = fmt::format("{} {}", date_time.at(DateTime::DATE_MINI), date_time.at(DateTime::CLOCK));
            const auto date_time_size = ImGui::CalcTextSize(date_time_str.c_str());
            const auto REFRESH_ICON_SIZE = 42.f * SCALE.x;
            ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - date_time_size.x - REFRESH_ICON_SIZE - (20.f * SCALE.x), TITLE_BAR_HEIGHT / 2.f));
            ImGui::Text("%s", date_time_str.c_str());

            const ImVec2 refresh_icon_pos(WINDOW_POS.x + WINDOW_SIZE.x - REFRESH_ICON_SIZE - (10.f * SCALE.x), WINDOW_POS.y + (TITLE_BAR_HEIGHT - REFRESH_ICON_SIZE) / 2.f);
            draw_list->AddCircleFilled(ImVec2(refresh_icon_pos.x + REFRESH_ICON_SIZE / 2.f, refresh_icon_pos.y + REFRESH_ICON_SIZE / 2.f), REFRESH_ICON_SIZE / 2.f, IM_COL32(180, 180, 180, 255));
        }
    };

    const auto draw_conversation_list = [&]() {
        draw_title_bar(messages_lang.at("title").c_str(), false, true);

        ImGui::SetCursorPos(ImVec2(50.f * SCALE.x, TITLE_BAR_HEIGHT));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.8f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.8f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.8f, 1.0f));

        const ImVec2 BUTTON_SIZE(VIEWPORT_SIZE.x - (100.f * SCALE.x), BUTTON_HEIGHT);
        ImGui::SetWindowFontScale(1.4f * RES_SCALE.y);

        if (ImGui::Button(messages_lang.at("create_message").c_str(), BUTTON_SIZE)) {
            recipients_list.clear();
            selected_friends.clear();
            selected_recipient_index = -1;
            open_add_recipient_popup = true;
            message_screen = MessageScreen::RecipientsPreview;
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        const ImVec2 LIST_SIZE(VIEWPORT_SIZE.x - (100.f * SCALE.x), WINDOW_SIZE.y - TITLE_BAR_HEIGHT - BUTTON_HEIGHT);
        const ImVec2 LIST_POS(50.f * SCALE.x, TITLE_BAR_HEIGHT + BUTTON_HEIGHT);
        ImGui::SetCursorPos(LIST_POS);
        ImGui::BeginChild("##conversations_list", LIST_SIZE, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

        auto &conversations = emuenv.v3kn.messages_state.conversations_list;

        for (size_t i = 0; i < conversations.size(); i++) {
            const auto &conv = conversations[i];

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.6f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.6f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.6f, 0.9f));

            const ImVec2 CONV_BUTTON_SIZE(VIEWPORT_SIZE.x - (20.f * SCALE.x), BUTTON_HEIGHT);
            ImGui::SetCursorPosX(10.f * SCALE.x);

            const auto button_id = fmt::format("##conv_{}", i);
            if (ImGui::Button(button_id.c_str(), CONV_BUTTON_SIZE)) {
            }

            const auto button_pos = ImGui::GetItemRectMin();
            const auto ICON_SIZE = 50.f * SCALE.x;
            const auto ICON_PADDING = 15.f * SCALE.x;
            const auto ICON_ROUNDING = 6.f * SCALE.x;

            const ImVec2 conv_avatar_pos(button_pos.x + ICON_PADDING, button_pos.y + (BUTTON_HEIGHT - ICON_SIZE) / 2.f);
            if (gui.friends_avatar.contains(conv.online_id) && gui.friends_avatar[conv.online_id]) {
                draw_list->AddImageRounded(gui.friends_avatar[conv.online_id], conv_avatar_pos, ImVec2(conv_avatar_pos.x + ICON_SIZE, conv_avatar_pos.y + ICON_SIZE), ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, ICON_ROUNDING);
            } else {
                draw_list->AddRectFilled(conv_avatar_pos, ImVec2(conv_avatar_pos.x + ICON_SIZE, conv_avatar_pos.y + ICON_SIZE), IM_COL32(200, 200, 200, 255), ICON_ROUNDING);
            }

            ImGui::SetCursorPos(ImVec2(ICON_PADDING * 2.f + ICON_SIZE, LIST_POS.y + (i * BUTTON_HEIGHT)));
            ImGui::SetWindowFontScale(1.3f * RES_SCALE.y);
            const auto display_name = conv.group_name.empty() ? conv.online_id : conv.group_name;
            ImGui::Text("%s", display_name.c_str());

            if (!conv.last_message.msg.empty()) {
                ImGui::SetCursorPos(ImVec2(ICON_PADDING * 2.f + ICON_SIZE, LIST_POS.y + (i * BUTTON_HEIGHT) + (40.f * SCALE.y)));
                ImGui::SetWindowFontScale(1.0f * RES_SCALE.y);
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", conv.last_message.msg.c_str());
            }

            ImGui::SetWindowFontScale(1.0f * RES_SCALE.y);
            tm msg_tm = {};
            SAFE_LOCALTIME(&conv.last_message.timestamp, &msg_tm);
            const auto msg_date = get_date_time(gui, emuenv, msg_tm);
            const std::string date_str = msg_date.at(DateTime::DATE_MINI);
            const auto date_str_size = ImGui::CalcTextSize(date_str.c_str());
            ImGui::SetCursorPos(ImVec2(VIEWPORT_SIZE.x - date_str_size.x - (25.f * SCALE.x), LIST_POS.y + (i * BUTTON_HEIGHT) + (15.f * SCALE.y)));
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", date_str.c_str());

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
        }

        ImGui::EndChild();

        const auto SETTINGS_BUTTON_SIZE = 60.f * SCALE.x;
        const ImVec2 settings_button_pos(VIEWPORT_SIZE.x - SETTINGS_BUTTON_SIZE - (15.f * SCALE.x), VIEWPORT_SIZE.y - SETTINGS_BUTTON_SIZE - (15.f * SCALE.y));

        ImGui::SetCursorPos(settings_button_pos);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, SETTINGS_BUTTON_SIZE / 2.f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.2f, 0.5f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.2f, 0.5f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.2f, 0.5f, 1.0f));
        ImGui::SetWindowFontScale(2.0f * RES_SCALE.y);

        if (ImGui::Button("...", ImVec2(SETTINGS_BUTTON_SIZE, SETTINGS_BUTTON_SIZE))) {
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    };

    switch (message_screen) {
    case MessageScreen::ConversationList:
        draw_conversation_list();
        break;
    case MessageScreen::AddRecipientMenu:
        open_add_recipient_popup = true;
        message_screen = MessageScreen::RecipientsPreview;
        break;
    case MessageScreen::SelectFriend: {
        ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
        ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.f));

        bool is_open = true;
        if (ImGui::Begin("##friends_list", &is_open, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground)) {
            const float bottom_bar_height = TITLE_BAR_HEIGHT;
            draw_list->AddRectFilled(WINDOW_POS, ImVec2(WINDOW_POS.x + WINDOW_SIZE.x, WINDOW_POS.y + WINDOW_SIZE.y), IM_COL32(48, 48, 56, 255));
            draw_list->AddRectFilled(ImVec2(WINDOW_POS.x, WINDOW_POS.y + WINDOW_SIZE.y - bottom_bar_height),
                ImVec2(WINDOW_POS.x + WINDOW_SIZE.x, WINDOW_POS.y + WINDOW_SIZE.y), IM_COL32(38, 29, 48, 255));
            draw_title_bar(friends_lang.at("friends").c_str(), true, false);
            // Title bar handles close and title

            ImGui::SetWindowFontScale(1.2f * RES_SCALE.y);
            const auto &status_text = friends_lang.at("online_status");
            const auto status_text_size = ImGui::CalcTextSize(status_text.c_str());
            ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - status_text_size.x - (100.f * SCALE.x), (TITLE_BAR_HEIGHT - status_text_size.y) / 2.f));
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s ▼", status_text.c_str());

            ImGui::SetCursorPosY(TITLE_BAR_HEIGHT);
            ImGui::Separator();

            const auto &friends = emuenv.v3kn.friends_state.friends_list;
            const float LIST_HEIGHT = WINDOW_SIZE.y - TITLE_BAR_HEIGHT - bottom_bar_height - (20.f * SCALE.y);
            const float ITEM_HEIGHT = 80.f * SCALE.y;

            ImGui::SetCursorPos(ImVec2(10.f * SCALE.x, TITLE_BAR_HEIGHT + (10.f * SCALE.y)));
            ImGui::BeginChild("##friends_scroll", ImVec2(WINDOW_SIZE.x - 20.f * SCALE.x, LIST_HEIGHT), false, ImGuiWindowFlags_NoBackground);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
            for (size_t i = 0; i < friends.size(); i++) {
                const auto item_cursor_pos = ImGui::GetCursorPos();
                const auto &friend_info = friends[i];
                const bool already_added = std::find(recipients_list.begin(), recipients_list.end(), friend_info.online_id) != recipients_list.end();

                ImGui::PushID(i);

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.25f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.35f, 0.9f));
                const ImVec2 FRIEND_BUTTON_SIZE(WINDOW_SIZE.x - 40.f * SCALE.x, ITEM_HEIGHT);

                if (ImGui::Button("##friend_item", FRIEND_BUTTON_SIZE)) {
                    if (!already_added) {
                        auto it = std::find(selected_friends.begin(), selected_friends.end(), friend_info.online_id);
                        if (it != selected_friends.end())
                            selected_friends.erase(it);
                        else
                            selected_friends.push_back(friend_info.online_id);
                    }
                }

                const auto item_pos = ImGui::GetItemRectMin();
                const auto draw_list_fg = ImGui::GetWindowDrawList();
                const auto window_pos = ImGui::GetWindowPos();
                const ImVec2 item_local_pos(item_pos.x - window_pos.x, item_pos.y - window_pos.y);

                const float CHECKBOX_SIZE = 46.f * SCALE.x;
                const float ITEM_START_X = item_pos.x + 16.f * SCALE.x;
                if (!already_added) {
                    const bool is_selected = std::find(selected_friends.begin(), selected_friends.end(), friend_info.online_id) != selected_friends.end();
                    const ImVec2 checkbox_pos(ITEM_START_X, item_pos.y + (ITEM_HEIGHT - CHECKBOX_SIZE) / 2.f);
                    draw_list_fg->AddRect(checkbox_pos, ImVec2(checkbox_pos.x + CHECKBOX_SIZE, checkbox_pos.y + CHECKBOX_SIZE), IM_COL32(150, 150, 150, 255), 5.f * SCALE.x, 0, 2.f * SCALE.x);
                    if (is_selected) {
                        draw_list_fg->AddRectFilled(ImVec2(checkbox_pos.x + 10.f * SCALE.x, checkbox_pos.y + 10.f * SCALE.x),
                            ImVec2(checkbox_pos.x + CHECKBOX_SIZE - 10.f * SCALE.x, checkbox_pos.y + CHECKBOX_SIZE - 10.f * SCALE.y),
                            IM_COL32(100, 150, 255, 255), 3.f * SCALE.x);
                    }
                }

                const float AVATAR_SIZE = 50.f * SCALE.x;
                const float AVATAR_ROUNDING = 6.f * SCALE.x;
                const float AVATAR_START_X = ITEM_START_X + CHECKBOX_SIZE + (52.f * SCALE.x);
                const ImVec2 avatar_pos(AVATAR_START_X, item_pos.y + (ITEM_HEIGHT - AVATAR_SIZE) / 2.f);
                if (gui.friends_avatar.contains(friend_info.online_id) && gui.friends_avatar[friend_info.online_id]) {
                    draw_list_fg->AddImageRounded(gui.friends_avatar[friend_info.online_id], avatar_pos, ImVec2(avatar_pos.x + AVATAR_SIZE, avatar_pos.y + AVATAR_SIZE), ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, AVATAR_ROUNDING);
                } else {
                    draw_list_fg->AddRectFilled(avatar_pos, ImVec2(avatar_pos.x + AVATAR_SIZE, avatar_pos.y + AVATAR_SIZE), IM_COL32(150, 150, 150, 255), AVATAR_ROUNDING);
                }

                const auto STATUS_RAD_SIZE = 11.f * SCALE.x;
                const ImVec2 STATUS_CENTER_POS(AVATAR_START_X + AVATAR_SIZE + (22.f * SCALE.x) + STATUS_RAD_SIZE, item_pos.y + (ITEM_HEIGHT / 2.f));

                draw_friend_status_dot(draw_list_fg, STATUS_CENTER_POS, SCALE, friend_info.presence_status);

                const float NAME_START_X = STATUS_CENTER_POS.x + STATUS_RAD_SIZE + (16.f * SCALE.x);
                ImGui::SetCursorPos(ImVec2(NAME_START_X - window_pos.x, item_local_pos.y + (ITEM_HEIGHT - ImGui::GetFontSize()) / 2.f));
                ImGui::SetWindowFontScale(1.0f * RES_SCALE.y);
                ImGui::Text("%s", friend_info.online_id.c_str());

                const float TROPHY_ICON_SIZE = 35.f * SCALE.x;
                const ImVec2 trophy_pos(item_pos.x + WINDOW_SIZE.x - 160.f * SCALE.x, item_pos.y + (ITEM_HEIGHT - TROPHY_ICON_SIZE) / 2.f);
                draw_list_fg->AddCircleFilled(ImVec2(trophy_pos.x + TROPHY_ICON_SIZE / 2.f, trophy_pos.y + TROPHY_ICON_SIZE / 2.f), TROPHY_ICON_SIZE / 2.f, IM_COL32(255, 215, 0, 255));

                ImGui::SetCursorPos(ImVec2(item_local_pos.x + WINDOW_SIZE.x - 95.f * SCALE.x, item_local_pos.y + (ITEM_HEIGHT - ImGui::GetFontSize()) / 2.f));
                ImGui::SetWindowFontScale(1.2f * RES_SCALE.y);
                ImGui::Text("0");

                ImGui::PopStyleColor(3);
                ImGui::PopID();

                ImGui::SetCursorPos(ImVec2(item_cursor_pos.x, item_cursor_pos.y + ITEM_HEIGHT));
            }
            ImGui::PopStyleVar();

            ImGui::EndChild();

            ImGui::SetCursorPos(ImVec2(0.f, WINDOW_SIZE.y - bottom_bar_height));
            ImGui::Separator();

            ImGui::SetWindowFontScale(1.2f * RES_SCALE.y);
            ImGui::SetCursorPos(ImVec2(30.f * SCALE.x, WINDOW_SIZE.y - 60.f * SCALE.y));
            if (ImGui::Button(messages_lang.at("uncheck_all").c_str(), ImVec2(200.f * SCALE.x, 40.f * SCALE.y)))
                selected_friends.clear();

            ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x - 150.f * SCALE.x) / 2.f, WINDOW_SIZE.y - 60.f * SCALE.y));
            ImGui::Text("%zu / %zu", selected_friends.size(), friends.size());

            ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - 150.f * SCALE.x, WINDOW_SIZE.y - 60.f * SCALE.y));
            ImGui::BeginDisabled(selected_friends.empty());
            if (ImGui::Button("OK", ImVec2(120.f * SCALE.x, 40.f * SCALE.y))) {
                for (const auto &friend_online_id : selected_friends) {
                    if (std::find(recipients_list.begin(), recipients_list.end(), friend_online_id) == recipients_list.end())
                        recipients_list.push_back(friend_online_id);
                }
                selected_friends.clear();
                message_screen = MessageScreen::RecipientsPreview;
            }
            ImGui::EndDisabled();
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);

        if (!is_open) {
            message_screen = recipients_list.empty() ? MessageScreen::ConversationList : MessageScreen::RecipientsPreview;
            selected_friends.clear();
        }
        break;
    }
    case MessageScreen::EnterOnlineId: {
        draw_title_bar(friends_lang.at("online_id").c_str(), true, false);
        ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
        ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.3f, 0.1f, 0.5f, 0.95f));

        bool is_open = true;
        if (ImGui::Begin("##online_id_input", &is_open, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
            // Title bar handles close and title

            ImGui::SetCursorPosY(TITLE_BAR_HEIGHT);
            ImGui::Separator();

            ImGui::SetWindowFontScale(1.3f * RES_SCALE.y);
            const auto &instruction = messages_lang.at("enter_online_id");
            const auto instruction_size = ImGui::CalcTextSize(instruction.c_str());
            ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x - instruction_size.x) / 2.f, 150.f * SCALE.y));
            ImGui::Text("%s", instruction.c_str());

            ImGui::SetCursorPos(ImVec2(80.f * SCALE.x, 220.f * SCALE.y));
            ImGui::PushItemWidth(WINDOW_SIZE.x - 160.f * SCALE.x);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.f * SCALE.x);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.3f, 0.7f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.5f, 0.3f, 0.7f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.5f, 0.3f, 0.7f, 0.9f));
            ImGui::InputTextWithHint("##online_id", friends_lang.at("online_id").c_str(), online_id_input, sizeof(online_id_input));
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
            ImGui::PopItemWidth();

            ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - 180.f * SCALE.x, WINDOW_SIZE.y - 80.f * SCALE.y));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.f * SCALE.x);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.7f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.4f, 0.8f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.5f, 0.9f, 1.0f));
            ImGui::BeginDisabled(std::string(online_id_input).empty());
            if (ImGui::Button(messages_lang.at("add").c_str(), ImVec2(150.f * SCALE.x, 50.f * SCALE.y))) {
                const std::string online_id_str = online_id_input;
                if (std::find(recipients_list.begin(), recipients_list.end(), online_id_str) == recipients_list.end())
                    recipients_list.push_back(online_id_str);
                online_id_input[0] = '\0';
                message_screen = MessageScreen::RecipientsPreview;
            }
            ImGui::EndDisabled();
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);

        if (!is_open) {
            message_screen = recipients_list.empty() ? MessageScreen::ConversationList : MessageScreen::RecipientsPreview;
            online_id_input[0] = '\0';
        }
        break;
    }
    case MessageScreen::RecipientsPreview: {
        draw_title_bar(messages_lang.at("to").c_str(), true, false);
        draw_list->AddRectFilled(ImVec2(WINDOW_POS.x, WINDOW_POS.y + WINDOW_SIZE.y - TITLE_BAR_HEIGHT),
            ImVec2(WINDOW_POS.x + WINDOW_SIZE.x, WINDOW_POS.y + WINDOW_SIZE.y), IM_COL32(38, 29, 48, 255));
        ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
        ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, 0.f));

        if (ImGui::Begin("##recipients_preview", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
            const auto preview_draw_list = ImGui::GetWindowDrawList();

            // Title bar handles close and title

            const float BOTTOM_BAR_HEIGHT = 150.f * SCALE.y;
            const float RECIPIENTS_LIST_HEIGHT = WINDOW_SIZE.y - TITLE_BAR_HEIGHT - BOTTOM_BAR_HEIGHT;
            ImGui::SetCursorPos(ImVec2(0.f, TITLE_BAR_HEIGHT));
            ImGui::BeginChild("##recipients_scroll", ImVec2(WINDOW_SIZE.x, RECIPIENTS_LIST_HEIGHT), false, ImGuiWindowFlags_NoBackground);

            for (size_t i = 0; i < recipients_list.size(); i++) {
                const auto &recipient = recipients_list[i];

                ImGui::PushID(i);

                const float RECIPIENT_ITEM_HEIGHT = 80.f * SCALE.y;
                const ImVec2 item_start_pos = ImGui::GetCursorScreenPos();

                const bool is_item_selected = (selected_recipient_index == static_cast<int>(i));

                preview_draw_list->AddRectFilled(
                    ImVec2(item_start_pos.x + 10.f * SCALE.x, item_start_pos.y),
                    ImVec2(item_start_pos.x + WINDOW_SIZE.x - 10.f * SCALE.x, item_start_pos.y + RECIPIENT_ITEM_HEIGHT),
                    is_item_selected ? IM_COL32(80, 50, 100, 200) : IM_COL32(60, 40, 80, 150), 10.f * SCALE.x);

                ImGui::SetCursorScreenPos(item_start_pos);
                ImGui::InvisibleButton("##recipient_button", ImVec2(WINDOW_SIZE.x - 20.f * SCALE.x, RECIPIENT_ITEM_HEIGHT));
                if (ImGui::IsItemClicked()) {
                    selected_recipient_index = static_cast<int>(i);
                    ImGui::OpenPopup("##delete_recipient_menu");
                }

                const float AVATAR_SIZE = 58.f * SCALE.x;
                const float AVATAR_ROUNDING = 6.f * SCALE.x;
                const ImVec2 avatar_pos(item_start_pos.x + 30.f * SCALE.x, item_start_pos.y + (RECIPIENT_ITEM_HEIGHT - AVATAR_SIZE) / 2.f);
                if (gui.friends_avatar.contains(recipient) && gui.friends_avatar[recipient]) {
                    preview_draw_list->AddImageRounded(gui.friends_avatar[recipient], avatar_pos, ImVec2(avatar_pos.x + AVATAR_SIZE, avatar_pos.y + AVATAR_SIZE), ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, AVATAR_ROUNDING);
                } else {
                    preview_draw_list->AddRectFilled(avatar_pos, ImVec2(avatar_pos.x + AVATAR_SIZE, avatar_pos.y + AVATAR_SIZE), IM_COL32(150, 150, 150, 255), AVATAR_ROUNDING);
                }

                ImGui::SetCursorPos(ImVec2(95.f * SCALE.x, (i * (RECIPIENT_ITEM_HEIGHT + 10.f * SCALE.y)) + (RECIPIENT_ITEM_HEIGHT - ImGui::GetFontSize()) / 2.f));
                ImGui::SetWindowFontScale(1.2f * RES_SCALE.y);
                ImGui::Text("%s", recipient.c_str());

                if (selected_recipient_index == static_cast<int>(i)) {
                    ImGui::SetNextWindowPos(ImVec2(item_start_pos.x + WINDOW_SIZE.x - 250.f * SCALE.x, item_start_pos.y + (RECIPIENT_ITEM_HEIGHT / 2.f)), ImGuiCond_Appearing);
                    if (ImGui::BeginPopup("##delete_recipient_menu", ImGuiWindowFlags_NoMove)) {
                        ImGui::SetWindowFontScale(1.2f * RES_SCALE.y);
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
                        if (ImGui::MenuItem(messages_lang.at("delete").c_str())) {
                            recipients_list.erase(recipients_list.begin() + i);
                            selected_recipient_index = -1;
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::PopStyleColor(3);
                        ImGui::EndPopup();
                    }
                }

                ImGui::PopID();

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.f * SCALE.y);
            }

            ImGui::EndChild();

            const float BUTTON_Y_POS = TITLE_BAR_HEIGHT + RECIPIENTS_LIST_HEIGHT + 10.f * SCALE.y;
            ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x - 600.f * SCALE.x) / 2.f, BUTTON_Y_POS));
            ImGui::SetWindowFontScale(1.3f * RES_SCALE.y);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.7f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.4f, 0.8f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.5f, 0.9f, 1.0f));
            if (ImGui::Button(messages_lang.at("add_recipient").c_str(), ImVec2(600.f * SCALE.x, 50.f * SCALE.y)))
                open_add_recipient_popup = true;
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();

            if (open_add_recipient_popup) {
                ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                ImGui::SetNextWindowSize(ImVec2(700.f * SCALE.x, 400.f * SCALE.y), ImGuiCond_Always);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f * SCALE.x);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.2f, 0.2f, 0.2f, 0.95f));

                bool is_open = true;
                if (ImGui::Begin("##add_recipient", &is_open, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
                    ImGui::SetCursorPos(ImVec2(10.f * SCALE.x, 10.f * SCALE.y));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.f * SCALE.x);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                    if (ImGui::Button("X", ImVec2(40.f * SCALE.x, 40.f * SCALE.y)))
                        open_add_recipient_popup = false;
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar();

                    ImGui::SetWindowFontScale(1.5f * RES_SCALE.y);
                    const auto &menu_title = messages_lang.at("add_recipient");
                    const auto menu_title_size = ImGui::CalcTextSize(menu_title.c_str());
                    ImGui::SetCursorPos(ImVec2((700.f * SCALE.x - menu_title_size.x) / 2.f, (80.f * SCALE.y - menu_title_size.y) / 2.f));
                    ImGui::Text("%s", menu_title.c_str());

                    ImGui::SetCursorPosY(80.f * SCALE.y);
                    ImGui::Separator();

                    const ImVec2 MENU_BUTTON_SIZE(650.f * SCALE.x, 60.f * SCALE.y);
                    ImGui::SetWindowFontScale(1.3f * RES_SCALE.y);
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.f * SCALE.x);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));

                    ImGui::SetCursorPos(ImVec2(25.f * SCALE.x, 100.f * SCALE.y));
                    if (ImGui::Button(friends_lang.at("friends").c_str(), MENU_BUTTON_SIZE)) {
                        open_add_recipient_popup = false;
                        message_screen = MessageScreen::SelectFriend;
                        selected_friends.clear();
                    }

                    ImGui::SetCursorPos(ImVec2(25.f * SCALE.x, 260.f * SCALE.y));
                    if (ImGui::Button(friends_lang.at("online_id").c_str(), MENU_BUTTON_SIZE)) {
                        open_add_recipient_popup = false;
                        message_screen = MessageScreen::EnterOnlineId;
                        online_id_input[0] = '\0';
                    }

                    ImGui::PopStyleColor(3);
                    ImGui::PopStyleVar();
                }
                ImGui::End();
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();

                if (!is_open)
                    open_add_recipient_popup = false;
            }

            const float BOTTOM_BUTTON_HEIGHT = 60.f * SCALE.y;
            const float BOTTOM_BUTTON_Y = WINDOW_SIZE.y - BOTTOM_BUTTON_HEIGHT - 15.f * SCALE.y;

            ImGui::SetCursorPos(ImVec2(40.f * SCALE.x, BOTTOM_BUTTON_Y));
            ImGui::SetWindowFontScale(1.6f * RES_SCALE.y);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            if (ImGui::Button(messages_lang.at("cancel").c_str(), ImVec2(220.f * SCALE.x, BOTTOM_BUTTON_HEIGHT))) {
                recipients_list.clear();
                selected_recipient_index = -1;
                open_add_recipient_popup = false;
                message_screen = MessageScreen::ConversationList;
            }
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();

            ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - 260.f * SCALE.x, BOTTOM_BUTTON_Y));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.3f, 0.7f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.4f, 0.8f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.5f, 0.9f, 1.0f));
            ImGui::BeginDisabled(recipients_list.empty());
            if (ImGui::Button("OK", ImVec2(220.f * SCALE.x, BOTTOM_BUTTON_HEIGHT))) {
                recipients_list.clear();
                selected_recipient_index = -1;
                open_add_recipient_popup = false;
                message_screen = MessageScreen::Conversation;
            }
            ImGui::EndDisabled();
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);
        break;
    }
    case MessageScreen::Conversation:
    default:
        break;
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace gui
