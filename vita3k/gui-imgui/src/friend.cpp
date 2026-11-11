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

#include <algorithm>
#include <limits>
#include <map>
#include <numbers>
#include <optional>

#include <v3kn/account.h>
#include <v3kn/friend.h>
#include <v3kn/state.h>

#include <dialog/state.h>
#include <util/log.h>
#include <util/safe_time.h>

#include <stb_image.h>

namespace gui {

constexpr ImU32 kDefaultPanelBackground = IM_COL32(120, 120, 120, 255);

enum class FriendSortMode {
    OnlineStatus,
    OnlineId,
};

enum class FriendsScreen {
    Search = 0,
    FriendList,
    FriendRequests,
    BlockedPlayers,
};

static FriendsScreen friends_screen = FriendsScreen::FriendList;
static FriendSortMode friend_sort_mode = FriendSortMode::OnlineStatus;
static char search_online_id_input[16] = "";
static std::string search_result_message;
static std::string last_search_trigger;

const std::map<FriendsScreen, const char *> friends_screen_groups = {
    { FriendsScreen::FriendList, "friends" },
    { FriendsScreen::FriendRequests, "friend_requests" },
    { FriendsScreen::BlockedPlayers, "players_blocked" },
};

struct FriendRowLayout {
    ImVec2 item_pos;
    ImVec2 avatar_pos;
    float center_y;
    float item_height;
};

struct FriendRowData {
    bool alternate_row;
    ImU32 background_color;
    PresenceStatus presence_status;
    bool is_self;
    const char *right_text;
    const char *sub_text;
    bool draw_separator;
    ImU32 separator_color;
    bool advance_cursor;
};

void draw_friend_status_dot(ImDrawList *draw_list, const ImVec2 &pos, const ImVec2 &scale, PresenceStatus presence_status) {
    const float STATUS_DOT_RAD = 11.f * scale.x;
    ImU32 status_color = IM_COL32(177, 46, 51, 255);
    if (presence_status == PresenceStatus::Online)
        status_color = IM_COL32(0, 134, 194, 255);
    else if (presence_status == PresenceStatus::NotAvailable)
        status_color = IM_COL32(236, 186, 0, 255);

    draw_list->AddCircleFilled(pos, STATUS_DOT_RAD, status_color);
}

void refresh_current_friends_screen(GuiState &gui, EmuEnvState &emuenv, bool reload_avatar) {
    const auto group_it = friends_screen_groups.find(friends_screen);
    if (group_it != friends_screen_groups.end() && v3kn::is_v3kn_logged_in())
        v3kn::load_friends_list(emuenv, group_it->second, reload_avatar);
}

std::optional<gui::IconData> decode_v3kn_image(const v3kn::ImageData &image, const std::string &online_id) {
    if (image.bytes.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        LOG_ERROR("V3KN image is too large for {}", online_id);
        return std::nullopt;
    }
    gui::IconData decoded;
    decoded.data.reset(stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(image.bytes.data()),
        static_cast<int>(image.bytes.size()), &decoded.width, &decoded.height, nullptr, STBI_rgb_alpha));
    if (!decoded.data) {
        LOG_ERROR("Invalid V3KN image for {}", online_id);
        return std::nullopt;
    }
    return decoded;
}

ImU32 get_panel_edge_dominant_color(const gui::IconData &panel_data) {
    if (!panel_data.data || panel_data.width <= 0 || panel_data.height <= 0)
        return kDefaultPanelBackground;

    const auto *pixels = static_cast<const unsigned char *>(panel_data.data.get());
    auto accumulate_colors = [&](int sample_width, bool filter_luminance) {
        uint64_t sum_r = 0;
        uint64_t sum_g = 0;
        uint64_t sum_b = 0;
        size_t count = 0;

        const int clamped_width = std::clamp(sample_width, 1, panel_data.width);

        for (int y = 0; y < panel_data.height; ++y) {
            for (int x = 0; x < clamped_width; ++x) {
                const size_t offset = (static_cast<size_t>(y) * static_cast<size_t>(panel_data.width) + static_cast<size_t>(x)) * 4;
                const unsigned char r = pixels[offset];
                const unsigned char g = pixels[offset + 1];
                const unsigned char b = pixels[offset + 2];
                const unsigned char alpha = pixels[offset + 3];
                if (alpha < 16)
                    continue;
                if (filter_luminance) {
                    const float luminance = (0.2126f * r + 0.7152f * g + 0.0722f * b) / 255.f;
                    if (luminance < 0.08f || luminance > 0.92f)
                        continue;
                }
                sum_r += r;
                sum_g += g;
                sum_b += b;
                ++count;
            }
        }

        if (count == 0)
            return std::optional<ImU32>{};

        const auto avg_r = static_cast<unsigned char>(sum_r / count);
        const auto avg_g = static_cast<unsigned char>(sum_g / count);
        const auto avg_b = static_cast<unsigned char>(sum_b / count);
        return std::optional<ImU32>{ IM_COL32(avg_r, avg_g, avg_b, 255) };
    };

    const int narrow_width = std::min(4, panel_data.width);
    if (const auto filtered = accumulate_colors(narrow_width, true))
        return *filtered;
    return kDefaultPanelBackground;
}

static ImU32 with_alpha(ImU32 color, int alpha) {
    return (color & ~(static_cast<ImU32>(0xFF) << IM_COL32_A_SHIFT)) | (static_cast<ImU32>(std::clamp(alpha, 0, 255)) << IM_COL32_A_SHIFT);
}

static void draw_panel_edge_fade(ImDrawList *draw_list, const ImVec2 &panel_min, const ImVec2 &panel_max, ImU32 panel_background, const ImVec2 &scale) {
    const float panel_width = panel_max.x - panel_min.x;
    if (panel_width <= 0.f)
        return;

    const float fade_width = std::min(panel_width * 0.24f, 84.f * scale.x);
    if (fade_width <= 0.f)
        return;

    const ImU32 opaque = with_alpha(panel_background, 255);
    const ImU32 transparent = with_alpha(panel_background, 0);

    draw_list->AddRectFilledMultiColor(panel_min, ImVec2(panel_min.x + fade_width, panel_max.y), opaque, transparent, transparent, opaque);
    draw_list->AddRectFilledMultiColor(ImVec2(panel_max.x - fade_width, panel_min.y), panel_max, transparent, opaque, opaque, transparent);
}

