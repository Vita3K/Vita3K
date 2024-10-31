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

#include <dialog/state.h>

#include <gui/functions.h>
#include <host/dialog/filesystem.h>

#include <io/state.h>

#include <util/fs.h>
#include <util/hash.h>
#include <util/log.h>
#include <util/safe_time.h>

#include <v3kn/account.h>
#include <v3kn/state.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>
#include <optional>
#include <utility>

#include <stb_image.h>
#include <stb_image_write.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

#undef ERROR

struct PanelUploadState {
    std::string *result_message;
    bool *upload_success;
    bool *is_uploading;
};

void upload_v3kn_panel(GuiState *gui, EmuEnvState *emuenv, UserInfo *user_info, std::vector<unsigned char> panel_source,
    int source_w, int source_h, float crop_scroll_x, float crop_scroll_y, float crop_scale, float crop_zoom, PanelUploadState state) {
    constexpr int panel_width = 400;
    constexpr int panel_height = 80;

    if (panel_source.empty() || source_w <= 0 || source_h <= 0) {
        *state.result_message = "Failed to load image for processing.";
        *state.is_uploading = false;
        return;
    }

    constexpr float panel_ratio = static_cast<float>(panel_width) / static_cast<float>(panel_height);
    const float max_crop_w = static_cast<float>(source_w) / std::max(crop_zoom, 1.f);
    const float max_crop_h = static_cast<float>(source_h) / std::max(crop_zoom, 1.f);
    int crop_w = static_cast<int>(std::round(max_crop_w));
    int crop_h = static_cast<int>(std::round(static_cast<float>(crop_w) / panel_ratio));
    if (crop_h > static_cast<int>(std::round(max_crop_h))) {
        crop_h = static_cast<int>(std::round(max_crop_h));
        crop_w = static_cast<int>(std::round(static_cast<float>(crop_h) * panel_ratio));
    }

    int crop_x = 0;
    int crop_y = 0;
    if (crop_w < source_w) {
        crop_x = static_cast<int>(std::round(crop_scroll_x / crop_scale));
        crop_x = std::clamp(crop_x, 0, std::max(0, source_w - crop_w));
    }
    if (crop_h < source_h) {
        crop_y = static_cast<int>(std::round(crop_scroll_y / crop_scale));
        crop_y = std::clamp(crop_y, 0, std::max(0, source_h - crop_h));
    }

    std::vector<unsigned char> cropped(crop_w * crop_h * 4);
    for (int y = 0; y < crop_h; ++y) {
        const auto src_offset = ((crop_y + y) * source_w + crop_x) * 4;
        const auto dst_offset = y * crop_w * 4;
        std::copy_n(panel_source.data() + src_offset, crop_w * 4, cropped.data() + dst_offset);
    }

    std::vector<unsigned char> resized(panel_width * panel_height * 4);
    stbir_resize_uint8_linear(cropped.data(), crop_w, crop_h, 0, resized.data(), panel_width, panel_height, 0, STBIR_RGBA);

    std::vector<unsigned char> png_data;
    stbi_write_png_to_func(
        [](void *context, void *data, int size) {
            auto *vec = static_cast<std::vector<unsigned char> *>(context);
            const auto ptr = static_cast<unsigned char *>(data);
            vec->insert(vec->end(), ptr, ptr + size);
        },
        &png_data, panel_width, panel_height, 4, resized.data(), panel_width * 4);

    const auto res = v3kn::v3kn_panel_upload(*user_info, png_data);
    v3kn::handle_v3kn_status(*emuenv, res);

    if (res.body.starts_with("OK")) {
        int w = 0;
        int h = 0;
        auto *data = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(png_data.data()), static_cast<int>(png_data.size()), &w, &h, nullptr, 4);
        if (data)
            gui->v3kn_panel = ImGui_Texture(gui->imgui_state.get(), data, w, h);
        *state.upload_success = true;
        *state.result_message = "Panel uploaded successfully.";
    } else {
        *state.result_message = v3kn::get_v3kn_error_message(*emuenv, res);
    }
    *state.is_uploading = false;
}

enum class V3knPendingAction {
    NONE,
    CREATE,
    LOGIN,
    REFRESH_QUOTA,
    DELETE_ACCOUNT,
    CHANGE_ONLINE_ID,
    CHANGE_PASSWORD,
    CHANGE_ABOUT_ME,
};

struct V3knPendingResult {
    V3knPendingAction action = V3knPendingAction::NONE;
    net_utils::WebResponse response;
    std::string online_id;
    std::string password;
    std::string token;
    std::string message;
    uint64_t quota_used = 0;
    uint64_t quota_total = 0;
    bool has_quota = false;
};