static FriendRowLayout draw_friend_row(GuiState &gui, const std::string &online_id, ImDrawList *draw_list,
    const ImVec2 &item_pos, float content_width, float item_height, float avatar_size, const ImVec2 &scale,
    const ImVec2 &res_scale, const FriendRowData &row_data) {
    ImU32 panel_background = kDefaultPanelBackground;
    const bool is_friend_list_screen = (friends_screen == FriendsScreen::FriendList);
    if (is_friend_list_screen) {
        if (row_data.is_self) {
            if (gui.v3kn_panel && gui.self_panel_bg_color)
                panel_background = *gui.self_panel_bg_color;
        } else if (gui.friends_panel.contains(online_id) && gui.friends_panel[online_id]) {
            const auto color_it = gui.friends_panel_bg_color.find(online_id);
            if (color_it != gui.friends_panel_bg_color.end())
                panel_background = color_it->second;
        }
        draw_list->AddRectFilled(item_pos, ImVec2(item_pos.x + content_width, item_pos.y + item_height), panel_background);
    } else if (row_data.alternate_row) {
        draw_list->AddRectFilled(item_pos, ImVec2(item_pos.x + content_width, item_pos.y + item_height), row_data.background_color);
    }

    const ImVec2 AVATAR_POS_MIN(item_pos.x + 62.f * scale.x, item_pos.y + (item_height - avatar_size) / 2.f);
    const ImVec2 AVATAR_POS_MAX(AVATAR_POS_MIN.x + avatar_size, AVATAR_POS_MIN.y + avatar_size);
    if (row_data.is_self) {
        if (gui.v3kn_avatar)
            draw_list->AddImage(gui.v3kn_avatar, AVATAR_POS_MIN, AVATAR_POS_MAX);
        else
            draw_list->AddRectFilled(AVATAR_POS_MIN, AVATAR_POS_MAX, IM_COL32(150, 150, 150, 255));
    } else {
        if (gui.friends_avatar.contains(online_id) && gui.friends_avatar[online_id])
            draw_list->AddImage(gui.friends_avatar[online_id], AVATAR_POS_MIN, AVATAR_POS_MAX);
        else
            draw_list->AddRectFilled(AVATAR_POS_MIN, AVATAR_POS_MAX, IM_COL32(150, 150, 150, 255));
    }

    const float center_y = item_pos.y + item_height / 2.f;
    const bool align_name_top = (friends_screen == FriendsScreen::FriendList || friends_screen == FriendsScreen::FriendRequests);
    if (is_friend_list_screen) {
        const auto PANEL_SIZE = ImVec2(400.f * scale.x, 80.f * scale.y);
        const float panel_y = item_pos.y + (item_height - PANEL_SIZE.y) / 2.f;
        const ImVec2 panel_min(item_pos.x + content_width - PANEL_SIZE.x, panel_y);
        const ImVec2 panel_max(panel_min.x + PANEL_SIZE.x, panel_y + PANEL_SIZE.y);
        if (row_data.is_self) {
            if (gui.v3kn_panel)
                draw_list->AddImage(gui.v3kn_panel, panel_min, panel_max);
        } else if (gui.friends_panel.contains(online_id) && gui.friends_panel[online_id]) {
            draw_list->AddImage(gui.friends_panel[online_id], panel_min, panel_max);
        }
        draw_panel_edge_fade(draw_list, panel_min, panel_max, panel_background, scale);
    }
    ImGui::SetWindowFontScale(1.9f);
    const auto online_id_TEXT_SIZE = ImGui::CalcTextSize(online_id.c_str());
    const ImVec2 online_id_POS(AVATAR_POS_MAX.x + ((is_friend_list_screen ? 58.f : 14.f) * scale.x), center_y - (align_name_top ? online_id_TEXT_SIZE.y : (online_id_TEXT_SIZE.y / 2.f)));

    ImGui::SetCursorScreenPos(online_id_POS);
    ImGui::Text("%s", online_id.c_str());
    if (is_friend_list_screen) {
        const ImVec2 STATUS_CENTER_DOT_POS(AVATAR_POS_MAX.x + ((58.f * scale.x) / 2.f), AVATAR_POS_MIN.y + (avatar_size / 4.f));
        draw_friend_status_dot(draw_list, STATUS_CENTER_DOT_POS, scale, row_data.presence_status);

        if (row_data.sub_text && row_data.sub_text[0] != '\0') {
            if (gui.vita_icons.contains("playing_game")) {
                constexpr float icon_aspect_ratio = 1.35f;
                const float icon_height = 26 * scale.y;
                const ImVec2 icon_size(icon_height * icon_aspect_ratio, icon_height);
                const ImVec2 icon_pos(STATUS_CENTER_DOT_POS.x - (icon_size.x / 2.f), center_y + (item_height / 4.f) - (icon_size.y / 2.f));
                draw_list->AddImage(gui.vita_icons["playing_game"], icon_pos, ImVec2(icon_pos.x + icon_size.x, icon_pos.y + icon_size.y));
            }
            ImGui::SetWindowFontScale(1.38f);
            const auto sub_text_size = ImGui::CalcTextSize(row_data.sub_text);
            const ImVec2 SUB_TEXT_POS(online_id_POS.x, center_y + (item_height / 4.f) - (sub_text_size.y / 2.f));
            ImGui::SetCursorScreenPos(SUB_TEXT_POS);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", row_data.sub_text);
        }
    }

    if ((friends_screen == FriendsScreen::FriendRequests) && row_data.right_text && (row_data.right_text[0] != '\0')) {
        ImGui::SetWindowFontScale(1.2f);
        const auto right_text_size = ImGui::CalcTextSize(row_data.right_text);
        const ImVec2 RIGHT_TEXT_POS(item_pos.x + content_width - (94.f * scale.x) - right_text_size.x, center_y + (item_height / 4.f) - (right_text_size.y / 2.f));
        ImGui::SetCursorScreenPos(RIGHT_TEXT_POS);
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", row_data.right_text);
    }

    if (is_friend_list_screen) {
        const float trophy_size = 54.f * scale.x;
        const ImVec2 trophy_pos(item_pos.x + content_width - (182.f * scale.x), item_pos.y + (item_height - trophy_size) / 2.f);
        if (gui.vita_icons.contains("trophy_level")) {
            draw_list->AddImage(gui.vita_icons["trophy_level"], trophy_pos, ImVec2(trophy_pos.x + trophy_size, trophy_pos.y + trophy_size));
        } else {
            const auto DrawStar = [](ImDrawList *dl, ImVec2 center, float outer_radius, float inner_radius, ImU32 col) {
                std::array<ImVec2, 5> outer_pts{};
                std::array<ImVec2, 5> inner_pts{};
                const float angle_step = std::numbers::pi_v<float> / 5.f;
                const float start_angle = -std::numbers::pi_v<float> / 2.f;
                const float inner_radius_fill = inner_radius + (outer_radius - inner_radius) * 0.03f;
                for (int i = 0; i < 5; ++i) {
                    const float outer_angle = start_angle + angle_step * (2.f * static_cast<float>(i));
                    const float inner_angle = outer_angle + angle_step;
                    outer_pts[i] = ImVec2(center.x + std::cos(outer_angle) * outer_radius, center.y + std::sin(outer_angle) * outer_radius);
                    inner_pts[i] = ImVec2(center.x + std::cos(inner_angle) * inner_radius_fill, center.y + std::sin(inner_angle) * inner_radius_fill);
                }

                for (int i = 0; i < 5; ++i) {
                    const ImVec2 tri[3] = { outer_pts[i], inner_pts[i], inner_pts[(i + 4) % 5] };
                    dl->AddConvexPolyFilled(tri, 3, col);
                }
                dl->AddConvexPolyFilled(inner_pts.data(), static_cast<int>(inner_pts.size()), col);
            };

            const float outer_radius = trophy_size / 2.f;
            const float inner_radius = outer_radius * 0.64f;
            DrawStar(draw_list, ImVec2(trophy_pos.x + trophy_size / 2.f, trophy_pos.y + trophy_size / 2.f), outer_radius, inner_radius, IM_COL32(255, 215, 0, 255));
        }

        ImGui::SetWindowFontScale(1.94f);
        const auto trophy_text_size = ImGui::CalcTextSize(row_data.right_text);
        const auto trophy_max_text_width = ImGui::CalcTextSize("999").x;
        ImGui::SetCursorScreenPos(ImVec2(trophy_pos.x + trophy_size + (6.f * scale.x) + ((trophy_max_text_width - trophy_text_size.x) / 2.f), item_pos.y + (item_height - trophy_text_size.y) / 2.f));
        ImGui::Text("%s", row_data.right_text);
    }

    if (row_data.draw_separator)
        draw_list->AddLine(ImVec2(item_pos.x, item_pos.y + item_height - 1.f), ImVec2(item_pos.x + content_width, item_pos.y + item_height - 1.f), row_data.separator_color);

    if (row_data.advance_cursor)
        ImGui::SetCursorScreenPos(ImVec2(item_pos.x, item_pos.y + item_height));

    return { item_pos, AVATAR_POS_MIN, center_y, item_height };
}

void commit_friend_avatars(GuiState &gui, EmuEnvState &emuenv) {
    auto &friends_state = emuenv.v3kn.friends_state;
    std::lock_guard<std::mutex> lock(friends_state.images_mutex);
    for (auto &pair : friends_state.pending_friends_avatar) {
        if (auto image = decode_v3kn_image(pair.second, pair.first))
            gui.friends_avatar[pair.first] = ImGui_Texture(gui.imgui_state.get(), image->data.get(), image->width, image->height);
        else
            friends_state.loaded_avatar_ids.erase(pair.first);
    }
    friends_state.pending_friends_avatar.clear();

    for (auto &pair : friends_state.pending_friends_panel) {
        if (auto image = decode_v3kn_image(pair.second, pair.first)) {
            gui.friends_panel[pair.first] = ImGui_Texture(gui.imgui_state.get(), image->data.get(), image->width, image->height);
            gui.friends_panel_bg_color[pair.first] = get_panel_edge_dominant_color(*image);
        }
    }
    friends_state.pending_friends_panel.clear();

    if (friends_state.pending_v3kn_avatar) {
        if (auto image = decode_v3kn_image(*friends_state.pending_v3kn_avatar, emuenv.v3kn.account_state.user_info.online_id))
            gui.v3kn_avatar = ImGui_Texture(gui.imgui_state.get(), image->data.get(), image->width, image->height);
        friends_state.pending_v3kn_avatar.reset();
    }

    if (friends_state.pending_v3kn_panel) {
        if (auto image = decode_v3kn_image(*friends_state.pending_v3kn_panel, emuenv.v3kn.account_state.user_info.online_id)) {
            gui.v3kn_panel = ImGui_Texture(gui.imgui_state.get(), image->data.get(), image->width, image->height);
            gui.self_panel_bg_color = get_panel_edge_dominant_color(*image);
        }
        friends_state.pending_v3kn_panel.reset();
    }
}

static bool show_profile_options_popup = false;
static bool show_profile_confirm_dialog = false;
static bool show_friend_notice_dialog = false;

void open_friend(GuiState &gui, EmuEnvState &emuenv) {
    friends_screen = FriendsScreen::FriendList;
    gui.vita_area.friends = true;
    const auto group_it = friends_screen_groups.find(friends_screen);
    if (group_it != friends_screen_groups.end() && v3kn::is_v3kn_logged_in()) {
        const auto &friends_state = emuenv.v3kn.friends_state;
        const bool load_avatars = !friends_state.has_loaded_friends_avatars.load() || !friends_state.has_loaded_friends_panels.load();
        v3kn::load_friends_list(emuenv, group_it->second, load_avatars);
    }
}

void open_friend_requests(GuiState &gui, EmuEnvState &emuenv) {
    friends_screen = FriendsScreen::FriendRequests;
    gui.vita_area.friends = true;
    const auto group_it = friends_screen_groups.find(friends_screen);
    if (group_it != friends_screen_groups.end() && v3kn::is_v3kn_logged_in())
        v3kn::load_friends_list(emuenv, group_it->second, true);
}

void draw_friend(GuiState &gui, EmuEnvState &emuenv) {
    commit_friend_avatars(gui, emuenv);
    const auto &viewport = sdl::get_viewport();
    const ImVec2 VIEWPORT_SIZE(viewport.logical_width, viewport.logical_height);
    const ImVec2 VIEWPORT_POS(viewport.logical_x, viewport.logical_y);
    const auto RES_SCALE = ImVec2(viewport.scale_x, viewport.scale_y);
    const auto SCALE = ImVec2(RES_SCALE.x * viewport.manual_dpi_scale, RES_SCALE.y * viewport.manual_dpi_scale);
    const auto INDICATOR_SIZE = 32.f * SCALE.y;

    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INDICATOR_SIZE);
    const ImVec2 WINDOW_POS(VIEWPORT_POS.x, VIEWPORT_POS.y + INDICATOR_SIZE);

    const auto BG_PATH = "NPXS10006";

    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    const auto flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##friends_app", &gui.vita_area.friends, flags);

    const auto bg_draw_list = ImGui::GetBackgroundDrawList();
    const ImVec2 BG_POS_MAX(VIEWPORT_POS.x + VIEWPORT_SIZE.x, VIEWPORT_POS.y + VIEWPORT_SIZE.y);
    if (gui.apps_background.contains(BG_PATH))
        bg_draw_list->AddImage(gui.apps_background[BG_PATH], VIEWPORT_POS, BG_POS_MAX);
    else
        bg_draw_list->AddRectFilled(VIEWPORT_POS, BG_POS_MAX, IM_COL32(0, 120, 166, 255), 0.f, ImDrawFlags_RoundCornersAll);

    const auto &draw_list = ImGui::GetWindowDrawList();

    const auto TITLE_BAR_HEIGHT = 64.f * SCALE.y;
    const auto SIDEBAR_WIDTH = 160.f * SCALE.x;
    const auto CONTENT_WIDTH = WINDOW_SIZE.x - SIDEBAR_WIDTH;

    auto &lang = gui.lang.friends;

    const std::map<FriendsScreen, std::string> screen_labels = {
        { FriendsScreen::Search, lang["search"] },
        { FriendsScreen::FriendList, lang["friends"] },
        { FriendsScreen::FriendRequests, lang["friend_requests"] },
        { FriendsScreen::BlockedPlayers, lang["players_blocked"] }
    };

    // Title bar
    const auto &title = screen_labels.at(friends_screen);

    draw_list->AddRectFilled(WINDOW_POS, ImVec2(WINDOW_POS.x + WINDOW_SIZE.x, WINDOW_POS.y + TITLE_BAR_HEIGHT), IM_COL32(1, 168, 200, 255));
    draw_list->AddLine(ImVec2(WINDOW_POS.x, WINDOW_POS.y + TITLE_BAR_HEIGHT - 1.f), ImVec2(WINDOW_POS.x + WINDOW_SIZE.x, WINDOW_POS.y + TITLE_BAR_HEIGHT - 1.f), IM_COL32(255, 255, 255, 100));

    ImGui::SetWindowFontScale(1.64f * RES_SCALE.y);
    const auto *title_text = title.c_str();
    const auto title_size = ImGui::CalcTextSize(title_text);
    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x - title_size.x) / 2.f, (TITLE_BAR_HEIGHT - title_size.y) / 2.f));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", title_text);

    if (friends_screen == FriendsScreen::FriendList) {
        const ImVec2 SORT_SIZE(184.f * SCALE.x, 46.f * SCALE.y);
        const ImVec2 SORT_POS(WINDOW_SIZE.x - (260.f * SCALE.x), (TITLE_BAR_HEIGHT - SORT_SIZE.y) / 2.f);
        const auto sort_label = friend_sort_mode == FriendSortMode::OnlineId ? lang["online_id"].c_str() : lang["online_status"].c_str();
        const auto sort_button = fmt::format("{}", sort_label);

        ImGui::SetWindowFontScale(1.04f * RES_SCALE.y);
        ImGui::SetCursorPos(SORT_POS);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f * SCALE.x);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.55f, 0.75f, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.6f, 0.8f, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.55f, 0.75f, 0.55f));
        if (ImGui::Button(sort_button.c_str(), SORT_SIZE))
            ImGui::OpenPopup("##friends_sort");
        const ImVec2 sort_min = ImGui::GetItemRectMin();
        const ImVec2 sort_max = ImGui::GetItemRectMax();
        const float arrow_size = 8.f * SCALE.x;
        const float arrow_cx = sort_max.x - (18.f * SCALE.x);
        const float arrow_cy = (sort_min.y + sort_max.y) * 0.5f;
        draw_list->AddTriangleFilled(ImVec2(arrow_cx - arrow_size, arrow_cy - arrow_size * 0.6f),
            ImVec2(arrow_cx + arrow_size, arrow_cy - arrow_size * 0.6f),
            ImVec2(arrow_cx, arrow_cy + arrow_size), IM_COL32(255, 255, 255, 220));
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        const ImVec2 BTN_SORT_SIZE(264.f * SCALE.x, 58.f * SCALE.y);
        const ImVec2 popup_pos(WINDOW_POS.x + SORT_POS.x - (86.f * SCALE.x), WINDOW_POS.y + SORT_POS.y + SORT_SIZE.y + (30.f * SCALE.y));
        const ImVec2 popup_size(BTN_SORT_SIZE.x, (BTN_SORT_SIZE.y * 2.f) + 1.f);
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.15f, 0.16f, 0.18f, 0.99f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.f * SCALE.x);
        ImGui::SetNextWindowPos(popup_pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(popup_size, ImGuiCond_Always);
        if (ImGui::BeginPopupModal("##friends_sort", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
            const auto popup_window_pos = ImGui::GetWindowPos();
            const auto popup_window_size = ImGui::GetWindowSize();
            draw_list->AddRectFilled(popup_window_pos, ImVec2(popup_window_pos.x + popup_window_size.x, popup_window_pos.y + popup_window_size.y),
                IM_COL32(35, 35, 45, 250), 16.f * SCALE.x);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(55.f * SCALE.x, 0.f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.2f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.23f, 0.68f, 1.f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.23f, 0.68f, 1.f, 0.8f));

            const auto draw_sort_item = [&](const char *label, FriendSortMode mode) {
                const bool selected = friend_sort_mode == mode;
                if (ImGui::Button(label, BTN_SORT_SIZE)) {
                    friend_sort_mode = mode;
                    ImGui::CloseCurrentPopup();
                }
                if (selected) {
                    const ImVec2 min_pos = ImGui::GetItemRectMin();
                    const ImVec2 max_pos = ImGui::GetItemRectMax();
                    const auto &sort_draw_list = ImGui::GetWindowDrawList();
                    const ImVec2 base(min_pos.x + (30.f * SCALE.x), (min_pos.y + max_pos.y) * 0.5f);
                    const float check_scale = 1.7f;
                    const float check_thickness = 3.5f * SCALE.x;
                    const float check_left = 6.f * SCALE.x * check_scale;
                    const float check_mid = 1.f * SCALE.x * check_scale;
                    const float check_down = 6.f * SCALE.y * check_scale;
                    const float check_right = 10.f * SCALE.x * check_scale;
                    const float check_up = 6.f * SCALE.y * check_scale;
                    sort_draw_list->AddLine(ImVec2(base.x - check_left, base.y), ImVec2(base.x - check_mid, base.y + check_down), IM_COL32(255, 255, 255, 255), check_thickness);
                    sort_draw_list->AddLine(ImVec2(base.x - check_mid, base.y + check_down), ImVec2(base.x + check_right, base.y - check_up), IM_COL32(255, 255, 255, 255), check_thickness);
                }
            };

            ImGui::SetWindowFontScale(1.5f);

            draw_sort_item(lang["online_id"].c_str(), FriendSortMode::OnlineId);
            ImGui::Separator();
            draw_sort_item(lang["online_status"].c_str(), FriendSortMode::OnlineStatus);

            const auto border_color = IM_COL32(255, 255, 255, 80);
            const float border_size = 8.f * SCALE.x;
            const float half_border = border_size / 2.f;
            ImVec2 p0 = ImVec2(ImGui::GetWindowPos().x - half_border, ImGui::GetWindowPos().y - half_border);
            ImVec2 p1 = ImVec2(p0.x + ImGui::GetWindowSize().x + border_size, p0.y + ImGui::GetWindowSize().y + border_size);
            const auto popup_overlay_draw_list = ImGui::GetForegroundDrawList();
            popup_overlay_draw_list->AddRect(p0, p1, border_color, 16.f * SCALE.x, 0, border_size);

            const float arrow_base_width = 18.f * SCALE.x;
            const float arrow_tip_height = 22.f * SCALE.y;
            const float arrow_tip_from_left = 8.f * SCALE.x;
            const float arrow_base_left = popup_pos.x + (popup_size.x / 2.f) - (arrow_base_width / 2.f);
            const float arrow_base_right = arrow_base_left + arrow_base_width;
            const float arrow_base_y = p0.y - half_border + 1.f;
            const ImVec2 tip_top = ImVec2(arrow_base_left + 24.f * SCALE.x, arrow_base_y - arrow_tip_height);
            const ImVec2 tip_left = ImVec2(arrow_base_left, arrow_base_y);
            const ImVec2 tip_right = ImVec2(arrow_base_right, arrow_base_y);
            popup_overlay_draw_list->AddTriangleFilled(tip_top, tip_left, tip_right, border_color);

            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(4);

            if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
    }

    if (friends_screen != FriendsScreen::Search) {
        ImGui::SetWindowFontScale(1.04f * RES_SCALE.y);
        const ImVec2 REFRESH_SIZE(40.f * SCALE.x, 40.f * SCALE.y);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, REFRESH_SIZE.x);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.55f, 0.75f, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.12f, 0.6f, 0.8f, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.55f, 0.75f, 0.55f));
        ImGui::SetCursorPos(ImVec2(WINDOW_SIZE.x - REFRESH_SIZE.x - (8.f * SCALE.x), (TITLE_BAR_HEIGHT - REFRESH_SIZE.y) / 2.f));
        if (ImGui::Button("R", REFRESH_SIZE))
            refresh_current_friends_screen(gui, emuenv, true);
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
    }

    // Sidebar
    const auto SIDEBAR_TOP = TITLE_BAR_HEIGHT;
    const auto SIDEBAR_HEIGHT = WINDOW_SIZE.y - SIDEBAR_TOP;
    draw_list->AddRectFilled(ImVec2(WINDOW_POS.x, WINDOW_POS.y + SIDEBAR_TOP), ImVec2(WINDOW_POS.x + SIDEBAR_WIDTH, WINDOW_POS.y + WINDOW_SIZE.y), IM_COL32(0, 46, 56, 255));

    const FriendsScreen sidebar_screens[] = {
        FriendsScreen::Search,
        FriendsScreen::FriendList,
        FriendsScreen::FriendRequests,
        FriendsScreen::BlockedPlayers
    };

    const float SIDEBAR_ITEM_HEIGHT = SIDEBAR_HEIGHT / 4.f;
    const auto draw_sidebar_icon = [&](FriendsScreen screen, const ImVec2 &item_min, const ImVec2 &pos, float size, ImU32 color, ImU32 hole_color) {
        const float thickness = 2.f * SCALE.x;
        switch (screen) {
        case FriendsScreen::Search: {
            const float search_thickness = 5.f * SCALE.x;
            const float radius = (10.f * SCALE.x) + (search_thickness * 0.5f);
            const ImVec2 center(pos.x + size * 0.5f, pos.y + size * 0.5f + 6.f * SCALE.y + (search_thickness * 0.5f));
            draw_list->AddCircle(center, radius, color, 0, search_thickness);
            const ImVec2 handle_dir(0.70710677f, 0.70710677f);
            const ImVec2 handle_start(center.x + handle_dir.x * radius * 0.7f, center.y + handle_dir.y * radius * 0.7f);
            const ImVec2 handle_end(center.x + handle_dir.x * (radius + 18.f * SCALE.x), center.y + handle_dir.y * (radius + 18.f * SCALE.x));
            draw_list->AddLine(handle_start, handle_end, color, search_thickness);
            break;
        }
        case FriendsScreen::FriendList: {
            const float face = 30.f * SCALE.x;
            const float face_offset = face * 0.5f;
            const ImVec2 base_pos(item_min.x + (SIDEBAR_WIDTH - face) * 0.5f, item_min.y + 34.f * SCALE.y);
            const ImVec2 back_pos(base_pos.x + face_offset, base_pos.y - face_offset);
            const auto draw_face = [&](const ImVec2 &face_pos) {
                draw_list->AddRectFilled(face_pos, ImVec2(face_pos.x + face, face_pos.y + face), color, 6.f * SCALE.x, ImDrawFlags_RoundCornersAll);
                draw_list->AddCircleFilled(ImVec2(face_pos.x + face * 0.32f, face_pos.y + face * 0.36f), face * 0.07f, hole_color);
                draw_list->AddCircleFilled(ImVec2(face_pos.x + face * 0.68f, face_pos.y + face * 0.36f), face * 0.07f, hole_color);
                const ImVec2 mouth_start(face_pos.x + face * 0.34f, face_pos.y + face * 0.66f);
                const ImVec2 mouth_end(face_pos.x + face * 0.66f, face_pos.y + face * 0.66f);
                const ImVec2 mouth_ctrl1(face_pos.x + face * 0.44f, face_pos.y + face * 0.70f);
                const ImVec2 mouth_ctrl2(face_pos.x + face * 0.56f, face_pos.y + face * 0.70f);
                const float mouth_thickness = 3.f * SCALE.x;
                draw_list->AddBezierCubic(mouth_start, mouth_ctrl1, mouth_ctrl2, mouth_end, hole_color, mouth_thickness);
                draw_list->AddCircleFilled(mouth_start, mouth_thickness * 0.5f, hole_color);
                draw_list->AddCircleFilled(mouth_end, mouth_thickness * 0.5f, hole_color);
            };
            draw_face(back_pos);
            draw_face(base_pos);
            break;
        }
        case FriendsScreen::FriendRequests: {
            const float face = 34.f * SCALE.x;
            const float plus_size = 16.f * SCALE.x;
            const ImVec2 face_pos(item_min.x + (SIDEBAR_WIDTH - face) * 0.5f, item_min.y + 28.f * SCALE.y);
            draw_list->AddRectFilled(face_pos, ImVec2(face_pos.x + face, face_pos.y + face), color, 6.f * SCALE.x, ImDrawFlags_RoundCornersAll);
            draw_list->AddCircleFilled(ImVec2(face_pos.x + face * 0.3f, face_pos.y + face * 0.35f), face * 0.07f, hole_color);
            draw_list->AddCircleFilled(ImVec2(face_pos.x + face * 0.7f, face_pos.y + face * 0.35f), face * 0.07f, hole_color);
            const ImVec2 mouth_start(face_pos.x + face * 0.34f, face_pos.y + face * 0.67f);
            const ImVec2 mouth_end(face_pos.x + face * 0.66f, face_pos.y + face * 0.67f);
            const ImVec2 mouth_ctrl1(face_pos.x + face * 0.44f, face_pos.y + face * 0.71f);
            const ImVec2 mouth_ctrl2(face_pos.x + face * 0.56f, face_pos.y + face * 0.71f);
            const float mouth_thickness = 4.f * SCALE.x;
            draw_list->AddBezierCubic(mouth_start, mouth_ctrl1, mouth_ctrl2, mouth_end, hole_color, mouth_thickness);
            draw_list->AddCircleFilled(mouth_start, mouth_thickness * 0.5f, hole_color);
            draw_list->AddCircleFilled(mouth_end, mouth_thickness * 0.5f, hole_color);
            const float plus_left_x = face_pos.x + face + 4.f * SCALE.x;
            const float plus_top_y = item_min.y + 14.f * SCALE.y;
            const ImVec2 plus_h_start(plus_left_x, plus_top_y + plus_size * 0.5f);
            const ImVec2 plus_h_end(plus_left_x + plus_size, plus_top_y + plus_size * 0.5f);
            const ImVec2 plus_v_start(plus_left_x + plus_size * 0.5f, plus_top_y);
            const ImVec2 plus_v_end(plus_left_x + plus_size * 0.5f, plus_top_y + plus_size);
            draw_list->AddLine(plus_h_start, plus_h_end, color, 4.f * SCALE.x);
            draw_list->AddLine(plus_v_start, plus_v_end, color, 4.f * SCALE.x);
            break;
        }
        case FriendsScreen::BlockedPlayers: {
            const float face = 34.f * SCALE.x;
            const float stripe_h = 3.5f * SCALE.x;
            const float gap_h = 3.f * SCALE.x;
            const float total_stripe_h = stripe_h * 5.f + gap_h * 4.f;
            const ImVec2 face_pos(item_min.x + (SIDEBAR_WIDTH - face) * 0.5f, item_min.y + 24.f * SCALE.y);
            draw_list->AddRectFilled(face_pos, ImVec2(face_pos.x + face, face_pos.y + face), color, 6.f * SCALE.x, ImDrawFlags_RoundCornersAll);
            const float stripe_offset = (face - total_stripe_h) * 0.5f;
            for (int i = 0; i < 4; ++i) {
                const float y0 = face_pos.y + stripe_offset + stripe_h * (i + 1) + gap_h * i;
                const float y1 = y0 + gap_h;
                draw_list->AddRectFilled(ImVec2(face_pos.x, y0), ImVec2(face_pos.x + face, y1), hole_color);
            }

            const float eye_r = 3.f * SCALE.x;
            const float eye_y = face_pos.y + (10.f * SCALE.y) + eye_r;
            const float eye_rx = face_pos.x + (7.f * SCALE.x) + eye_r;
            const float eye_lx = face_pos.x + face - (7.f * SCALE.x) - eye_r;
            draw_list->AddCircleFilled(ImVec2(eye_rx, eye_y), eye_r, hole_color);
            draw_list->AddCircleFilled(ImVec2(eye_lx, eye_y), eye_r, hole_color);

            const float mouth_y = face_pos.y + stripe_offset + stripe_h * 3.f + gap_h * 3.f + stripe_h * 0.90f;
            const ImVec2 mouth_start(face_pos.x + face * 0.34f, mouth_y);
            const ImVec2 mouth_end(face_pos.x + face * 0.66f, mouth_y);
            const ImVec2 mouth_ctrl1(face_pos.x + face * 0.44f, mouth_y - stripe_h * 0.35f);
            const ImVec2 mouth_ctrl2(face_pos.x + face * 0.56f, mouth_y - stripe_h * 0.35f);
            const float mouth_thickness = 3.f * SCALE.x;
            draw_list->AddBezierCubic(mouth_start, mouth_ctrl1, mouth_ctrl2, mouth_end, hole_color, mouth_thickness);
            draw_list->AddCircleFilled(mouth_start, mouth_thickness * 0.5f, hole_color);
            draw_list->AddCircleFilled(mouth_end, mouth_thickness * 0.5f, hole_color);
            break;
        }
        }
    };

    for (int i = 0; i < 4; i++) {
        const auto screen = sidebar_screens[i];
        const auto &label = screen_labels.at(screen);
        const bool is_selected = (friends_screen == screen);
        const ImVec2 item_min(WINDOW_POS.x, WINDOW_POS.y + SIDEBAR_TOP + (i * SIDEBAR_ITEM_HEIGHT));
        const ImVec2 item_max(WINDOW_POS.x + SIDEBAR_WIDTH, item_min.y + SIDEBAR_ITEM_HEIGHT);

        if (is_selected)
            draw_list->AddRectFilled(item_min, item_max, IM_COL32(236, 237, 241, 255));

        // Icon placeholder
        const float ICON_SIZE = 36.f * SCALE.x;
        const ImVec2 icon_pos(item_min.x + (SIDEBAR_WIDTH - ICON_SIZE) / 2.f, item_min.y + 10.f * SCALE.y);
        const ImU32 icon_color = is_selected ? IM_COL32(0, 100, 113, 255) : IM_COL32(202, 209, 212, 255);
        const ImU32 icon_hole_color = is_selected ? IM_COL32(236, 237, 241, 255) : IM_COL32(0, 46, 56, 255);
        draw_sidebar_icon(screen, item_min, icon_pos, ICON_SIZE, icon_color, icon_hole_color);

        // Label
        ImGui::SetWindowFontScale(0.82f * RES_SCALE.y);
        const auto label_size = ImGui::CalcTextSize(label.c_str());
        const float label_y = (screen == FriendsScreen::FriendList || screen == FriendsScreen::Search) ? item_min.y + 76.f * SCALE.y : item_min.y + 78.f * SCALE.y;
        ImGui::SetCursorScreenPos(ImVec2(item_min.x + (SIDEBAR_WIDTH - label_size.x) / 2.f, label_y));
        ImGui::TextColored(is_selected ? ImVec4(0.f, 100.f / 255.f, 113.f / 255.f, 1.f) : ImVec4(235.f / 255.f, 239.f / 255.f, 242.f / 255.f, 1.f), "%s", label.c_str());

        // Invisible button for click
        ImGui::SetCursorScreenPos(item_min);
        const auto btn_id = fmt::format("##sidebar_{}", i);
        if (ImGui::InvisibleButton(btn_id.c_str(), ImVec2(SIDEBAR_WIDTH, SIDEBAR_ITEM_HEIGHT))) {
            friends_screen = screen;
            if (screen != FriendsScreen::Search) {
                auto &profile = emuenv.v3kn.profile_state;
                search_online_id_input[0] = '\0';
                search_result_message.clear();
                last_search_trigger.clear();
                profile.search_results.clear();
                profile.is_searching.store(false);
            }
            const auto group_it = friends_screen_groups.find(screen);
            if (group_it != friends_screen_groups.end() && v3kn::is_v3kn_logged_in()) {
                bool load_avatars = false;
                if (screen == FriendsScreen::FriendList) {
                    const auto &friends_state = emuenv.v3kn.friends_state;
                    load_avatars = !friends_state.has_loaded_friends_avatars.load() || !friends_state.has_loaded_friends_panels.load();
                } else if (screen == FriendsScreen::FriendRequests)
                    load_avatars = !emuenv.v3kn.friends_state.has_loaded_requests_avatars.load();
                else if (screen == FriendsScreen::BlockedPlayers)
                    load_avatars = !emuenv.v3kn.friends_state.has_loaded_blocked_avatars.load();
                v3kn::load_friends_list(emuenv, group_it->second, load_avatars);
            }
        }

        if (i < 3)
            draw_list->AddLine(ImVec2(item_min.x + 10.f * SCALE.x, item_max.y), ImVec2(item_max.x - 10.f * SCALE.x, item_max.y), IM_COL32(255, 255, 255, 40));
    }

    static std::string friend_notice_text;

    auto &friends_state = emuenv.v3kn.friends_state;

    if (friends_state.is_loading_friends.load()) {
        ImGui::End();
        ImGui::PopStyleVar(3);
        return;
    }

    // Content area
    const ImVec2 CONTENT_POS(SIDEBAR_WIDTH, TITLE_BAR_HEIGHT);
    const ImVec2 CONTENT_SIZE(CONTENT_WIDTH, WINDOW_SIZE.y - TITLE_BAR_HEIGHT);

    ImGui::SetCursorPos(CONTENT_POS);
    ImGui::BeginChild("##friends_content", CONTENT_SIZE, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);

    auto &user_info = emuenv.v3kn.account_state.user_info;

    switch (friends_screen) {
    case FriendsScreen::Search: {
        auto &profile = emuenv.v3kn.profile_state;

        ImGui::SetWindowFontScale(1.65f);

        const ImVec2 content_screen_pos = ImGui::GetCursorScreenPos();
        const ImVec2 search_bg_size(800.f * SCALE.x, 64.f * SCALE.y);
        const ImVec2 search_input_size(668.f * SCALE.x, 46.f * SCALE.y);
        const ImVec2 search_bg_pos((CONTENT_WIDTH - search_bg_size.x) / 2.f, 8.f * SCALE.y);
        const ImVec2 search_bg_screen_pos(content_screen_pos.x + search_bg_pos.x,
            content_screen_pos.y + search_bg_pos.y);
        const ImVec2 search_input_screen_pos(search_bg_screen_pos.x + (search_bg_size.x - search_input_size.x) / 2.f,
            search_bg_screen_pos.y + (search_bg_size.y - search_input_size.y) / 2.f);
        ImGui::GetWindowDrawList()->AddRectFilled(search_bg_screen_pos,
            ImVec2(search_bg_screen_pos.x + search_bg_size.x, search_bg_screen_pos.y + search_bg_size.y),
            IM_COL32(1, 123, 170, 210), 8.f * SCALE.x);

        ImGui::SetCursorScreenPos(search_input_screen_pos);
        ImGui::PushItemWidth(search_input_size.x);
        const float search_font_size = ImGui::GetFontSize();
        const float search_pad_y = std::max(0.f, (search_input_size.y - search_font_size) * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.f * SCALE.x, search_pad_y));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.22f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.f, 0.f, 0.f, 0.28f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.f, 0.f, 0.f, 0.32f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.92f, 1.f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.75f, 0.85f, 0.95f, 0.7f));
        const auto search_hint = fmt::format("{} ({})", lang["online_id"], lang["enter_from_3_to_16_characters"]);
        if (ImGui::InputTextWithHint("##search_online_id", search_hint.c_str(), search_online_id_input, sizeof(search_online_id_input))) {
            const std::string query(search_online_id_input);
            if (query.size() >= 3 && query != last_search_trigger && !profile.is_searching.load()) {
                last_search_trigger = query;
                profile.is_searching.store(true);
                search_result_message.clear();
                std::thread([&gui, &emuenv, query]() {
                    v3kn::search_friend(emuenv, query);
                    v3kn::load_search_avatars(emuenv);
                }).detach();
            } else if (query.size() < 3) {
                profile.search_results.clear();
                last_search_trigger.clear();
                search_result_message.clear();
            }
        }
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);
        ImGui::PopItemWidth();

        ImGui::GetWindowDrawList()->AddRect(search_input_screen_pos,
            ImVec2(search_input_screen_pos.x + search_input_size.x, search_input_screen_pos.y + search_input_size.y),
            IM_COL32(255, 255, 255, 140), 8.f * SCALE.x);

        const auto query_size = std::string(search_online_id_input).size();
        const bool show_search_hint = search_result_message.empty() && query_size < 3;
        if (show_search_hint) {
            const ImVec2 hint_pos(search_input_screen_pos.x + (16.f * SCALE.x), search_bg_screen_pos.y + search_bg_size.y + (20.f * SCALE.y));
            ImGui::SetCursorScreenPos(hint_pos);
            ImGui::SetWindowFontScale(1.86f);
            const float push_width = CONTENT_SIZE.x - ((CONTENT_SIZE.x - search_input_size.x) / 2.f) - (20.f * SCALE.x);
            ImGui::PushTextWrapPos(push_width);
            ImGui::TextColored(ImVec4(1.f, 1.f, 1.f, 0.9f), "%s", lang["search_for_player"].c_str());
            ImGui::PopTextWrapPos();
        }

        if (!search_result_message.empty()) {
            const ImVec2 hint_pos(search_input_screen_pos.x, search_bg_screen_pos.y + search_bg_size.y + 8.f * SCALE.y);
            ImGui::SetCursorScreenPos(hint_pos);
            ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "%s", search_result_message.c_str());
        }

        if (profile.is_searching.load()) {
            const float spinner_radius = 10.f * SCALE.x;
            const float dot_radius = 2.6f * SCALE.x;
            const int dot_count = 8;
            constexpr float kPi = std::numbers::pi_v<float>;
            const float angle_step = kPi * 2.f / static_cast<float>(dot_count);
            const float angle_offset = static_cast<float>(ImGui::GetTime()) * 4.f;
            const ImVec2 spinner_screen_pos(content_screen_pos.x + (CONTENT_WIDTH * 0.5f) - spinner_radius,
                search_bg_screen_pos.y + search_bg_size.y + 8.f * SCALE.y);
            ImGui::SetCursorScreenPos(spinner_screen_pos);
            const ImVec2 spinner_pos = ImGui::GetCursorScreenPos();
            const ImVec2 center(spinner_pos.x + spinner_radius, spinner_pos.y + spinner_radius);
            const auto spinner_draw_list = ImGui::GetWindowDrawList();

            for (int i = 0; i < dot_count; ++i) {
                const float angle = angle_step * static_cast<float>(i) + angle_offset;
                const float alpha = 0.25f + (static_cast<float>(i) / static_cast<float>(dot_count)) * 0.75f;
                const ImVec2 dot_pos(center.x + std::cos(angle) * spinner_radius, center.y + std::sin(angle) * spinner_radius);
                spinner_draw_list->AddCircleFilled(dot_pos, dot_radius, IM_COL32(128, 128, 128, static_cast<int>(255.f * alpha)));
            }
        }

        // Display search results
        if (!profile.search_results.empty()) {
            const float RESULTS_Y = 90.f * SCALE.y;
            const float RESULT_HEIGHT = 80.f * SCALE.y;
            const float RESULT_AVATAR_SIZE = 64.f * SCALE.x;

            ImGui::SetCursorPos(ImVec2(0.f, RESULTS_Y));

            const auto results_draw_list = ImGui::GetWindowDrawList();
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));

            for (size_t i = 0; i < profile.search_results.size(); i++) {
                const auto &entry = profile.search_results[i];
                const auto item_screen_pos = ImGui::GetCursorScreenPos();

                ImGui::PushID(static_cast<int>(5000 + i));
                if (ImGui::InvisibleButton("##search_result", ImVec2(CONTENT_WIDTH, RESULT_HEIGHT))) {
                    open_friend_profile(gui, emuenv, entry.online_id);
                }
                ImGui::PopID();

                const FriendRowData row_data{ (i % 2 == 0), IM_COL32(255, 255, 255, 15), PresenceStatus::Offline, false, nullptr, nullptr, true, IM_COL32(255, 255, 255, 30), true };
                draw_friend_row(gui, entry.online_id, results_draw_list, item_screen_pos, CONTENT_WIDTH, RESULT_HEIGHT, RESULT_AVATAR_SIZE,
                    SCALE, RES_SCALE, row_data);
            }
            ImGui::PopStyleVar();
        } else if (!profile.is_searching.load() && query_size >= 3 && !profile.last_search_query.empty()) {
            ImGui::SetCursorPos(ImVec2(20.f * SCALE.x, 90.f * SCALE.y));
            ImGui::SetWindowFontScale(1.0f * RES_SCALE.y);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), "%s", lang["no_players_found"].c_str());
        }
        break;
    }
    case FriendsScreen::FriendList: {
        const auto &friends = friends_state.friends_list;
        std::vector<const FriendInfo *> sorted_friends;
        sorted_friends.reserve(friends.size());
        for (const auto &f : friends)
            sorted_friends.push_back(&f);
        std::sort(sorted_friends.begin(), sorted_friends.end(), [&](const FriendInfo *lhs, const FriendInfo *rhs) {
            if (friend_sort_mode == FriendSortMode::OnlineStatus && (lhs->presence_status != rhs->presence_status))
                return lhs->presence_status > rhs->presence_status;
            return lhs->online_id < rhs->online_id;
        });
        const float ITEM_HEIGHT = 80.f * SCALE.y;
        const float AVATAR_SIZE = 64.f * SCALE.x;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
        const auto content_draw_list = ImGui::GetWindowDrawList();

        // Own profile first
        if (v3kn::is_v3kn_logged_in() && !user_info.online_id.empty()) {
            const auto self_now_playing = v3kn::get_friend_now_playing_title(friends_state);
            const auto item_screen_pos = ImGui::GetCursorScreenPos();

            ImGui::PushID(user_info.online_id.c_str());
            if (ImGui::InvisibleButton("##self_item", ImVec2(CONTENT_WIDTH, ITEM_HEIGHT)))
                open_friend_profile(gui, emuenv, user_info.online_id);
            ImGui::PopID();

            const std::string self_trophy_text = std::to_string(friends_state.self_info.trophy_level);
            const FriendRowData row_data{ true, IM_COL32(255, 255, 255, 25), friends_state.self_info.presence_status, true, self_trophy_text.c_str(),
                self_now_playing.empty() ? nullptr : self_now_playing.c_str(), true, IM_COL32(255, 255, 255, 50), true };
            const auto layout = draw_friend_row(gui, user_info.online_id, content_draw_list, item_screen_pos, CONTENT_WIDTH, ITEM_HEIGHT,
                AVATAR_SIZE, SCALE, RES_SCALE, row_data);

            const float diamond_size = 12.f * SCALE.x;
            const float diamond_x = item_screen_pos.x + 32.f * SCALE.x;
            content_draw_list->AddQuadFilled(ImVec2(diamond_x, layout.center_y - diamond_size), ImVec2(diamond_x + diamond_size, layout.center_y),
                ImVec2(diamond_x, layout.center_y + diamond_size), ImVec2(diamond_x - diamond_size, layout.center_y), IM_COL32(255, 255, 255, 255));
        }

        for (size_t i = 0; i < sorted_friends.size(); i++) {
            const auto &f = *sorted_friends[i];
            const auto item_screen_pos = ImGui::GetCursorScreenPos();

            ImGui::PushID(static_cast<int>(i));
            if (ImGui::InvisibleButton("##friend_item", ImVec2(CONTENT_WIDTH, ITEM_HEIGHT)))
                open_friend_profile(gui, emuenv, f.online_id);
            ImGui::PopID();
            const bool has_now_playing = f.presence_status != PresenceStatus::Offline && !f.now_playing.empty();
            const std::string trophy_text = std::to_string(f.trophy_level);
            const FriendRowData row_data{ (i % 2 == 0), IM_COL32(255, 255, 255, 15), f.presence_status, false, trophy_text.c_str(),
                has_now_playing ? f.now_playing.c_str() : nullptr, true, IM_COL32(255, 255, 255, 30), true };
            draw_friend_row(gui, f.online_id, content_draw_list, item_screen_pos, CONTENT_WIDTH, ITEM_HEIGHT, AVATAR_SIZE, SCALE,
                RES_SCALE, row_data);
        }
        ImGui::PopStyleVar();

        if (friends.empty()) {
            ImGui::SetWindowFontScale(1.1f * RES_SCALE.y);
            const auto &empty_text = lang["there_are_no_players"];
            const auto *empty_text_ptr = empty_text.c_str();
            const auto empty_size = ImGui::CalcTextSize(empty_text_ptr);
            ImGui::SetCursorPos(ImVec2((CONTENT_WIDTH - empty_size.x) / 2.f, (CONTENT_SIZE.y - empty_size.y) / 2.f));
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.f), "%s", empty_text_ptr);
        }
        break;
    }
    case FriendsScreen::FriendRequests: {
        const auto &received = friends_state.requests_received;
        const auto &sent = friends_state.requests_sent;
        const float ITEM_HEIGHT = 80.f * SCALE.y;
        const float AVATAR_SIZE = 64.f * SCALE.x;
        const ImVec2 ACCEPT_BTN_SIZE(146.f * SCALE.x, 46.f * SCALE.y);
        const float ACCEPT_BTN_PAD = 70.f * SCALE.x;
        const float SECTION_BANNER_H = 60.f * SCALE.y;
        const auto section_banner_color = IM_COL32(242, 250, 252, 255);
        const auto section_text_color = ImVec4(11.f / 255.f, 86.f / 255.f, 99.f / 255.f, 1.f);
        const auto row_bg_color = IM_COL32(136, 136, 136, 255);
        auto &profile = emuenv.v3kn.profile_state;
        const auto content_draw_list = ImGui::GetWindowDrawList();
        const float section_gap = 80.f * SCALE.y;
        ImVec2 received_banner_pos;
        ImVec2 sent_banner_pos;
        float received_section_y = 0.f;

        const auto SECTION_FONT_SIZE = 1.86f;
        ImGui::SetWindowFontScale(SECTION_FONT_SIZE);
        {
            const auto section_title = lang["received"].c_str();
            received_banner_pos = ImGui::GetCursorScreenPos();
            content_draw_list->AddRectFilled(received_banner_pos, ImVec2(received_banner_pos.x + CONTENT_WIDTH, received_banner_pos.y + SECTION_BANNER_H), section_banner_color);
            const auto section_size = ImGui::CalcTextSize(section_title);
            ImGui::SetCursorScreenPos(ImVec2(received_banner_pos.x + (CONTENT_WIDTH - section_size.x) / 2.f, received_banner_pos.y + (SECTION_BANNER_H - section_size.y) / 2.f));
            ImGui::TextColored(section_text_color, "%s", section_title);
            received_section_y = received_banner_pos.y + SECTION_BANNER_H;
            ImGui::SetCursorScreenPos(ImVec2(received_banner_pos.x, received_section_y));
        }

        const auto EMPTY_FONT_SIZE = 1.54f;
        if (received.empty()) {
            ImGui::SetWindowFontScale(EMPTY_FONT_SIZE);
            const std::string &empty_text = lang["there_are_no_players"];
            const auto empty_size = ImGui::CalcTextSize(empty_text.c_str());
            const float empty_text_y = received_section_y + (section_gap - empty_size.y) / 2.f;
            ImGui::SetCursorScreenPos(ImVec2(received_banner_pos.x, empty_text_y));
            ImGui::SetCursorPosX((CONTENT_WIDTH - empty_size.x) / 2.f);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", empty_text.c_str());
            received_section_y += section_gap;
        } else {
            ImGui::SetCursorScreenPos(ImVec2(received_banner_pos.x, received_section_y));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
            for (size_t i = 0; i < received.size(); i++) {
                const auto &r = received[i];
                const auto item_screen_pos = ImGui::GetCursorScreenPos();
                ImGui::PushID(r.online_id.c_str());
                const float row_click_width = CONTENT_WIDTH - (ACCEPT_BTN_SIZE.x + ACCEPT_BTN_PAD * 2.f);
                if (ImGui::InvisibleButton("##received_item", ImVec2(row_click_width, ITEM_HEIGHT)))
                    open_friend_profile(gui, emuenv, r.online_id);

                const FriendRowData row_data{ true, row_bg_color, PresenceStatus::Offline, false, nullptr, nullptr, false, IM_COL32(255, 255, 255, 30), false };
                draw_friend_row(gui, r.online_id, content_draw_list, item_screen_pos, CONTENT_WIDTH, ITEM_HEIGHT, AVATAR_SIZE,
                    SCALE, RES_SCALE, row_data);

                const ImVec2 accept_pos(item_screen_pos.x + CONTENT_WIDTH - ACCEPT_BTN_SIZE.x - ACCEPT_BTN_PAD,
                    item_screen_pos.y + (ITEM_HEIGHT - ACCEPT_BTN_SIZE.y) / 2.f);
                ImGui::SetWindowFontScale(1.24f);
                ImGui::SetCursorScreenPos(accept_pos);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f * SCALE.x);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.f, 1.f, 1.f, 0.2f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.3f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 1.f, 1.f, 0.4f));
                if (ImGui::Button(lang["accept"].c_str(), ACCEPT_BTN_SIZE)) {
                    profile.is_action_pending.store(true);
                    const std::string online_id = r.online_id;
                    std::thread([&gui, &emuenv, online_id]() {
                        gui.vita_area.please_wait = true;
                        const auto res = v3kn::v3kn_friend_accept(emuenv.v3kn.account_state.user_info, online_id);
                        v3kn::handle_v3kn_status(emuenv, res);
                        gui.vita_area.please_wait = false;
                        refresh_current_friends_screen(gui, emuenv, true);
                        if (res.body == "ERR:NoRequestFound") {
                            friend_notice_text = gui.lang.friends["friend_request_already_canceled"];
                            show_friend_notice_dialog = true;
                        } else if (!res.body.starts_with("OK:"))
                            emuenv.v3kn.profile_state.action_result_message = get_v3kn_error_message(gui.lang, res);
                    }).detach();
                }
                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar();

                content_draw_list->AddLine(ImVec2(item_screen_pos.x, item_screen_pos.y + ITEM_HEIGHT - 1.f), ImVec2(item_screen_pos.x + CONTENT_WIDTH, item_screen_pos.y + ITEM_HEIGHT - 1.f), IM_COL32(255, 255, 255, 30));
                ImGui::SetCursorScreenPos(ImVec2(item_screen_pos.x, item_screen_pos.y + ITEM_HEIGHT));
                ImGui::PopID();
            }
            ImGui::PopStyleVar();
            received_section_y = ImGui::GetCursorScreenPos().y + section_gap;
        }

        ImGui::SetWindowFontScale(SECTION_FONT_SIZE);
        {
            const auto section_title = lang["sent"].c_str();
            ImGui::SetCursorScreenPos(ImVec2(received_banner_pos.x, received_section_y));
            sent_banner_pos = ImGui::GetCursorScreenPos();
            content_draw_list->AddRectFilled(sent_banner_pos, ImVec2(sent_banner_pos.x + CONTENT_WIDTH, sent_banner_pos.y + SECTION_BANNER_H), section_banner_color);
            const auto section_size = ImGui::CalcTextSize(section_title);
            ImGui::SetCursorScreenPos(ImVec2(sent_banner_pos.x + (CONTENT_WIDTH - section_size.x) / 2.f, sent_banner_pos.y + (SECTION_BANNER_H - section_size.y) / 2.f));
            ImGui::TextColored(section_text_color, "%s", section_title);
            ImGui::SetCursorScreenPos(ImVec2(sent_banner_pos.x, sent_banner_pos.y + SECTION_BANNER_H));
        }

        if (sent.empty()) {
            ImGui::SetWindowFontScale(EMPTY_FONT_SIZE);
            const auto &empty_text = lang["there_are_no_players"];
            const auto empty_size = ImGui::CalcTextSize(empty_text.c_str());
            const float empty_text_y = sent_banner_pos.y + SECTION_BANNER_H + (section_gap - empty_size.y) / 2.f;
            ImGui::SetCursorScreenPos(ImVec2(sent_banner_pos.x, empty_text_y));
            ImGui::SetCursorPosX((CONTENT_WIDTH - empty_size.x) / 2.f);
            ImGui::TextColored(GUI_COLOR_TEXT, "%s", empty_text.c_str());
        } else {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
            for (size_t i = 0; i < sent.size(); i++) {
                const auto &s = sent[i];
                const auto item_screen_pos = ImGui::GetCursorScreenPos();
                ImGui::PushID(static_cast<int>(2000 + i));
                if (ImGui::InvisibleButton("##sent_item", ImVec2(CONTENT_WIDTH, ITEM_HEIGHT)))
                    open_friend_profile(gui, emuenv, s.online_id);

                const FriendRowData row_data{ true, row_bg_color, PresenceStatus::Offline, false, lang["waiting_for_friend_response"].c_str(), nullptr, true, IM_COL32(255, 255, 255, 30), true };
                draw_friend_row(gui, s.online_id, content_draw_list, item_screen_pos, CONTENT_WIDTH, ITEM_HEIGHT, AVATAR_SIZE,
                    SCALE, RES_SCALE, row_data);
                ImGui::PopID();
            }
            ImGui::PopStyleVar();
        }
        break;
    }
    case FriendsScreen::BlockedPlayers: {
        const auto &blocked = friends_state.blocked_players;
        const float ITEM_HEIGHT = 80.f * SCALE.y;
        const float AVATAR_SIZE = 64.f * SCALE.x;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
        const auto content_draw_list = ImGui::GetWindowDrawList();

        for (size_t i = 0; i < blocked.size(); i++) {
            const auto &entry = blocked[i];
            const auto item_screen_pos = ImGui::GetCursorScreenPos();

            ImGui::PushID(static_cast<int>(3000 + i));
            if (ImGui::InvisibleButton("##blocked_item", ImVec2(CONTENT_WIDTH, ITEM_HEIGHT))) {
                open_friend_profile(gui, emuenv, entry.online_id);
            }
            ImGui::PopID();

            const FriendRowData row_data{ (i % 2 == 0), IM_COL32(255, 255, 255, 15), PresenceStatus::Offline, false, nullptr, nullptr, true, IM_COL32(255, 255, 255, 30), true };
            draw_friend_row(gui, entry.online_id, content_draw_list, item_screen_pos, CONTENT_WIDTH, ITEM_HEIGHT, AVATAR_SIZE, SCALE,
                RES_SCALE, row_data);
        }
        ImGui::PopStyleVar();

        if (blocked.empty()) {
            ImGui::SetWindowFontScale(1.1f * RES_SCALE.y);
            const auto &empty_text = lang["there_are_no_players"];
            const auto *empty_text_ptr = empty_text.c_str();
            const auto empty_size = ImGui::CalcTextSize(empty_text_ptr);
            ImGui::SetCursorPos(ImVec2((CONTENT_WIDTH - empty_size.x) / 2.f, (CONTENT_SIZE.y - empty_size.y) / 2.f));
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.f), "%s", empty_text_ptr);
        }
        break;
    }
    }

    ImGui::ScrollWhenDragging();
    ImGui::EndChild();

    // Friend notice dialog (for errors or important messages after actions, like Vita)
    if (!friend_notice_text.empty())
        ImGui::OpenPopup("##friend_notice_dialog");
    const ImVec2 DLG_SIZE(760.f * SCALE.x, 436.f * SCALE.y);
    const ImVec2 DLG_POS(VIEWPORT_POS.x + (VIEWPORT_SIZE.x - DLG_SIZE.x) / 2.f, VIEWPORT_POS.y + (VIEWPORT_SIZE.y - DLG_SIZE.y) / 2.f);
    ImGui::SetNextWindowPos(DLG_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(DLG_SIZE, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.f, 0.f, 0.f, 0.2f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.f * SCALE.x);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f * SCALE.x);
    if (ImGui::BeginPopupModal("##friend_notice_dialog", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::SetWindowFontScale(1.72f);
        const ImVec2 BTN_SIZE(320.f * SCALE.x, 46.f * SCALE.y);
        const ImVec2 BTN_POS((DLG_SIZE.x - BTN_SIZE.x) / 2.f, DLG_SIZE.y - BTN_SIZE.y - (22.f * SCALE.y));
        const auto ct_sz = ImGui::CalcTextSize(friend_notice_text.c_str());
        ImGui::SetCursorPos(ImVec2((DLG_SIZE.x - ct_sz.x) / 2.f, (BTN_POS.y / 2.f) - ct_sz.y));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", friend_notice_text.c_str());
        ImGui::SetCursorPos(BTN_POS);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f * SCALE.x);
        if (ImGui::Button(::lang::get(::lang::str::ok).c_str(), BTN_SIZE)) {
            friend_notice_text.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleVar();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    ImGui::End();
    ImGui::PopStyleVar(3);
}

} // namespace gui