namespace gui {

void draw_v3kn_dialog(GuiState &gui, EmuEnvState &emuenv) {
    static std::mutex result_mutex;
    static std::optional<V3knPendingResult> pending_result;
    static std::atomic_bool is_result_ready{ false };
    static std::atomic_bool is_waiting_result{ false };

    enum class V3knResultState {
        NONE,
        PENDING_ACK,
        CLOSE_PARENT,
    };

    static V3knResultState result_state = V3knResultState::NONE;

    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    const float INDICATOR_SIZE(32.f * SCALE.y);
    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x / 1.6f, 0.f);
    const ImVec2 WINDOW_POS(VIEWPORT_POS.x + (VIEWPORT_SIZE.x - WINDOW_SIZE.x) / 2.f, VIEWPORT_POS.y + (VIEWPORT_SIZE.y - WINDOW_SIZE.y) / 2.f);

    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always, ImVec2(0.f, 0.5f));
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::Begin("##v3kn_dialog", &gui.configuration_menu.v3kn_dialog, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove);

    const ImVec2 BUTTON_SIZE(200.f * SCALE.x, 40.f * SCALE.y);
    const ImVec2 SMALL_BUTTON_SIZE(100.f * SCALE.x, 40.f * SCALE.y);

    ImGui::SetWindowFontScale(RES_SCALE.y);
    const ImVec2 POPUP_SIZE = ImGui::GetWindowSize();
    const ImVec2 POPUP_POS = ImGui::GetWindowPos();

    const auto title = "V3KN Account Management";
    const auto FONT_SCALE = 0.81f;
    const auto title_size = ImGui::CalcTextSize(title);
    const auto BLOCK_TITLE_SIZE_HEIGHT = 40.f * SCALE.y;
    const ImVec2 title_pos((POPUP_SIZE.x - title_size.x) / 2.f, (BLOCK_TITLE_SIZE_HEIGHT - (title_size.y * FONT_SCALE)) / 2.f);
    ImGui::SetCursorPos(title_pos);
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title);
    ImGui::SetCursorPosY(BLOCK_TITLE_SIZE_HEIGHT);
    ImGui::Separator();

    static std::string result_message;
    static char username[16] = "";
    static char password[64] = "";
    static char confirm[64] = "";
    static char about_me[22] = "";

    auto &common_lang = emuenv.common_dialog.lang.common;
    auto &account_state = emuenv.v3kn.account_state;
    auto &user_info = account_state.user_info;

    if (is_result_ready.exchange(false)) {
        std::optional<V3knPendingResult> result;
        {
            std::lock_guard<std::mutex> lock(result_mutex);
            result = std::exchange(pending_result, std::nullopt);
        }

        if (result) {
            v3kn::handle_v3kn_status(emuenv, result->response);
            result_state = V3knResultState::NONE;

            const auto rename_avatar_cache = [&](const std::string &old_online_id, const std::string &new_online_id) {
                if (old_online_id.empty() || new_online_id.empty() || old_online_id == new_online_id)
                    return;

                const fs::path server_cache_dir = emuenv.cache_path / "v3kn" / user_info.host;
                const fs::path old_avatar_cache_dir = server_cache_dir / old_online_id;
                const fs::path new_avatar_cache_dir = server_cache_dir / new_online_id;

                if (!fs::exists(old_avatar_cache_dir))
                    return;

                fs::create_directories(server_cache_dir);
                fs::rename(old_avatar_cache_dir, new_avatar_cache_dir);
            };

            const auto complete_sign_in = [&]() {
                user_info.online_id = result->online_id;
                user_info.password = result->password;
                user_info.token = result->token;
                v3kn::save_v3kn_user_info(emuenv);
                std::thread([&gui, &emuenv]() {
                    v3kn::init_v3kn_user_info(gui, emuenv);
                }).detach();
            };

            switch (result->action) {
            case V3knPendingAction::CREATE:
            case V3knPendingAction::LOGIN:
                if (result->response.body.starts_with("OK:") && !result->token.empty()) {
                    complete_sign_in();
                    result_state = V3knResultState::PENDING_ACK;
                }
                break;
            case V3knPendingAction::REFRESH_QUOTA:
                if (result->has_quota) {
                    user_info.quota_used = result->quota_used;
                    user_info.quota_total = result->quota_total;
                }
                break;
            case V3knPendingAction::DELETE_ACCOUNT:
                if (result->response.body.starts_with("OK:")) {
                    user_info = {};
                    v3kn::save_v3kn_user_info(emuenv);
                    result_state = V3knResultState::PENDING_ACK;
                    std::thread([&gui, &emuenv]() {
                        v3kn::init_v3kn_user_info(gui, emuenv);
                    }).detach();
                }
                break;
            case V3knPendingAction::CHANGE_ONLINE_ID:
                if (result->response.body.starts_with("OK:")) {
                    const std::string old_online_id = user_info.online_id;
                    user_info.online_id = result->online_id;
                    rename_avatar_cache(old_online_id, user_info.online_id);
                    v3kn::save_v3kn_user_info(emuenv);
                    result_state = V3knResultState::PENDING_ACK;
                }
                break;
            case V3knPendingAction::CHANGE_PASSWORD:
                if (result->response.body.starts_with("OK:") && !result->token.empty()) {
                    user_info.password = result->password;
                    user_info.token = result->token;
                    v3kn::save_v3kn_user_info(emuenv);
                    result_state = V3knResultState::PENDING_ACK;
                }
                break;
            case V3knPendingAction::CHANGE_ABOUT_ME:
                if (result->response.body.starts_with("OK:")) {
                    result_state = V3knResultState::PENDING_ACK;
                }
                break;
            case V3knPendingAction::NONE:
                break;
            }

            result_message = result->message;
            is_waiting_result = false;
        }
    }

    const std::string OK_STR = common_lang["ok"];
    const std::string CANCEL_STR = common_lang["cancel"];

    const auto popup_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

    static const auto result_popup = [&]() {
        const ImVec2 POPUP_SIZE(620.f * SCALE.x, 200.f * SCALE.y);
        const ImVec2 POPUP_POS((VIEWPORT_SIZE.x - POPUP_SIZE.x) / 2.f, (VIEWPORT_SIZE.y - POPUP_SIZE.y) / 2.f);
        ImGui::SetNextWindowSize(POPUP_SIZE, ImGuiCond_Always);
        ImGui::SetNextWindowPos(POPUP_POS, ImGuiCond_Always);
        if (ImGui::BeginPopupModal("Result", nullptr, popup_flags)) {
            const auto popup_width_size = ImGui::GetWindowWidth();
            const auto text = result_message.empty() ? emuenv.common_dialog.lang.common["please_wait"] : result_message;
            const auto text_size = ImGui::CalcTextSize(text.c_str());
            ImGui::SetCursorPosY(((POPUP_SIZE.y - text_size.y) / 2.f) - SMALL_BUTTON_SIZE.y);
            TextColoredCentered(GUI_COLOR_TEXT, text.c_str(), 20.f * SCALE.x);
            ImGui::SetCursorPos(ImVec2((popup_width_size - SMALL_BUTTON_SIZE.x) / 2.f, POPUP_SIZE.y - SMALL_BUTTON_SIZE.y - (16.f * SCALE.y)));
            ImGui::BeginDisabled(is_waiting_result);
            if (ImGui::Button("OK", SMALL_BUTTON_SIZE)) {
                ImGui::CloseCurrentPopup();
                if (result_state == V3knResultState::PENDING_ACK) {
                    result_state = V3knResultState::CLOSE_PARENT;
                    username[0] = '\0';
                    password[0] = '\0';
                    confirm[0] = '\0';
                    about_me[0] = '\0';
                } else {
                    result_state = V3knResultState::NONE;
                    result_message.clear();
                }
            }
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }
    };

    const auto AVATAR_SIZE = ImVec2(static_cast<float>(AvatarSize::V3KN) * SCALE.x, static_cast<float>(AvatarSize::V3KN) * SCALE.y);
    const auto BLOCK_INFO_SIZE_HEIGHT = AVATAR_SIZE.y + (16.f * SCALE.y);
    const auto INFO_SIZE_HEIGHT = BLOCK_INFO_SIZE_HEIGHT / 4.f;
    const auto BEGIN_INFO_POS_Y = BLOCK_TITLE_SIZE_HEIGHT + 1.f;
    const auto FIRST_INFO_POS_Y = BEGIN_INFO_POS_Y + ((INFO_SIZE_HEIGHT - (ImGui::GetFontSize() * FONT_SCALE)) / 2.f);

    ImGui::SetCursorPosY(FIRST_INFO_POS_Y);
    const auto selected_server = account_state.selected_server_index;
    if (selected_server < V3KN_SERVER_LIST_NAME.size())
        ImGui::Text("Server: %s", V3KN_SERVER_LIST_NAME[selected_server]);

    const auto draw_list = ImGui::GetWindowDrawList();
    const auto STATUS_CIRCLE_RADIUS = ImGui::GetFontSize() / 2.f;
    const auto circle_pos = ImVec2(POPUP_POS.x + ImGui::GetCursorPosX() + STATUS_CIRCLE_RADIUS, POPUP_POS.y + ImGui::GetCursorPosY() + (INFO_SIZE_HEIGHT / 3.f));
    ImU32 status_color;
    std::string status_text;

    if (!user_info.token.empty()) {
        if (v3kn::is_v3kn_logged_in()) {
            status_color = IM_COL32(14, 138, 22, 255);
            status_text = fmt::format("Signed In - ID: {}", user_info.online_id);
        } else {
            status_color = IM_COL32(224, 138, 30, 255);
            status_text = fmt::format("Offline - ID: {}", user_info.online_id);
        }
    } else {
        status_color = IM_COL32(255, 0, 0, 255);
        status_text = "Not Signed In - ID: " + (user_info.online_id.empty() ? std::string("-") : user_info.online_id);
    }

    draw_list->AddCircleFilled(circle_pos, STATUS_CIRCLE_RADIUS, status_color);
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (STATUS_CIRCLE_RADIUS * 2.f) + (8.f * SCALE.x), FIRST_INFO_POS_Y + INFO_SIZE_HEIGHT));
    ImGui::Text("%s", status_text.c_str());

    std::string created_at = "-";
    if (user_info.created_at > 0) {
        tm created_tm = {};
        SAFE_LOCALTIME(&user_info.created_at, &created_tm);
        auto CREATED_AT = get_date_time(gui, emuenv, created_tm);
        created_at = fmt::format("{} {} {}", CREATED_AT[DateTime::DATE_MINI], CREATED_AT[DateTime::CLOCK], emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR ? CREATED_AT[DateTime::DAY_MOMENT] : "");
    }
    ImGui::SetCursorPosY(FIRST_INFO_POS_Y + INFO_SIZE_HEIGHT * 2);
    ImGui::Text("Created At: %s", created_at.c_str());
    ImGui::SetCursorPosY(FIRST_INFO_POS_Y + (INFO_SIZE_HEIGHT * 3));
    ImGui::Text("Quota Used: %s / %s",
        user_info.quota_used == 0 ? "-" : get_unit_size(user_info.quota_used).c_str(),
        user_info.quota_total == 0 ? "-" : get_unit_size(user_info.quota_total).c_str());

    const auto END_INFO_POS_Y = BEGIN_INFO_POS_Y + BLOCK_INFO_SIZE_HEIGHT;

    // Display V3KN avatar if available, otherwise fall back to local user avatar
    if (gui.v3kn_avatar) {
        const ImVec2 V3KN_AVATAR_SIZE(static_cast<float>(AvatarSize::V3KN) * SCALE.x, static_cast<float>(AvatarSize::V3KN) * SCALE.y);
        const ImVec2 V3KN_AVATAR_POS(
            POPUP_SIZE.x - V3KN_AVATAR_SIZE.x - (10.f * SCALE.x),
            BEGIN_INFO_POS_Y + ((BLOCK_INFO_SIZE_HEIGHT - V3KN_AVATAR_SIZE.y) / 2.f));
        const float BORDER_THICKNESS = 2.f * SCALE.x;
        ImGui::GetWindowDrawList()->AddRect(
            ImVec2(POPUP_POS.x + V3KN_AVATAR_POS.x - BORDER_THICKNESS, POPUP_POS.y + V3KN_AVATAR_POS.y - BORDER_THICKNESS),
            ImVec2(POPUP_POS.x + V3KN_AVATAR_POS.x + V3KN_AVATAR_SIZE.x + BORDER_THICKNESS, POPUP_POS.y + V3KN_AVATAR_POS.y + V3KN_AVATAR_SIZE.y + BORDER_THICKNESS),
            IM_COL32(100, 149, 237, 255), 6.f * SCALE.x, 0, BORDER_THICKNESS);
        ImGui::SetCursorPos(V3KN_AVATAR_POS);
        ImGui::Image(gui.v3kn_avatar, V3KN_AVATAR_SIZE);
    } else if (gui.users_avatar[emuenv.io.user_id]) {
        const auto &user_avatar_infos = gui.users_avatar_infos[emuenv.io.user_id][AvatarSize::V3KN];
        const ImVec2 AVATAR_FINAL_SIZE = ImVec2(user_avatar_infos.size.x * SCALE.x, user_avatar_infos.size.y * SCALE.y);
        const ImVec2 AVATAR_POS = ImVec2(
            POPUP_SIZE.x - AVATAR_SIZE.x - (10.f * SCALE.x) + (user_avatar_infos.pos.x * SCALE.x),
            BEGIN_INFO_POS_Y + ((BLOCK_INFO_SIZE_HEIGHT - AVATAR_SIZE.y) / 2.f) + (user_avatar_infos.pos.y * SCALE.y));
        const float BORDER_THICKNESS = 2.f * SCALE.x;
        ImGui::GetWindowDrawList()->AddRect(
            ImVec2(POPUP_POS.x + AVATAR_POS.x - BORDER_THICKNESS, POPUP_POS.y + AVATAR_POS.y - BORDER_THICKNESS),
            ImVec2(POPUP_POS.x + AVATAR_POS.x + AVATAR_FINAL_SIZE.x + BORDER_THICKNESS, POPUP_POS.y + AVATAR_POS.y + AVATAR_FINAL_SIZE.y + BORDER_THICKNESS),
            IM_COL32(100, 149, 237, 255), 0.f, 0, BORDER_THICKNESS);
        ImGui::SetCursorPos(AVATAR_POS);
        ImGui::Image(gui.users_avatar[emuenv.io.user_id], AVATAR_FINAL_SIZE);
    }

    ImGui::SetCursorPosY(END_INFO_POS_Y);
    ImGui::Separator();
    ImGui::Spacing();

    const auto AFTER_INFO_POS = ImGui::GetCursorPosY();

    ImGui::BeginDisabled(v3kn::is_v3kn_logged_in() && result_state == V3knResultState::NONE);

    // Create Account Button
    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - BUTTON_SIZE.x - (20.f * SCALE.x));
    if (ImGui::Button("Create Account", BUTTON_SIZE)) {
        result_state = V3knResultState::NONE;
        ImGui::OpenPopup("V3KN Create");
    }

    if (ImGui::BeginPopupModal("V3KN Create", nullptr, popup_flags)) {
        const auto title_popup = "Create V3KN Account";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title_popup).x) / 2.f);
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title_popup);
        ImGui::Separator();

        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "Please enter a username and password to create your V3KN account.");
        ImGui::TextColored(GUI_COLOR_TEXT, "Username must start with a letter and contain 3–16 characters (letters, numbers, _ or -).");
        ImGui::TextColored(GUI_COLOR_TEXT, "Password must be at least 8 characters long.");

        ImGui::InputText("Username", username, sizeof(username), ImGuiInputTextFlags_CharsNoBlank);
        ImGui::InputText("Password", password, sizeof(password), ImGuiInputTextFlags_Password);
        const auto res = ImGui::InputText("Confirm Password", confirm, sizeof(confirm), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_Password);

        const std::string username_str(username);
        const std::string password_str(password);
        const std::string confirm_str(confirm);
        ImGui::BeginDisabled(is_waiting_result || username_str.empty() || (username_str.size() < 3) || password_str.empty() || confirm_str.empty() || (password_str.size() < 8));
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - SMALL_BUTTON_SIZE.x - (20.f * SCALE.x));
        if (ImGui::Button(OK_STR.c_str(), SMALL_BUTTON_SIZE) || res) {
            ImGui::OpenPopup("Result");
            if (std::string(password) != std::string(confirm)) {
                result_state = V3knResultState::NONE;
                result_message = "Password and confirmation do not match.";
            } else {
                const std::string derived_password = derive_password(password);
                const std::string base64_password = base64_encode(derived_password);
                const std::string requested_username = username;
                const UserInfo current_user_info = user_info;
                is_waiting_result = true;
                std::thread([&emuenv, requested_username, base64_password, current_user_info]() {
                    const auto res = v3kn::v3kn_create(current_user_info, requested_username, base64_password);
                    V3knPendingResult result;
                    result.action = V3knPendingAction::CREATE;
                    result.response = res;
                    result.online_id = requested_username;
                    result.password = base64_password;
                    if (res.body.starts_with("OK:")) {
                        std::smatch match;
                        std::regex regex_pattern(R"(OK:([^:]+))");
                        if (std::regex_search(res.body, match, regex_pattern) && (match.size() == 2)) {
                            result.token = match[1];
                            result.message = fmt::format("Account created successfully for user {}", requested_username);
                        } else {
                            result.message = fmt::format("Unexpected response format from server: {}", res.body);
                        }
                    } else {
                        result.message = v3kn::get_v3kn_error_message(emuenv, res);
                    }
                    {
                        std::lock_guard<std::mutex> lock(result_mutex);
                        pending_result = std::move(result);
                    }
                    is_result_ready = true;
                }).detach();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 40.f * SCALE.x);
        if (ImGui::Button(CANCEL_STR.c_str(), SMALL_BUTTON_SIZE)) {
            result_state = V3knResultState::NONE;
            username[0] = '\0';
            password[0] = '\0';
            confirm[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        result_popup();
        if (result_state == V3knResultState::CLOSE_PARENT) {
            ImGui::CloseCurrentPopup();
            result_message.clear();
            result_state = V3knResultState::NONE;
        }
        ImGui::EndPopup();
    }

    // Login Button
    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - BUTTON_SIZE.x - (20.f * SCALE.x));
    if (ImGui::Button("Login", BUTTON_SIZE)) {
        result_state = V3knResultState::NONE;
        ImGui::OpenPopup("V3KN Login");
    }

    if (ImGui::BeginPopupModal("V3KN Login", nullptr, popup_flags)) {
        const auto title_popup = "V3KN Login";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title_popup).x) / 2.f);
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title_popup);
        ImGui::Separator();

        ImGui::InputText("Username", username, sizeof(username));
        auto res = ImGui::InputText("Password", password, sizeof(password), ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::BeginDisabled(is_waiting_result || std::string(password).empty() || std::string(username).empty());
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - SMALL_BUTTON_SIZE.x - (20.f * SCALE.x));
        if (ImGui::Button(OK_STR.c_str(), SMALL_BUTTON_SIZE) || res) {
            const std::string derived_password = derive_password(password);
            const std::string base64_password = base64_encode(derived_password);
            const std::string requested_username = username;
            const UserInfo current_user_info = user_info;
            result_state = V3knResultState::NONE;
            is_waiting_result = true;
            ImGui::OpenPopup("Result");
            std::thread([&emuenv, requested_username, base64_password, current_user_info]() {
                const auto res = v3kn::v3kn_login(current_user_info, requested_username, base64_password);
                V3knPendingResult result;
                result.action = V3knPendingAction::LOGIN;
                result.response = res;
                result.password = base64_password;
                if (res.body.starts_with("OK:")) {
                    std::smatch match;
                    std::regex regex_pattern(R"(OK:([^:]+):([^:]+))");
                    if (std::regex_search(res.body, match, regex_pattern) && (match.size() == 3)) {
                        result.online_id = match[1];
                        result.token = match[2];
                        result.message = "Login on V3KN successfully for user " + result.online_id;
                    } else {
                        result.message = v3kn::get_v3kn_error_message(emuenv, res);
                    }
                } else {
                    result.message = v3kn::get_v3kn_error_message(emuenv, res);
                }
                {
                    std::lock_guard<std::mutex> lock(result_mutex);
                    pending_result = std::move(result);
                }
                is_result_ready = true;
            }).detach();
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 40.f * SCALE.x);
        if (ImGui::Button(CANCEL_STR.c_str(), SMALL_BUTTON_SIZE)) {
            result_state = V3knResultState::NONE;
            username[0] = '\0';
            password[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        result_popup();
        if (result_state == V3knResultState::CLOSE_PARENT) {
            ImGui::CloseCurrentPopup();
            result_message.clear();
            result_state = V3knResultState::NONE;
        }
        ImGui::EndPopup();
    }
    ImGui::EndDisabled();

    if (!v3kn::is_v3kn_logged_in() && !user_info.token.empty()) {
        ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - BUTTON_SIZE.x - (20.f * SCALE.x));
        static bool is_testing_connection = false;
        ImGui::BeginDisabled(is_waiting_result || is_testing_connection);
        if (ImGui::Button(is_testing_connection ? "Testing..." : "Test Connection", BUTTON_SIZE)) {
            is_testing_connection = true;
            std::thread([&gui, &emuenv]() {
                v3kn::init_v3kn_user_info(gui, emuenv);
                is_testing_connection = false;
            }).detach();
        }
        ImGui::EndDisabled();
        SetTooltipEx("Test connection to V3KN server with saved credentials");
    }

    ImGui::BeginDisabled(!v3kn::is_v3kn_logged_in() && result_state == V3knResultState::NONE);

    // Refresh quota button
    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - BUTTON_SIZE.x - (20.f * SCALE.x));
    ImGui::BeginDisabled(is_waiting_result);
    if (ImGui::Button("Refresh Quota", BUTTON_SIZE)) {
        const UserInfo current_user_info = user_info;
        is_waiting_result = true;
        std::thread([&emuenv, current_user_info]() {
            const std::string url = v3kn::get_v3kn_server_url(current_user_info.host, "v3kn/quota");
            const auto res = net_utils::get_web_response_ex(url, current_user_info.token);
            V3knPendingResult result;
            result.action = V3knPendingAction::REFRESH_QUOTA;
            result.response = res;
            if (res.body.starts_with("OK:")) {
                std::smatch match;
                std::regex regex_pattern(R"(OK:([^:]+):([^:]+))");
                if (std::regex_search(res.body, match, regex_pattern) && (match.size() == 3)) {
                    result.quota_used = std::strtoull(match[1].str().c_str(), nullptr, 10);
                    result.quota_total = std::strtoull(match[2].str().c_str(), nullptr, 10);
                    result.has_quota = true;
                    result.message = fmt::format("Quota refreshed: {} / {}", get_unit_size(result.quota_used), get_unit_size(result.quota_total));
                } else {
                    result.message = v3kn::get_v3kn_error_message(emuenv, res);
                }
            } else {
                result.message = v3kn::get_v3kn_error_message(emuenv, res);
            }
            {
                std::lock_guard<std::mutex> lock(result_mutex);
                pending_result = std::move(result);
            }
            is_result_ready = true;
        }).detach();
        ImGui::OpenPopup("Result");
    }
    ImGui::EndDisabled();
    SetTooltipEx("Refresh quota");
    result_popup();

    // Delete account button
    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - BUTTON_SIZE.x - (20.f * SCALE.x));
    if (ImGui::Button("Delete Account", BUTTON_SIZE))
        ImGui::OpenPopup("V3KN Delete");

    if (ImGui::BeginPopupModal("V3KN Delete", nullptr, popup_flags)) {
        const auto title_popup = "Delete V3KN Account";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title_popup).x) / 2.f);
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title_popup);
        ImGui::Separator();

        ImGui::InputText("Password", password, sizeof(password), ImGuiInputTextFlags_Password);
        ImGui::BeginDisabled(is_waiting_result || std::string(password).empty());
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - SMALL_BUTTON_SIZE.x - (20.f * SCALE.x));
        if (ImGui::Button(OK_STR.c_str(), SMALL_BUTTON_SIZE)) {
            const std::string derived_password = derive_password(password);
            const std::string base64_password = base64_encode(derived_password);
            const UserInfo current_user_info = user_info;
            const std::string current_online_id = user_info.online_id;
            is_waiting_result = true;
            ImGui::OpenPopup("Result");
            std::thread([&emuenv, current_online_id, base64_password, current_user_info]() {
                const auto res = v3kn::v3kn_delete(current_user_info, base64_password);
                V3knPendingResult result;
                result.action = V3knPendingAction::DELETE_ACCOUNT;
                result.response = res;
                if (res.body.starts_with("OK:"))
                    result.message = "Account V3KN deleted successfully for user " + current_online_id;
                else
                    result.message = v3kn::get_v3kn_error_message(emuenv, res);
                {
                    std::lock_guard<std::mutex> lock(result_mutex);
                    pending_result = std::move(result);
                }
                is_result_ready = true;
            }).detach();
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 40.f * SCALE.x);
        if (ImGui::Button(CANCEL_STR.c_str(), SMALL_BUTTON_SIZE)) {
            password[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        result_popup();
        if (result_state == V3knResultState::CLOSE_PARENT) {
            ImGui::CloseCurrentPopup();
            result_message.clear();
            result_state = V3knResultState::NONE;
        }
        ImGui::EndPopup();
    }

    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - BUTTON_SIZE.x - (20.f * SCALE.x));
    if (ImGui::Button("Logout", BUTTON_SIZE))
        ImGui::OpenPopup("Confirm Logout");

    if (ImGui::BeginPopupModal("Confirm Logout", nullptr, popup_flags)) {
        ImGui::Text("Are you sure you want to logout from V3KN?");
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - (SMALL_BUTTON_SIZE.x * 2.f) - (40.f * SCALE.x)) / 2.f);
        if (ImGui::Button(common_lang["no"].c_str(), SMALL_BUTTON_SIZE))
            ImGui::CloseCurrentPopup();
        ImGui::SameLine(0, 40.f * SCALE.x);
        if (ImGui::Button(common_lang["yes"].c_str(), SMALL_BUTTON_SIZE)) {
            user_info = {};
            v3kn::set_v3kn_logged_in(false);
            v3kn::save_v3kn_user_info(emuenv);
            gui.v3kn_avatar = {};
            std::thread([&gui, &emuenv]() {
                v3kn::init_v3kn_user_info(gui, emuenv);
            }).detach();
            result_message = "Logged out successfully.";
            ImGui::CloseCurrentPopup();
        }
        result_popup();
        if (result_state == V3knResultState::CLOSE_PARENT) {
            ImGui::CloseCurrentPopup();
            result_message.clear();
            result_state = V3knResultState::NONE;
        }
        ImGui::EndPopup();
    }

    ImGui::SetCursorPosY(AFTER_INFO_POS);
    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) + (20.f * SCALE.x));
    if (ImGui::Button("Change Online ID", BUTTON_SIZE))
        ImGui::OpenPopup("V3KN Change Online ID");

    if (ImGui::BeginPopupModal("V3KN Change Online ID", nullptr, popup_flags)) {
        const auto title_popup = "Change V3KN Online ID";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title_popup).x) / 2.f);
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title_popup);
        ImGui::Separator();

        ImGui::InputText("New Username", username, sizeof(username), ImGuiInputTextFlags_CharsNoBlank);
        ImGui::BeginDisabled(is_waiting_result || std::string(username).empty());
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - SMALL_BUTTON_SIZE.x - (20.f * SCALE.x));
        if (ImGui::Button(OK_STR.c_str(), SMALL_BUTTON_SIZE)) {
            const std::string new_online_id = username;
            const UserInfo current_user_info = user_info;
            is_waiting_result = true;
            ImGui::OpenPopup("Result");
            std::thread([&emuenv, new_online_id, current_user_info]() {
                const auto res = v3kn::v3kn_change_online_id(current_user_info, new_online_id);
                V3knPendingResult result;
                result.action = V3knPendingAction::CHANGE_ONLINE_ID;
                result.response = res;
                result.online_id = new_online_id;
                if (res.body.starts_with("OK:"))
                    result.message = "Online ID successfully changed to " + new_online_id;
                else
                    result.message = v3kn::get_v3kn_error_message(emuenv, res);
                {
                    std::lock_guard<std::mutex> lock(result_mutex);
                    pending_result = std::move(result);
                }
                is_result_ready = true;
            }).detach();
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 40.f * SCALE.x);
        if (ImGui::Button(CANCEL_STR.c_str(), SMALL_BUTTON_SIZE)) {
            username[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        result_popup();
        if (result_state == V3knResultState::CLOSE_PARENT) {
            ImGui::CloseCurrentPopup();
            result_message.clear();
            result_state = V3knResultState::NONE;
        }
        ImGui::EndPopup();
    }

    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) + (20.f * SCALE.x));
    if (ImGui::Button("Change Password", BUTTON_SIZE))
        ImGui::OpenPopup("V3KN Change Password");

    if (ImGui::BeginPopupModal("V3KN Change Password", nullptr, popup_flags)) {
        const auto title_popup = "Change V3KN Password";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title_popup).x) / 2.f);
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title_popup);
        ImGui::Separator();

        ImGui::InputText("Current Password", password, sizeof(password), ImGuiInputTextFlags_Password);
        ImGui::InputText("New Password", confirm, sizeof(confirm), ImGuiInputTextFlags_Password);
        ImGui::BeginDisabled(is_waiting_result || std::string(password).empty() || std::string(confirm).empty());
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - SMALL_BUTTON_SIZE.x - (20.f * SCALE.x));
        if (ImGui::Button(OK_STR.c_str(), SMALL_BUTTON_SIZE)) {
            const std::string derived_current_password = derive_password(password);
            const std::string base64_current_password = base64_encode(derived_current_password);
            if (base64_current_password != user_info.password) {
                result_message = "Current password is incorrect.";
                ImGui::OpenPopup("Result");
            } else {
                const std::string derived_new_password = derive_password(confirm);
                const std::string base64_new_password = base64_encode(derived_new_password);
                const UserInfo current_user_info = user_info;
                is_waiting_result = true;
                ImGui::OpenPopup("Result");
                std::thread([&emuenv, base64_current_password, base64_new_password, current_user_info]() {
                    const auto res = v3kn::v3kn_change_password(current_user_info, base64_current_password, base64_new_password);
                    V3knPendingResult result;
                    result.action = V3knPendingAction::CHANGE_PASSWORD;
                    result.response = res;
                    result.password = base64_new_password;
                    if (res.body.starts_with("OK:")) {
                        std::smatch match;
                        std::regex regex_pattern(R"(OK:([^:]+))");
                        if (std::regex_search(res.body, match, regex_pattern) && (match.size() == 2)) {
                            result.token = match[1];
                            result.message = "Password changed successfully for user " + current_user_info.online_id;
                        } else {
                            result.message = v3kn::get_v3kn_error_message(emuenv, res);
                        }
                    } else {
                        result.message = v3kn::get_v3kn_error_message(emuenv, res);
                    }
                    {
                        std::lock_guard<std::mutex> lock(result_mutex);
                        pending_result = std::move(result);
                    }
                    is_result_ready = true;
                }).detach();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 40.f * SCALE.x);
        if (ImGui::Button(CANCEL_STR.c_str(), SMALL_BUTTON_SIZE)) {
            password[0] = '\0';
            confirm[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        result_popup();
        if (result_state == V3knResultState::CLOSE_PARENT) {
            ImGui::CloseCurrentPopup();
            result_message.clear();
            result_state = V3knResultState::NONE;
        }
        ImGui::EndPopup();
    }

    // Change About Me Button
    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) + (20.f * SCALE.x));
    if (ImGui::Button("Change About Me", BUTTON_SIZE))
        ImGui::OpenPopup("V3KN Change About Me");

    if (ImGui::BeginPopupModal("V3KN Change About Me", nullptr, popup_flags)) {
        const auto title_popup = "Change V3KN About Me";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title_popup).x) / 2.f);
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title_popup);
        ImGui::Separator();
        const ImVec2 BOX_TEXT_SIZE = ImVec2(368.f * SCALE.x, 50.f * SCALE.y);
        ImGui::SetNextItemWidth(BOX_TEXT_SIZE.x);
        const auto res = ImGui::InputText("About Me", about_me, sizeof(about_me), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::BeginDisabled(is_waiting_result || std::string(about_me).empty());
        if (ImGui::Button(OK_STR.c_str(), SMALL_BUTTON_SIZE) || res) {
            const std::string about_me_str(about_me);
            const UserInfo current_user_info = user_info;
            is_waiting_result = true;
            ImGui::OpenPopup("Result");
            std::thread([&emuenv, about_me_str, current_user_info]() {
                const auto res = v3kn::v3kn_change_about_me(current_user_info, about_me_str);
                V3knPendingResult result;
                result.action = V3knPendingAction::CHANGE_ABOUT_ME;
                result.response = res;
                if (res.body.starts_with("OK:"))
                    result.message = "About Me changed successfully for user " + current_user_info.online_id;
                else
                    result.message = v3kn::get_v3kn_error_message(emuenv, res);
                {
                    std::lock_guard<std::mutex> lock(result_mutex);
                    pending_result = std::move(result);
                }
                is_result_ready = true;
            }).detach();
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 40.f * SCALE.x);
        if (ImGui::Button(CANCEL_STR.c_str(), SMALL_BUTTON_SIZE)) {
            about_me[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        result_popup();
        if (result_state == V3knResultState::CLOSE_PARENT) {
            ImGui::CloseCurrentPopup();
            result_message.clear();
            result_state = V3knResultState::NONE;
        }
        ImGui::EndPopup();
    }

    // Change avatar
    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) + (20.f * SCALE.x));
    if (ImGui::Button("Change Avatar", BUTTON_SIZE))
        ImGui::OpenPopup("V3KN Change Avatar");

    if (ImGui::BeginPopupModal("V3KN Change Avatar", nullptr, popup_flags)) {
        const auto title_popup = "Change V3KN Avatar";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title_popup).x) / 2.f);
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title_popup);
        ImGui::Separator();

        static std::string selected_avatar_path;
        static ImGui_Texture avatar_preview;
        static int preview_w = 0, preview_h = 0;
        static bool is_uploading = false;
        static std::string avatar_result_message;
        static bool avatar_upload_success = false;

        if (avatar_upload_success) {
            avatar_upload_success = false;
            selected_avatar_path.clear();
            avatar_preview = {};
            preview_w = 0;
            preview_h = 0;
            avatar_result_message.clear();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            // skip rest of popup this frame
            goto avatar_popup_end;
        }

        if (ImGui::Button("Browse...", BUTTON_SIZE)) {
            fs::path avatar_path{};
            const auto result = host::dialog::filesystem::open_file(avatar_path, { { "Image file", { "bmp", "gif", "jpg", "jpeg", "png", "tif" } } });
            if (result == host::dialog::filesystem::Result::SUCCESS && fs::exists(avatar_path)) {
                selected_avatar_path = avatar_path.string();
                avatar_result_message.clear();

                int w, h;
                stbi_uc *data = stbi_load(selected_avatar_path.c_str(), &w, &h, nullptr, STBI_rgb_alpha);
                if (data) {
                    avatar_preview = ImGui_Texture(gui.imgui_state.get(), data, w, h);
                    preview_w = w;
                    preview_h = h;
                    stbi_image_free(data);
                } else {
                    avatar_result_message = "Failed to load image.";
                    selected_avatar_path.clear();
                }
            } else if (result == host::dialog::filesystem::Result::ERROR) {
                avatar_result_message = fmt::format("File dialog error: {}", host::dialog::filesystem::get_error());
            }
        }

        if (!selected_avatar_path.empty()) {
            ImGui::Text("Selected: %s", fs::path(selected_avatar_path).filename().string().c_str());
            if (avatar_preview && preview_w > 0 && preview_h > 0) {
                const float max_preview = 128.f * SCALE.x;
                const float ratio = std::min(max_preview / static_cast<float>(preview_w), max_preview / static_cast<float>(preview_h));
                const ImVec2 preview_size(preview_w * ratio, preview_h * ratio);
                ImGui::SetCursorPosX((ImGui::GetWindowWidth() - preview_size.x) / 2.f);
                ImGui::Image(avatar_preview, preview_size);
            }
        }

        if (!avatar_result_message.empty()) {
            const auto msg_color = avatar_result_message.find("successfully") != std::string::npos
                ? ImVec4(0.3f, 1.f, 0.3f, 1.f)
                : ImVec4(1.f, 0.3f, 0.3f, 1.f);
            ImGui::TextColored(msg_color, "%s", avatar_result_message.c_str());
        }

        ImGui::BeginDisabled(selected_avatar_path.empty() || is_uploading);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - SMALL_BUTTON_SIZE.x - (20.f * SCALE.x));
        if (ImGui::Button(is_uploading ? "Uploading..." : OK_STR.c_str(), SMALL_BUTTON_SIZE)) {
            is_uploading = true;
            avatar_result_message.clear();
            std::thread([&gui, &emuenv, &user_info]() {
                constexpr int canvas_size = 128;

                int w, h;
                stbi_uc *img = stbi_load(selected_avatar_path.c_str(), &w, &h, nullptr, STBI_rgb_alpha);
                if (!img) {
                    avatar_result_message = "Failed to load image for processing.";
                    is_uploading = false;
                    return;
                }

                const float scale_factor = static_cast<float>(canvas_size) / static_cast<float>(std::max(w, h));
                const int new_w = static_cast<int>(w * scale_factor);
                const int new_h = static_cast<int>(h * scale_factor);

                std::vector<unsigned char> resized(new_w * new_h * 4);
                stbir_resize_uint8_linear(img, w, h, 0, resized.data(), new_w, new_h, 0, STBIR_RGBA);
                stbi_image_free(img);

                std::vector<unsigned char> canvas(canvas_size * canvas_size * 4, 0);
                const int offset_x = (canvas_size - new_w) / 2;
                const int offset_y = (canvas_size - new_h) / 2;

                for (int y = 0; y < new_h; y++) {
                    for (int x = 0; x < new_w; x++) {
                        const int src = (y * new_w + x) * 4;
                        const int dst = ((y + offset_y) * canvas_size + (x + offset_x)) * 4;
                        canvas[dst + 0] = resized[src + 0];
                        canvas[dst + 1] = resized[src + 1];
                        canvas[dst + 2] = resized[src + 2];
                        canvas[dst + 3] = resized[src + 3];
                    }
                }

                std::vector<unsigned char> png_data;
                stbi_write_png_to_func(
                    [](void *context, void *data, int size) {
                        auto *vec = static_cast<std::vector<unsigned char> *>(context);
                        const auto ptr = static_cast<unsigned char *>(data);
                        vec->insert(vec->end(), ptr, ptr + size);
                    },
                    &png_data, canvas_size, canvas_size, 4, canvas.data(), canvas_size * 4);

                const auto res = v3kn::v3kn_avatar_upload(user_info, png_data);
                v3kn::handle_v3kn_status(emuenv, res);

                if (res.body.starts_with("OK")) {
                    gui.v3kn_avatar = ImGui_Texture(gui.imgui_state.get(), canvas.data(), canvas_size, canvas_size);
                    const fs::path avatar_cache_path = emuenv.cache_path / "v3kn" / user_info.host / fmt::format("{}.png", user_info.online_id);
                    fs::create_directories(avatar_cache_path.parent_path());
                    fs_utils::dump_data(avatar_cache_path, png_data.data(), static_cast<std::streamsize>(png_data.size()));
                    avatar_upload_success = true;
                } else {
                    avatar_result_message = v3kn::get_v3kn_error_message(emuenv, res);
                }
                is_uploading = false;
            }).detach();
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 40.f * SCALE.x);
        if (ImGui::Button(CANCEL_STR.c_str(), SMALL_BUTTON_SIZE)) {
            selected_avatar_path.clear();
            avatar_preview = {};
            preview_w = 0;
            preview_h = 0;
            avatar_result_message.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
avatar_popup_end:

    // Change Panel
    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) + (20.f * SCALE.x));
    if (ImGui::Button("Change Panel", BUTTON_SIZE))
        ImGui::OpenPopup("V3KN Change Panel");
    ImGui::EndDisabled();

    static std::string selected_panel_path;
    static ImGui_Texture panel_preview;
    static int panel_preview_w = 0;
    static int panel_preview_h = 0;
    static std::vector<unsigned char> panel_source_data;
    static bool is_panel_uploading = false;
    static std::string panel_result_message;
    static bool panel_upload_success = false;
    static float panel_crop_scroll_x = 0.f;
    static float panel_crop_scroll_y = 0.f;
    static float panel_crop_scale = 1.f;
    static float panel_crop_zoom = 1.f;
    static bool show_panel_crop_popup = false;

    if (ImGui::BeginPopupModal("V3KN Change Panel", nullptr, popup_flags)) {
        const auto title_popup = "Change V3KN Panel";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title_popup).x) / 2.f);
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title_popup);
        ImGui::Separator();

        if (ImGui::Button("Browse...", BUTTON_SIZE)) {
            fs::path panel_path{};
            const auto result = host::dialog::filesystem::open_file(panel_path, { { "Image file", { "bmp", "gif", "jpg", "jpeg", "png", "tif" } } });
            if (result == host::dialog::filesystem::Result::SUCCESS && fs::exists(panel_path)) {
                selected_panel_path = panel_path.string();
                panel_result_message.clear();

                int w = 0;
                int h = 0;
                stbi_uc *data = stbi_load(selected_panel_path.c_str(), &w, &h, nullptr, STBI_rgb_alpha);
                if (data) {
                    panel_preview = ImGui_Texture(gui.imgui_state.get(), data, w, h);
                    panel_preview_w = w;
                    panel_preview_h = h;
                    panel_source_data.assign(data, data + (w * h * 4));
                    stbi_image_free(data);
                    panel_crop_scroll_x = 0.f;
                    panel_crop_scroll_y = 0.f;
                    panel_crop_zoom = 1.f;
                    show_panel_crop_popup = true;
                    ImGui::CloseCurrentPopup();
                } else {
                    panel_result_message = "Failed to load image.";
                    selected_panel_path.clear();
                }
            } else if (result == host::dialog::filesystem::Result::ERROR) {
                panel_result_message = fmt::format("File dialog error: {}", host::dialog::filesystem::get_error());
            }
        }
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - SMALL_BUTTON_SIZE.x) / 2.f);
        if (ImGui::Button(CANCEL_STR.c_str(), SMALL_BUTTON_SIZE)) {
            selected_panel_path.clear();
            panel_preview = {};
            panel_preview_w = 0;
            panel_preview_h = 0;
            panel_source_data.clear();
            panel_result_message.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (show_panel_crop_popup && !selected_panel_path.empty() && panel_preview && panel_preview_w > 0 && panel_preview_h > 0) {
        constexpr float panel_ratio = 360.f / 72.f;
        const float panel_width = 720.f * SCALE.x;
        const float panel_height = panel_width / panel_ratio;
        const ImVec2 crop_popup_size(panel_width + (80.f * SCALE.x), panel_height + (160.f * SCALE.y));
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(crop_popup_size, ImGuiCond_Appearing);
        ImGui::OpenPopup("V3KN Panel Crop");
        show_panel_crop_popup = false;
    }

    if (ImGui::BeginPopupModal("V3KN Panel Crop", nullptr, popup_flags)) {
        const auto title_popup = "Crop V3KN Panel";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title_popup).x) / 2.f);
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title_popup);
        ImGui::Separator();

        if (panel_upload_success) {
            panel_upload_success = false;
            selected_panel_path.clear();
            panel_preview = {};
            panel_preview_w = 0;
            panel_preview_h = 0;
            panel_source_data.clear();
            panel_result_message.clear();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        } else {
            const ImVec2 PANEL_SIZE(720.f * SCALE.x, 144.f * SCALE.y);
            const float panel_base_scale = PANEL_SIZE.x / static_cast<float>(panel_preview_w);
            panel_crop_scale = panel_base_scale * panel_crop_zoom;
            float panel_image_height = panel_preview_h * panel_crop_scale;
            const bool needs_scroll_y_for_size = panel_image_height > (PANEL_SIZE.y + 0.5f);
            const float panel_extra_width = needs_scroll_y_for_size ? ImGui::GetStyle().ScrollbarSize : 0.f;
            const ImVec2 panel_crop_size(PANEL_SIZE.x + panel_extra_width, PANEL_SIZE.y);
            static float panel_view_width = 0.f;
            static float panel_view_height = 0.f;
            if (panel_view_width <= 0.f)
                panel_view_width = PANEL_SIZE.x;
            if (panel_view_height <= 0.f)
                panel_view_height = PANEL_SIZE.y;

            ImGui::Text("Selected: %s", fs::path(selected_panel_path).filename().string().c_str());
            const ImVec2 zoom_button_size(40.f * SCALE.x, 40.f * SCALE.y);
            const float panel_crop_start_x = (ImGui::GetWindowWidth() - panel_crop_size.x) / 2.f;
            const float panel_crop_start_y = ImGui::GetCursorPosY();
            const float zoom_button_y = panel_crop_start_y + ((panel_crop_size.y - zoom_button_size.y) / 2.f);

            float next_scroll_x = panel_crop_scroll_x;
            float next_scroll_y = panel_crop_scroll_y;
            bool zoom_changed = false;

            const auto update_zoom = [&](float new_zoom) {
                new_zoom = std::clamp(new_zoom, 1.f, 4.f);
                if (std::abs(new_zoom - panel_crop_zoom) < 0.001f)
                    return;

                const float old_scale = panel_base_scale * panel_crop_zoom;
                const float new_scale = panel_base_scale * new_zoom;
                const float center_x = (panel_crop_scroll_x + (panel_view_width * 0.5f)) / old_scale;
                const float center_y = (panel_crop_scroll_y + (panel_view_height * 0.5f)) / old_scale;
                const float max_scroll_x = std::max(0.f, (panel_preview_w * new_scale) - panel_view_width);
                const float max_scroll_y = std::max(0.f, (panel_preview_h * new_scale) - panel_view_height);

                next_scroll_x = std::clamp((center_x * new_scale) - (panel_view_width * 0.5f), 0.f, max_scroll_x);
                next_scroll_y = std::clamp((center_y * new_scale) - (panel_view_height * 0.5f), 0.f, max_scroll_y);
                panel_crop_zoom = new_zoom;
                zoom_changed = true;
            };

            ImGui::SetCursorPos(ImVec2(panel_crop_start_x - zoom_button_size.x - (10.f * SCALE.x), zoom_button_y));
            if (ImGui::Button("-", zoom_button_size))
                update_zoom(panel_crop_zoom - 0.05f);

            ImGui::SetCursorPos(ImVec2(panel_crop_start_x + panel_crop_size.x + (10.f * SCALE.x), zoom_button_y));
            if (ImGui::Button("+", zoom_button_size))
                update_zoom(panel_crop_zoom + 0.05f);

            if (zoom_changed) {
                panel_crop_scale = panel_base_scale * panel_crop_zoom;
                panel_image_height = panel_preview_h * panel_crop_scale;
            }

            ImGui::SetCursorPos(ImVec2(panel_crop_start_x, panel_crop_start_y));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));

            const float panel_image_width = panel_preview_w * panel_crop_scale;
            const bool needs_scroll_x = panel_image_width > (PANEL_SIZE.x + 0.5f);
            const bool needs_scroll_y = panel_image_height > (PANEL_SIZE.y + 0.5f);
            ImGuiWindowFlags child_flags = 0;
            if (!needs_scroll_x && !needs_scroll_y)
                child_flags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
            else if (needs_scroll_x)
                child_flags |= ImGuiWindowFlags_HorizontalScrollbar;

            ImGui::BeginChild("V3KN Panel Crop", panel_crop_size, ImGuiChildFlags_Borders, child_flags);
            if (zoom_changed) {
                ImGui::SetScrollX(next_scroll_x);
                ImGui::SetScrollY(next_scroll_y);
            }
            const ImVec2 content_min = ImGui::GetWindowContentRegionMin();
            const ImVec2 content_max = ImGui::GetWindowContentRegionMax();
            panel_view_width = std::max(0.f, content_max.x - content_min.x);
            panel_view_height = std::max(0.f, content_max.y - content_min.y);
            ImGui::Image(panel_preview, ImVec2(panel_preview_w * panel_crop_scale, panel_preview_h * panel_crop_scale));
            panel_crop_scroll_x = ImGui::GetScrollX();
            panel_crop_scroll_y = ImGui::GetScrollY();
            ImGui::EndChild();
            ImGui::PopStyleVar(2);

            ImGui::SetCursorPosY(panel_crop_start_y + panel_crop_size.y + ImGui::GetStyle().ItemSpacing.y);
            ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x);

            if (!panel_result_message.empty()) {
                const auto msg_color = panel_result_message.find("successfully") != std::string::npos
                    ? ImVec4(0.3f, 1.f, 0.3f, 1.f)
                    : ImVec4(1.f, 0.3f, 0.3f, 1.f);
                ImGui::TextColored(msg_color, "%s", panel_result_message.c_str());
            }

            ImGui::BeginDisabled(is_panel_uploading);
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - SMALL_BUTTON_SIZE.x - (20.f * SCALE.x));
            if (ImGui::Button(is_panel_uploading ? "Uploading..." : OK_STR.c_str(), SMALL_BUTTON_SIZE)) {
                is_panel_uploading = true;
                panel_result_message.clear();

                const auto panel_source = panel_source_data;
                const auto source_w = panel_preview_w;
                const auto source_h = panel_preview_h;
                const auto crop_scroll_x = panel_crop_scroll_x;
                const auto crop_scroll_y = panel_crop_scroll_y;
                const auto crop_scale = panel_crop_scale;
                const auto crop_zoom = panel_crop_zoom;
                PanelUploadState panel_state{ &panel_result_message, &panel_upload_success, &is_panel_uploading };
                std::thread(upload_v3kn_panel, &gui, &emuenv, &user_info, panel_source, source_w, source_h, crop_scroll_x, crop_scroll_y, crop_scale, crop_zoom, panel_state).detach();
            }
            ImGui::EndDisabled();
            ImGui::SameLine(0, 40.f * SCALE.x);
            if (ImGui::Button(CANCEL_STR.c_str(), SMALL_BUTTON_SIZE)) {
                selected_panel_path.clear();
                panel_preview = {};
                panel_preview_w = 0;
                panel_preview_h = 0;
                panel_source_data.clear();
                panel_result_message.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    // Change server
    static int selected_index = static_cast<int>(account_state.selected_server_index);
    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) + (20.f * SCALE.x));
    if (ImGui::Button("Change Server", BUTTON_SIZE)) {
        selected_index = static_cast<int>(account_state.selected_server_index);
        ImGui::OpenPopup("V3KN Change Server");
    }

    if (ImGui::BeginPopupModal("V3KN Change Server", nullptr, popup_flags)) {
        const auto title_popup = "Change V3KN Server";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title_popup).x) / 2.f);
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title_popup);
        ImGui::Separator();
        ImGui::Combo("Server", &selected_index, V3KN_SERVER_LIST_NAME.data(), static_cast<int>(V3KN_SERVER_LIST_NAME.size()));
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - SMALL_BUTTON_SIZE.x - (20.f * SCALE.x));
        if (ImGui::Button(OK_STR.c_str(), SMALL_BUTTON_SIZE)) {
            if (selected_index != static_cast<int>(account_state.selected_server_index)) {
                account_state.selected_server_index = static_cast<uint32_t>(selected_index);
                user_info = {};
                const auto new_index = account_state.selected_server_index;
                if (new_index < V3KN_SERVER_LIST_URL.size())
                    user_info.host = V3KN_SERVER_LIST_URL[new_index];
                v3kn::set_v3kn_logged_in(false);
                v3kn::save_v3kn_user_info(emuenv);
                std::thread([&gui, &emuenv]() {
                    v3kn::init_v3kn_user_info(gui, emuenv);
                }).detach();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, 40.f * SCALE.x);
        if (ImGui::Button(CANCEL_STR.c_str(), SMALL_BUTTON_SIZE))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float close_x_pos = (WINDOW_SIZE.x - BUTTON_SIZE.x) / 2.f;
    ImGui::SetCursorPosX(close_x_pos);
    if (ImGui::Button("Close", BUTTON_SIZE)) {
        gui.configuration_menu.v3kn_dialog = false;
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace gui
