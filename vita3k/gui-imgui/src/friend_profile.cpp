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
#include <gui-imgui/functions.h>

#include <v3kn/account.h>
#include <v3kn/activity.h>
#include <v3kn/friend.h>
#include <v3kn/state.h>

#include <dialog/state.h>
#include <imgui_internal.h>
#include <util/log.h>
#include <util/safe_time.h>

#include <algorithm>
#include <array>
#include <optional>

namespace gui {

static bool show_profile_options_popup = false;
static bool show_profile_confirm_dialog = false;
static std::string selected_activity_online_id;
static std::string selected_activity_message;
static std::string selected_activity_title_name;
static time_t selected_activity_created_at = 0;
static std::optional<ActivityPostStatus> selected_activity_snapshot;
static std::string selected_comment_online_id;
static time_t selected_comment_created_at = 0;
static ImVec2 selected_comment_popup_pos(0.f, 0.f);
static constexpr size_t ACTIVITY_COMMENT_BUFFER_SIZE = 512;
static constexpr size_t ACTIVITY_COMMENT_MAX_LENGTH = 140 + 1;
static constexpr uint32_t ACTIVITY_COMMENT_MAX_COUNT = 20;
static constexpr uint32_t ACTIVITY_LIKE_MAX_COUNT = 100;
static std::array<char, ACTIVITY_COMMENT_BUFFER_SIZE> activity_comment_buffer{};
static std::atomic<bool> activity_interaction_pending = false;
static bool show_comment_options_popup = false;
static bool show_comment_confirm_dialog = false;
static std::atomic<bool> scroll_friend_activity_to_bottom = false;
static bool selected_activity_needs_resolution = false;

static void clear_friend_profile_assets(GuiState &gui, EmuEnvState &emuenv) {
    auto &friends_state = emuenv.v3kn.friends_state;
    std::lock_guard<std::mutex> lock(friends_state.images_mutex);
    gui.friend_profile_avatar = {};
    gui.friend_profile_avatar_online_id.clear();
    friends_state.pending_friend_profile_avatar.reset();
    friends_state.pending_friend_profile_avatar_online_id.clear();
    gui.friend_profile_panel = {};
    gui.friend_profile_panel_online_id.clear();
    gui.friend_profile_panel_bg_color.reset();
    friends_state.pending_friend_profile_panel.reset();
    friends_state.pending_friend_profile_panel_online_id.clear();
}

static void reset_friend_activity_details() {
    selected_activity_online_id.clear();
    selected_activity_message.clear();
    selected_activity_title_name.clear();
    selected_activity_created_at = 0;
    selected_activity_snapshot.reset();
    selected_comment_online_id.clear();
    selected_comment_created_at = 0;
    selected_comment_popup_pos = ImVec2(0.f, 0.f);
    activity_comment_buffer.fill('\0');
    activity_interaction_pending.store(false);
    show_comment_options_popup = false;
    show_comment_confirm_dialog = false;
    scroll_friend_activity_to_bottom.store(false);
    selected_activity_needs_resolution = false;
}

static void select_friend_activity(const ActivityPostStatus &activity) {
    selected_activity_online_id = activity.online_id;
    selected_activity_message = activity.message;
    selected_activity_title_name = activity.title_name;
    selected_activity_created_at = activity.created_at;
    selected_activity_snapshot = activity;
    selected_comment_online_id.clear();
    selected_comment_created_at = 0;
    selected_comment_popup_pos = ImVec2(0.f, 0.f);
    activity_comment_buffer.fill('\0');
    scroll_friend_activity_to_bottom.store(false);
    selected_activity_needs_resolution = false;
}

static bool is_selected_friend_activity(const ActivityPostStatus &activity) {
    return (activity.created_at == selected_activity_created_at)
        && (activity.online_id == selected_activity_online_id)
        && (activity.message == selected_activity_message)
        && (activity.title_name == selected_activity_title_name);
}

static bool is_selected_friend_activity_target(const ActivityPostStatus &activity) {
    if (activity.created_at != selected_activity_created_at)
        return false;

    return activity.online_id.empty() || (activity.online_id == selected_activity_online_id);
}

static const ActivityPostStatus *get_selected_friend_activity(const std::vector<ActivityPostStatus> &activities) {
    if (selected_activity_online_id.empty() || (selected_activity_created_at == 0))
        return nullptr;

    auto activity = std::find_if(activities.begin(), activities.end(), [](const auto &entry) {
        return is_selected_friend_activity(entry);
    });

    if ((activity == activities.end()) && selected_activity_needs_resolution) {
        activity = std::find_if(activities.begin(), activities.end(), [](const auto &entry) {
            return is_selected_friend_activity_target(entry);
        });
    }

    if (activity != activities.end()) {
        if (selected_activity_needs_resolution)
            select_friend_activity(*activity);
        else
            selected_activity_snapshot = *activity;

        return &(*activity);
    }

    return selected_activity_snapshot ? &(*selected_activity_snapshot) : nullptr;
}

static std::string get_activity_owner_online_id(const ActivityPostStatus &activity, const std::string &fallback_online_id) {
    return !activity.online_id.empty() ? activity.online_id : fallback_online_id;
}

static ImU32 with_alpha(ImU32 color, int alpha) {
    return (color & ~(static_cast<ImU32>(0xFF) << IM_COL32_A_SHIFT)) | (static_cast<ImU32>(std::clamp(alpha, 0, 255)) << IM_COL32_A_SHIFT);
}

static void draw_profile_panel_edge_fade(ImDrawList *draw_list, const ImVec2 &panel_min, const ImVec2 &panel_max, ImU32 panel_background, const ImVec2 &scale) {
    const float panel_width = panel_max.x - panel_min.x;
    const float panel_height = panel_max.y - panel_min.y;
    if ((panel_width <= 0.f) || (panel_height <= 0.f))
        return;

    const float fade_width = std::min(panel_width * 0.24f, 84.f * scale.x);
    const float fade_height = std::min(panel_height * 0.42f, 34.f * scale.y);
    if ((fade_width <= 0.f) || (fade_height <= 0.f))
        return;

    const ImU32 opaque = with_alpha(panel_background, 255);
    const ImU32 transparent = with_alpha(panel_background, 0);

    draw_list->AddRectFilledMultiColor(panel_min, ImVec2(panel_min.x + fade_width, panel_max.y), opaque, transparent, transparent, opaque);
    draw_list->AddRectFilledMultiColor(ImVec2(panel_min.x, panel_max.y - fade_height), panel_max, transparent, transparent, opaque, opaque);
}

static void draw_soft_section(ImDrawList *draw_list, const ImVec2 &section_pos, const ImVec2 &section_end, ImU32 border_color, const ImVec2 &scale) {
    draw_list->AddRectFilled(section_pos, section_end, IM_COL32(255, 255, 255, 34), 15.f * scale.x, ImDrawFlags_RoundCornersAll);
    draw_list->AddRect(section_pos, section_end, border_color, 15.f * scale.x, 0, 2.f * scale.x);
}

static void draw_profile_panel_image(ImDrawList *draw_list, ImTextureID texture, const ImVec2 &box_min, const ImVec2 &box_max,
    ImVec2 target_panel_size, float rounding, ImU32 panel_background, const ImVec2 &scale) {
    if (!texture)
        return;

    const ImVec2 PANEL_MIN(box_max.x - target_panel_size.x - (1.f * scale.x), box_min.y + (1.f * scale.y));
    const ImVec2 PANEL_MAX(PANEL_MIN.x + target_panel_size.x, PANEL_MIN.y + target_panel_size.y);

    draw_list->AddImageRounded(texture, PANEL_MIN, PANEL_MAX, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), IM_COL32_WHITE, rounding, ImDrawFlags_RoundCornersTopRight);
    draw_profile_panel_edge_fade(draw_list, PANEL_MIN, PANEL_MAX, panel_background, scale);
}

static void select_friend_activity_comment(const CommentActivity &comment) {
    selected_comment_online_id = comment.online_id;
    selected_comment_created_at = comment.created_at;
}

static const CommentActivity *get_selected_friend_activity_comment(const ActivityPostStatus &activity) {
    if (selected_comment_online_id.empty() || (selected_comment_created_at == 0))
        return nullptr;

    const auto comment = std::find_if(activity.comments_activity.begin(), activity.comments_activity.end(), [](const auto &entry) {
        return (entry.online_id == selected_comment_online_id) && (entry.created_at == selected_comment_created_at);
    });

    return (comment != activity.comments_activity.end()) ? &(*comment) : nullptr;
}

static bool can_delete_activity_comment(const ActivityPostStatus &activity, const CommentActivity &comment, const std::string &online_id, const std::string &fallback_online_id) {
    if (online_id.empty())
        return false;

    return (comment.online_id == online_id) || (get_activity_owner_online_id(activity, fallback_online_id) == online_id);
}

static std::string normalize_activity_comment_text(const std::string &text) {
    constexpr size_t MAX_COMMENT_LINES = 5;

    std::string normalized;
    normalized.reserve(text.size());

    size_t line_count = 1;
    for (const char ch : text) {
        if (ch == '\r')
            continue;

        if (ch == '\n') {
            if (line_count >= MAX_COMMENT_LINES)
                break;

            ++line_count;
        }

        normalized.push_back(ch);
    }

    return normalized;
}

static bool normalize_activity_comment_buffer() {
    const std::string current_text(activity_comment_buffer.data());
    const auto normalized = normalize_activity_comment_text(current_text);
    if (normalized == current_text)
        return false;

    activity_comment_buffer.fill('\0');
    std::copy(normalized.begin(), normalized.end(), activity_comment_buffer.begin());

    return true;
}

static uint32_t get_activity_comment_line_count(const std::string &text) {
    return static_cast<uint32_t>(std::clamp<size_t>(1 + std::count(text.begin(), text.end(), '\n'), 1, 5));
}

static bool has_reached_activity_comment_limit(const ActivityPostStatus &activity) {
    return activity.comment_count >= ACTIVITY_COMMENT_MAX_COUNT;
}

static bool has_reached_activity_like_limit(const ActivityPostStatus &activity) {
    return activity.like_count >= ACTIVITY_LIKE_MAX_COUNT;
}

static float get_activity_comment_box_height(const std::string &online_id, const std::string &comment, float wrap_width, const ImVec2 &scale) {
    const float min_height = 95.f * scale.y;
    const float max_height = 180.f * scale.y;
    const float top_padding = 14.f * scale.y;
    const float gap_after_online_id = 12.f * scale.y;
    const float bottom_padding = 16.f * scale.y;
    const float online_id_height = ImGui::CalcTextSize(online_id.c_str()).y;
    const float comment_height = comment.empty() ? ImGui::GetTextLineHeight() : ImGui::CalcTextSize(comment.c_str(), nullptr, false, wrap_width).y;

    return std::clamp(top_padding + online_id_height + gap_after_online_id + comment_height + bottom_padding, min_height, max_height);
}

static float get_activity_comment_composer_height(const std::string &comment, const ImVec2 &scale) {
    const float min_height = 66.f * scale.y;
    const float max_height = 180.f * scale.y;
    const float top_bottom_padding = 24.f * scale.y;
    const float line_height = ImGui::GetTextLineHeightWithSpacing();
    const float comment_height = std::max(line_height, get_activity_comment_line_count(comment) * line_height);

    return std::clamp(top_bottom_padding + comment_height, min_height, max_height);
}

static ImTextureID get_activity_avatar_texture(GuiState &gui, EmuEnvState &emuenv, const FriendProfileInfo &profile_info, const std::string &online_id) {
    if (online_id.empty())
        return {};

    if ((online_id == profile_info.online_id) && gui.friend_profile_avatar && (gui.friend_profile_avatar_online_id == profile_info.online_id))
        return gui.friend_profile_avatar;

    if ((online_id == emuenv.v3kn.account_state.user_info.online_id) && gui.v3kn_avatar)
        return gui.v3kn_avatar;

    const auto avatar = gui.friends_avatar.find(online_id);
    return (avatar != gui.friends_avatar.end()) ? avatar->second : ImTextureID{};
}

static ImTextureID get_activity_panel_texture(GuiState &gui, EmuEnvState &emuenv, const FriendProfileInfo &profile_info, const std::string &online_id) {
    if (online_id.empty())
        return {};

    if ((online_id == profile_info.online_id) && gui.friend_profile_panel && (gui.friend_profile_panel_online_id == profile_info.online_id))
        return gui.friend_profile_panel;

    if ((online_id == emuenv.v3kn.account_state.user_info.online_id) && gui.v3kn_panel)
        return gui.v3kn_panel;

    const auto panel = gui.friends_panel.find(online_id);
    return (panel != gui.friends_panel.end()) ? panel->second : ImTextureID{};
}

static ImU32 get_activity_panel_background(GuiState &gui, EmuEnvState &emuenv, const FriendProfileInfo &profile_info, const std::string &online_id, ImU32 fallback_color) {
    if (online_id.empty())
        return fallback_color;

    if ((online_id == profile_info.online_id) && gui.friend_profile_panel_bg_color && (gui.friend_profile_panel_online_id == profile_info.online_id))
        return *gui.friend_profile_panel_bg_color;

    if ((online_id == emuenv.v3kn.account_state.user_info.online_id) && gui.self_panel_bg_color)
        return *gui.self_panel_bg_color;

    const auto color_it = gui.friends_panel_bg_color.find(online_id);
    return (color_it != gui.friends_panel_bg_color.end()) ? color_it->second : fallback_color;
}

static ImU32 scale_activity_color(ImU32 color, float rgb_scale, float alpha_scale = 1.f) {
    auto scaled = ImGui::ColorConvertU32ToFloat4(color);
    scaled.x = std::clamp(scaled.x * rgb_scale, 0.f, 1.f);
    scaled.y = std::clamp(scaled.y * rgb_scale, 0.f, 1.f);
    scaled.z = std::clamp(scaled.z * rgb_scale, 0.f, 1.f);
    scaled.w = std::clamp(scaled.w * alpha_scale, 0.f, 1.f);
    return ImGui::ColorConvertFloat4ToU32(scaled);
}

static bool has_user_liked_activity(const ActivityPostStatus &activity, const std::string &online_id) {
    return std::find(activity.likes_online_ids.begin(), activity.likes_online_ids.end(), online_id) != activity.likes_online_ids.end();
}

static std::string get_activity_likes_text(GuiState &gui, const ActivityPostStatus &activity) {
    auto &lang = gui.lang.common.main;
    const auto &likes_online_ids = activity.likes_online_ids;
    const auto like_count = activity.like_count;
    if (likes_online_ids.empty())
        return like_count > 0 ? fmt::format(fmt::runtime(gui.lang.common.main["people_like_this"]), like_count) : std::string{};

    if (likes_online_ids.size() == 1)
        return fmt::format(fmt::runtime(lang["likes_this"]), likes_online_ids[0]);

    if (likes_online_ids.size() == 2)
        return fmt::format(fmt::runtime(lang["and_like_this"]), likes_online_ids[0], likes_online_ids[1]);

    std::string likes_list;
    for (size_t i = 0; i < likes_online_ids.size(); ++i) {
        if (!likes_list.empty())
            likes_list += ", ";
        likes_list += likes_online_ids[i];
    }

    const auto likes_summary = fmt::format(fmt::runtime(lang["people_like_this"]), like_count);
    return likes_list.empty() ? likes_summary : fmt::format("{}\n{}", likes_summary, likes_list);
}

static float get_activity_likes_box_height(const std::string &likes_text, float wrap_width, const ImVec2 &scale) {
    const float min_height = 68.f * scale.y;
    const float max_height = 180.f * scale.y;
    const float top_padding = 12.f * scale.y;
    const float bottom_padding = 14.f * scale.y;
    const float text_height = likes_text.empty() ? ImGui::GetTextLineHeight() : ImGui::CalcTextSize(likes_text.c_str(), nullptr, false, wrap_width).y;
    return std::clamp(top_padding + text_height + bottom_padding, min_height, max_height);
}

static void commit_friend_profile(GuiState &gui, EmuEnvState &emuenv) {
    auto &friends_state = emuenv.v3kn.friends_state;
    std::lock_guard<std::mutex> lock(friends_state.images_mutex);

    if (friends_state.pending_friend_profile_avatar) {
        if (auto image = decode_v3kn_image(*friends_state.pending_friend_profile_avatar, friends_state.pending_friend_profile_avatar_online_id)) {
            gui.friend_profile_avatar = ImGui_Texture(gui.imgui_state.get(), image->data.get(), image->width, image->height);
            gui.friend_profile_avatar_online_id = friends_state.pending_friend_profile_avatar_online_id;
        }
        friends_state.pending_friend_profile_avatar.reset();
        friends_state.pending_friend_profile_avatar_online_id.clear();
    }

    if (friends_state.pending_friend_profile_panel) {
        if (auto image = decode_v3kn_image(*friends_state.pending_friend_profile_panel, friends_state.pending_friend_profile_panel_online_id)) {
            gui.friend_profile_panel = ImGui_Texture(gui.imgui_state.get(), image->data.get(), image->width, image->height);
            gui.friend_profile_panel_online_id = friends_state.pending_friend_profile_panel_online_id;
            gui.friend_profile_panel_bg_color = get_panel_edge_dominant_color(*image);
        }
        friends_state.pending_friend_profile_panel.reset();
        friends_state.pending_friend_profile_panel_online_id.clear();
    }
}

static void refresh_friend_profile(GuiState &gui, EmuEnvState &emuenv, const std::string &online_id, bool reload_avatar = false) {
    auto &profile = emuenv.v3kn.profile_state;
    auto &profile_info = profile.profile_info;
    const auto &user_info = emuenv.v3kn.account_state.user_info;

    profile.action_result_message.clear();
    profile.is_action_pending.store(false);
    profile.is_loading.store(false);
    show_profile_options_popup = false;
    show_profile_confirm_dialog = false;
    show_comment_options_popup = false;
    show_comment_confirm_dialog = false;
    selected_comment_online_id.clear();
    selected_comment_created_at = 0;
    selected_comment_popup_pos = ImVec2(0.f, 0.f);

    if (reload_avatar) {
        auto &friends_state = emuenv.v3kn.friends_state;
        std::lock_guard<std::mutex> lock(friends_state.images_mutex);
        gui.friend_profile_avatar = {};
        gui.friend_profile_avatar_online_id.clear();
        friends_state.pending_friend_profile_avatar.reset();
        friends_state.pending_friend_profile_avatar_online_id.clear();
        gui.friend_profile_panel = {};
        gui.friend_profile_panel_online_id.clear();
        gui.friend_profile_panel_bg_color.reset();
        friends_state.pending_friend_profile_panel.reset();
        friends_state.pending_friend_profile_panel_online_id.clear();
    }

    if (v3kn::is_v3kn_logged_in()) {
        profile.is_loading.store(true);
        std::thread([&emuenv, &profile_info, online_id, reload_avatar]() {
            const auto res = v3kn::get_friend_profile_info(emuenv, online_id, profile_info);

            if (res && reload_avatar) {
                v3kn::load_friend_profile_avatar(emuenv, online_id);
                v3kn::load_friend_profile_panel(emuenv, online_id);
            }
            emuenv.v3kn.profile_state.is_loading.store(false);
        }).detach();
    }
}

void open_friend_activity(GuiState &gui, EmuEnvState &emuenv, const time_t created_at, const std::string &online_id) {
    reset_friend_activity_details();
    const auto target_online_id = online_id.empty() ? emuenv.v3kn.account_state.user_info.online_id : online_id;
    gui.vita_area.friend_profile = false;
    refresh_friend_profile(gui, emuenv, target_online_id, true);
    if (created_at > 0) {
        selected_activity_created_at = created_at;
        selected_activity_online_id = target_online_id;
        selected_activity_needs_resolution = true;
    }
    v3kn::get_user_activities(emuenv, target_online_id);
    gui.vita_area.friend_activity = true;
}

void open_friend_profile(GuiState &gui, EmuEnvState &emuenv, const std::string &online_id) {
    gui.vita_area.friend_profile = true;
    reset_friend_activity_details();
    refresh_friend_profile(gui, emuenv, online_id, true);
}

static std::string get_last_updated_activity_time(GuiState &gui, EmuEnvState &emuenv, const time_t &time) {
    std::string date;
    const auto normalized_time = time > static_cast<time_t>(9999999999LL) ? (time / 1000) : time;
    const auto diff_time = difftime(std::time(nullptr), normalized_time);
    constexpr auto minute = 60;
    constexpr auto hour = minute * 60;
    constexpr auto day = hour * 24;
    if (diff_time >= day) {
        tm date_tm = {};
        SAFE_LOCALTIME(&normalized_time, &date_tm);
        auto DATE_TIME = get_date_time(gui, emuenv, date_tm);
        date = fmt::format("{} {}", DATE_TIME[DateTime::DATE_MINI], DATE_TIME[DateTime::CLOCK]);
        if (emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR)
            date += fmt::format(" {}", DATE_TIME[DateTime::DAY_MOMENT]);
    } else {
        auto &lang = gui.lang.common.main;
        if (diff_time >= (hour * 2))
            date = fmt::format(fmt::runtime(lang["hours_ago"]), static_cast<uint32_t>(diff_time / hour));
        else if (diff_time >= hour)
            date = lang["one_hour_ago"];
        else if (diff_time >= (minute * 2))
            date = fmt::format(fmt::runtime(lang["minutes_ago"]), static_cast<uint32_t>(diff_time / 60));
        else
            date = lang["one_minute_ago"];
    }

    return date;
}

void draw_friend_activity(GuiState &gui, EmuEnvState &emuenv) {
    commit_friend_avatars(gui, emuenv);
    commit_friend_profile(gui, emuenv);
    const auto &viewport = sdl::get_viewport();
    const ImVec2 VIEWPORT_POS(viewport.logical_x, viewport.logical_y);
    const ImVec2 VIEWPORT_SIZE(viewport.logical_width, viewport.logical_height);
    const auto RES_SCALE = ImVec2(viewport.scale_x, viewport.scale_y);
    const auto SCALE = ImVec2(RES_SCALE.x * viewport.manual_dpi_scale, RES_SCALE.y * viewport.manual_dpi_scale);
    const float INDICATOR_SIZE(32.f * SCALE.y);

    auto &profile = emuenv.v3kn.profile_state;
    const auto &user_info = emuenv.v3kn.account_state.user_info;

    const auto &profile_info = profile.profile_info;
    ImU32 panel_background = IM_COL32(152, 152, 152, 255);
    if (gui.friend_profile_panel_bg_color && gui.friend_profile_panel_online_id == profile_info.online_id)
        panel_background = *gui.friend_profile_panel_bg_color;
    const ImU32 self_panel_background = get_activity_panel_background(gui, emuenv, profile_info, user_info.online_id, IM_COL32(152, 152, 152, 255));
    const ImU32 self_panel_background_hover = scale_activity_color(self_panel_background, 1.06f);
    const ImU32 self_panel_background_active = scale_activity_color(self_panel_background, 0.92f);
    const ImU32 activity_frame_border = IM_COL32(245, 245, 245, 230);

    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INDICATOR_SIZE);
    const ImVec2 WINDOW_POS(VIEWPORT_POS.x, VIEWPORT_POS.y + INDICATOR_SIZE);

    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::Begin("##friend_activity", &gui.vita_area.friend_activity, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();

    auto &common_lang = gui.lang.common.main;
    auto &profile_lang = gui.lang.friend_profile;

    auto *draw_list = ImGui::GetWindowDrawList();
    const auto draw_shadowed_text = [&](ImDrawList *list, const ImVec2 &pos, ImU32 color, float font_size, const std::string &text, float wrap_width = 0.f) {
        if (text.empty())
            return;

        const ImVec2 shadow_offset(1.6f * SCALE.x, 1.6f * SCALE.y);
        list->AddText(ImGui::GetFont(), font_size, ImVec2(pos.x + shadow_offset.x, pos.y + shadow_offset.y), IM_COL32(0, 0, 0, 120), text.c_str(), nullptr, wrap_width);
        list->AddText(ImGui::GetFont(), font_size, pos, color, text.c_str(), nullptr, wrap_width);
    };
    const auto draw_shadowed_ellipsis_text = [&](const std::string &text, const float wrap_width, const ImVec2 &pos, const ImVec2 &align, const ImVec4 &col, const float font_size, const uint32_t max_lines) {
        if (text.empty())
            return;

        ImGui::SetWindowFontScale(font_size / ImGui::GetFontSize());
        const ImVec2 SHADOW_OFFSET(1.6f * SCALE.x, 1.6f * SCALE.y);
        const ImVec2 WINDOW_LOCAL_POS = ImGui::GetWindowPos();
        ImGui::PushStyleVarY(ImGuiStyleVar_ItemSpacing, 2.f);
        draw_ellipsis_text(text, wrap_width, ImVec2(pos.x - WINDOW_LOCAL_POS.x + SHADOW_OFFSET.x, pos.y - WINDOW_LOCAL_POS.y + SHADOW_OFFSET.y), align, ImVec4(0.f, 0.f, 0.f, 120.f / 255.f), max_lines);
        draw_ellipsis_text(text, wrap_width, ImVec2(pos.x - WINDOW_LOCAL_POS.x, pos.y - WINDOW_LOCAL_POS.y), align, col, max_lines);
        ImGui::SetWindowFontScale(1.f);
        ImGui::PopStyleVar();
    };

    // Background
    draw_list->AddRectFilled(WINDOW_POS, ImVec2(WINDOW_POS.x + WINDOW_SIZE.x, WINDOW_POS.y + WINDOW_SIZE.y), panel_background);

    const float HEADER_HEIGHT = 64.f * SCALE.y;
    const ImVec2 HEADER_END(WINDOW_POS.x + WINDOW_SIZE.x, WINDOW_POS.y + HEADER_HEIGHT);
    draw_list->AddRectFilled(WINDOW_POS, HEADER_END, panel_background);

    const ImVec2 PANEL_SIZE(400.f * SCALE.x, 80.f * SCALE.y);
    const ImVec2 PANEL_POS(WINDOW_POS.x + WINDOW_SIZE.x - PANEL_SIZE.x, WINDOW_POS.y);
    if (gui.friend_profile_panel && gui.friend_profile_panel_online_id == profile_info.online_id) {
        ImGui::SetCursorScreenPos(PANEL_POS);
        ImGui::Image(gui.friend_profile_panel, PANEL_SIZE);
        draw_profile_panel_edge_fade(draw_list, PANEL_POS, ImVec2(PANEL_POS.x + PANEL_SIZE.x, PANEL_POS.y + PANEL_SIZE.y), panel_background, SCALE);
    }

    const float CLOSE_SIZE = 46.f * SCALE.x;
    const ImVec2 close_pos(WINDOW_POS.x + 10.f * SCALE.x, WINDOW_POS.y + 10.f * SCALE.y);
    ImGui::SetCursorScreenPos(close_pos);
    const ImU32 close_button_color = panel_background;
    const ImU32 close_button_color_hover = scale_activity_color(panel_background, 1.06f);
    const ImU32 close_button_color_active = scale_activity_color(panel_background, 0.92f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f * SCALE.x);
    ImGui::PushStyleColor(ImGuiCol_Button, close_button_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, close_button_color_hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, close_button_color_active);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.96f, 0.96f, 0.96f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
    ImGui::SetWindowFontScale(1.5f * RES_SCALE.y);
    if (ImGui::Button("X", ImVec2(CLOSE_SIZE, CLOSE_SIZE))) {
        gui.vita_area.friend_activity = false;
        gui.vita_area.friend_profile = false;
        reset_friend_activity_details();
        clear_friend_profile_assets(gui, emuenv);
        refresh_current_friends_screen(gui, emuenv);
    }
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.f);

    const bool is_self = profile_info.relation == FriendRelation::SELF;
    const std::string activities_title = is_self
        ? common_lang["activities"]
        : fmt::format(fmt::runtime(gui.lang.contacts_pa["activities_of"]), profile_info.online_id);
    const float header_title_font = ImGui::GetFontSize() * 1.45f;
    const ImVec2 header_title_size = ImGui::CalcTextSize(activities_title.c_str());
    draw_shadowed_text(draw_list,
        ImVec2(WINDOW_POS.x + (WINDOW_SIZE.x - header_title_size.x) / 2.f, WINDOW_POS.y + (HEADER_HEIGHT - header_title_font) / 2.f - (2.f * SCALE.y)),
        IM_COL32(255, 255, 255, 255), header_title_font, activities_title);

    draw_list->AddLine(ImVec2(WINDOW_POS.x, HEADER_END.y), ImVec2(WINDOW_POS.x + WINDOW_SIZE.x, HEADER_END.y), IM_COL32(210, 210, 210, 255), 2.f * SCALE.y);

    const auto user_activities = emuenv.v3kn.activity_state.posted_activities;
    const ActivityPostStatus *selected_activity = get_selected_friend_activity(user_activities);
    const bool show_activity_detail = selected_activity != nullptr;
    const std::string current_comment_text = activity_comment_buffer.data();
    const float CHILD_WIDTH = 840.f * SCALE.x;
    const ImVec2 CHILD_POS(WINDOW_POS.x + ((WINDOW_SIZE.x - CHILD_WIDTH) / 2.f), HEADER_END.y + (2.f * SCALE.y));
    const ImVec2 composer_size(832.f * SCALE.x, get_activity_comment_composer_height(current_comment_text, SCALE));
    const ImVec2 composer_pos(WINDOW_POS.x + ((WINDOW_SIZE.x - composer_size.x) / 2.f), WINDOW_POS.y + WINDOW_SIZE.y - composer_size.y - 4.f * SCALE.y);
    const ImVec2 composer_end(composer_pos.x + composer_size.x, composer_pos.y + composer_size.y);
    const float child_end_y = show_activity_detail ? composer_pos.y : (WINDOW_POS.y + WINDOW_SIZE.y);
    const ImVec2 CHILD_SIZE(CHILD_WIDTH, child_end_y - CHILD_POS.y);
    ImGui::SetCursorScreenPos(CHILD_POS);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
    ImGui::BeginChild("##friend_activity_list", CHILD_SIZE, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);

    auto *child_draw_list = ImGui::GetWindowDrawList();
    const float card_width = 810.f * SCALE.x;
    const float card_offset_x = std::max(0.f, (CHILD_SIZE.x - card_width - (12.f * SCALE.x)) / 2.f);
    const float activity_card_height = 124.f * SCALE.y;
    const float activity_card_spacing = 20.f * SCALE.y;
    const float comment_card_spacing = 14.f * SCALE.y;
    const float card_rounding = 8.f * SCALE.x;
    const ImVec2 avatar_frame_size(64.f * SCALE.x, 66.f * SCALE.y);
    const float avatar_frame_rounding = 10.f * SCALE.x;
    const float icon_size = 56.f * SCALE.x;
    const float body_font_size = ImGui::GetFontSize();
    const float meta_font_size = ImGui::GetFontSize();
    const float child_top_padding = 10.f * SCALE.y;
    const float comment_avatar_offset_x = 22.f * SCALE.x;
    const float comment_box_gap_x = 18.f * SCALE.x;

    const ImVec2 child_min = ImGui::GetWindowPos();
    ImVec2 detail_frame_min{};
    ImVec2 detail_frame_max{};
    if (show_activity_detail) {
        detail_frame_min = ImVec2(composer_pos.x, child_min.y + child_top_padding);
        detail_frame_max = ImVec2(composer_end.x, child_min.y + CHILD_SIZE.y);
        child_draw_list->AddRectFilled(detail_frame_min, detail_frame_max, panel_background, card_rounding);
        child_draw_list->AddRect(detail_frame_min, detail_frame_max, activity_frame_border, card_rounding, 0, 2.f * SCALE.x);
    }

    if (!show_activity_detail)
        ImGui::Dummy(ImVec2(0.f, child_top_padding));

    const auto draw_activity_card = [&](const ActivityPostStatus &activity, const char *id, const ImTextureID avatar_texture, bool selectable, bool draw_card_frame = true) {
        const auto time_text = get_last_updated_activity_time(gui, emuenv, activity.created_at);
        const bool has_title = (activity.type.find("friend") == std::string::npos) && !activity.title_name.empty();
        const bool has_time = !time_text.empty();
        const auto like_count = activity.like_count;
        const auto comment_count = activity.comment_count;
        const std::string online_id = !activity.online_id.empty() ? activity.online_id : profile_info.online_id;
        const std::string activity_body = (activity.type == "friendship_established") && !activity.friend_online_id.empty()
            ? fmt::format(fmt::runtime(profile_lang["is_now_friends_with"]), activity.friend_online_id)
            : (!activity.message.empty() ? activity.message : activity.title_name);
        const std::string activity_text = online_id.empty() ? activity_body : fmt::format("{} {}", online_id, activity_body);

        ImTextureID activity_icon = {};
        if (activity.type == "game_start") {
            if (gui.vita_icons.contains("game_start"))
                activity_icon = gui.vita_icons["game_start"];
        } else if (activity.type == "game") {
            if (activity.game_activity.action_uri.starts_with("https://") && gui.vita_icons.contains("www"))
                activity_icon = gui.vita_icons["www"];
        } else if (activity.type.starts_with("trophy")) {
            std::string trophy_icon = "trophy_hidden";
            if ((activity.type.find("hidden") == std::string::npos) && !activity.trophy_activity.grade.empty())
                trophy_icon = fmt::format("trophy_{}", activity.trophy_activity.grade);
            if (gui.vita_icons.contains(trophy_icon))
                activity_icon = gui.vita_icons[trophy_icon];
        } else if ((activity.type == "friendship_established") && !activity.friend_online_id.empty()) {
            activity_icon = get_activity_avatar_texture(gui, emuenv, profile_info, activity.friend_online_id);
        }

        const float current_card_width = draw_card_frame ? card_width : (detail_frame_max.x - detail_frame_min.x);
        const float current_card_offset_x = draw_card_frame ? card_offset_x : (detail_frame_min.x - child_min.x);
        const float text_start_x = 88.f * SCALE.x;
        const float icon_reserved_width = activity_icon ? (icon_size + (24.f * SCALE.x)) : 0.f;
        const float wrap_width = current_card_width - text_start_x - icon_reserved_width - (22.f * SCALE.x);

        ImGui::SetCursorPosX(current_card_offset_x);
        const bool pressed = ImGui::InvisibleButton(id, ImVec2(current_card_width, activity_card_height));
        const bool clicked = selectable && pressed;
        const ImVec2 cursor_after_button = ImGui::GetCursorPos();
        const ImVec2 card_min = ImGui::GetItemRectMin();
        const ImVec2 card_max = ImGui::GetItemRectMax();

        if (draw_card_frame) {
            child_draw_list->AddRectFilled(card_min, card_max, panel_background, card_rounding);
            child_draw_list->AddRect(card_min, card_max, activity_frame_border, card_rounding, 0, 2.f * SCALE.x);
        }

        if (gui.friend_profile_panel && gui.friend_profile_panel_online_id == profile_info.online_id)
            draw_profile_panel_image(child_draw_list, gui.friend_profile_panel, card_min, card_max, PANEL_SIZE, card_rounding, panel_background, SCALE);

        const ImVec2 avatar_frame_pos(card_min.x + (14.f * SCALE.x), card_min.y + (30.f * SCALE.y));
        const ImVec2 avatar_frame_end(avatar_frame_pos.x + avatar_frame_size.x, avatar_frame_pos.y + avatar_frame_size.y);
        child_draw_list->AddRectFilled(avatar_frame_pos, avatar_frame_end, IM_COL32(178, 178, 178, 255), avatar_frame_rounding, ImDrawFlags_RoundCornersAll);
        if (avatar_texture) {
            child_draw_list->AddImageRounded(avatar_texture, avatar_frame_pos, avatar_frame_end, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), IM_COL32_WHITE, avatar_frame_rounding, ImDrawFlags_RoundCornersAll);
        } else {
            child_draw_list->AddRectFilled(avatar_frame_pos, avatar_frame_end, IM_COL32(210, 210, 210, 255), avatar_frame_rounding, ImDrawFlags_RoundCornersAll);
        }
        child_draw_list->AddRect(avatar_frame_pos, avatar_frame_end, IM_COL32(255, 255, 255, 165), avatar_frame_rounding, 0, 1.5f * SCALE.x);

        if (activity_icon) {
            const ImVec2 icon_pos(card_max.x - (14.f * SCALE.x) - icon_size, card_min.y + ((activity_card_height - icon_size) / 2.f));
            child_draw_list->AddImageRounded(activity_icon, icon_pos, ImVec2(icon_pos.x + icon_size, icon_pos.y + icon_size), ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, icon_size * 0.5f, ImDrawFlags_RoundCornersAll);
        }

        const float meta_gap = (has_title && has_time) ? (14.f * SCALE.x) : 0.f;
        const float title_width = has_title ? ImGui::CalcTextSize(activity.title_name.c_str()).x : 0.f;
        const float time_width = has_time ? ImGui::CalcTextSize(time_text.c_str()).x : 0.f;
        const float meta_total_width = title_width + meta_gap + time_width;
        float meta_x = card_max.x - (14.f * SCALE.x) - meta_total_width;
        const float meta_y = card_min.y + (8.f * SCALE.y);
        if (has_title) {
            draw_shadowed_text(child_draw_list, ImVec2(meta_x, meta_y), IM_COL32(255, 255, 255, 245), meta_font_size, activity.title_name);
            meta_x += title_width + meta_gap;
        }
        if (has_time)
            draw_shadowed_text(child_draw_list, ImVec2(meta_x, meta_y), IM_COL32(255, 255, 255, 245), meta_font_size, time_text);

        draw_shadowed_text(child_draw_list,
            ImVec2(card_min.x + text_start_x, card_min.y + (34.f * SCALE.y)),
            IM_COL32(255, 255, 255, 255), body_font_size, activity_text, wrap_width);

        if (draw_card_frame && ((like_count > 0) || (comment_count > 0))) {
            constexpr float LIKE_COUNTER_WIDTH = 82.f;
            constexpr float COUNTER_HEIGHT = 36.f;
            constexpr float COMMENT_COUNTER_LEFT_PADDING = 20.f;
            constexpr float COMMENT_COUNTER_RIGHT_PADDING = 11.f;
            const float counter_gap = 5.f * SCALE.x;
            const float counter_right_margin = 2.f * SCALE.x;
            const float counter_font_size = body_font_size;
            const float counter_height = COUNTER_HEIGHT * SCALE.y;
            const float COMMENT_COUNTER_HEIGHT = 44.f * SCALE.y;
            const float counter_like_y = card_max.y + (4.f * SCALE.y) - (counter_height / 2.f);
            const float counter_comment_y = card_max.y - (COMMENT_COUNTER_HEIGHT / 2.f);
            float next_counter_right = card_max.x - counter_right_margin;

            if (comment_count > 0) {
                constexpr float COMMENT_BADGE_TEXTURE_WIDTH = 98.f;
                constexpr float COMMENT_BADGE_TEXTURE_HEIGHT = 44.f;
                constexpr float COMMENT_BADGE_LEFT_SLICE = 26.f;
                constexpr float COMMENT_BADGE_RIGHT_SLICE = 18.f;
                const std::string comment_counter_text = fmt::format(fmt::runtime(common_lang["comments"]), comment_count);
                const ImVec2 comment_text_size = ImGui::CalcTextSize(comment_counter_text.c_str());
                const float comment_counter_width = comment_text_size.x + ((COMMENT_COUNTER_LEFT_PADDING + COMMENT_COUNTER_RIGHT_PADDING) * SCALE.x);
                const ImVec2 comment_counter_pos(next_counter_right - comment_counter_width, counter_comment_y);

                if (gui.vita_icons.contains("comments")) {
                    draw_stretchable_horizontal_image(child_draw_list, gui.vita_icons["comments"], comment_counter_pos, ImVec2(comment_counter_width, COMMENT_COUNTER_HEIGHT),
                        COMMENT_BADGE_TEXTURE_WIDTH, COMMENT_BADGE_TEXTURE_HEIGHT, COMMENT_BADGE_LEFT_SLICE, COMMENT_BADGE_RIGHT_SLICE);
                } else {
                    child_draw_list->AddRectFilled(comment_counter_pos, ImVec2(comment_counter_pos.x + comment_counter_width, comment_counter_pos.y + COMMENT_COUNTER_HEIGHT), IM_COL32(240, 240, 240, 255), 10.f * SCALE.x);
                }

                ImGui::SetCursorScreenPos(ImVec2(comment_counter_pos.x + (COMMENT_COUNTER_LEFT_PADDING * SCALE.x), comment_counter_pos.y + ((counter_height - comment_text_size.y) / 2.f) + (4.f * SCALE.y)));
                ImGui::TextColored(GUI_COLOR_TEXT_BLACK, "%s", comment_counter_text.c_str());

                next_counter_right = comment_counter_pos.x + counter_gap;
            }

            if (like_count > 0) {
                const std::string like_counter_text = fmt::format("{}", like_count);
                const ImVec2 like_text_size = ImGui::CalcTextSize(like_counter_text.c_str());
                const float like_counter_width = LIKE_COUNTER_WIDTH * SCALE.x;
                const ImVec2 like_counter_pos(next_counter_right - like_counter_width, counter_like_y);

                if (gui.vita_icons.contains("likes")) {
                    ImGui::SetCursorScreenPos(like_counter_pos);
                    ImGui::Image(gui.vita_icons["likes"], ImVec2(like_counter_width, counter_height));
                } else {
                    child_draw_list->AddRectFilled(like_counter_pos, ImVec2(like_counter_pos.x + like_counter_width, like_counter_pos.y + counter_height), IM_COL32(240, 240, 240, 255), 10.f * SCALE.x);
                }

                ImGui::SetCursorScreenPos(ImVec2(like_counter_pos.x + (54.f * SCALE.x) - (like_text_size.x / 2.f) - (2.f * SCALE.x), like_counter_pos.y + ((counter_height - (6.f * SCALE.y) - like_text_size.y) / 2.f)));
                ImGui::TextColored(GUI_COLOR_TEXT_BLACK, "%s", like_counter_text.c_str());
            }
        }

        ImGui::SetCursorPos(cursor_after_button);

        return clicked;
    };

    enum class ActivityInteractionAction {
        Like,
        Unlike,
        Comment,
        DeleteComment,
    };

    auto queue_activity_refresh = [&](const ActivityPostStatus &activity, ActivityInteractionAction action, const std::string &comment = {}, std::optional<CommentActivity> selected_comment = std::nullopt) {
        if (activity_interaction_pending.exchange(true))
            return;

        const auto activity_copy = activity;
        const std::string online_id = profile_info.online_id;
        std::thread([&gui, &emuenv, activity_copy, online_id, action, comment, selected_comment]() {
            bool success = false;
            if (action == ActivityInteractionAction::Comment)
                success = v3kn::v3kn_activity_comment(emuenv, activity_copy, comment);
            else if ((action == ActivityInteractionAction::DeleteComment) && selected_comment)
                success = v3kn::delete_activity_comment(emuenv, activity_copy, *selected_comment);
            else if (action == ActivityInteractionAction::Unlike)
                success = v3kn::unlike_activity(emuenv, activity_copy);
            else
                success = v3kn::like_activity(emuenv, activity_copy);

            if (success) {
                v3kn::get_user_activities(emuenv, online_id);
                if (action == ActivityInteractionAction::Comment)
                    scroll_friend_activity_to_bottom.store(true);
            }

            activity_interaction_pending.store(false);
        }).detach();
    };

    if (show_activity_detail) {
        ImGui::SetCursorPos(ImVec2(0.f, child_top_padding));
        const auto activity_avatar = get_activity_avatar_texture(gui, emuenv, profile_info, selected_activity->online_id.empty() ? profile_info.online_id : selected_activity->online_id);
        draw_activity_card(*selected_activity, "##selected_activity_card", activity_avatar, false, false);
        const ImVec2 selected_activity_card_min = ImGui::GetItemRectMin();

        const bool is_own_activity = get_activity_owner_online_id(*selected_activity, profile_info.online_id) == user_info.online_id;
        const bool has_likes = selected_activity->like_count > 0;
        const bool is_liked_by_current_user = has_user_liked_activity(*selected_activity, user_info.online_id);
        const bool like_limit_reached = has_reached_activity_like_limit(*selected_activity);
        const bool like_action_disabled = activity_interaction_pending.load() || (!is_liked_by_current_user && like_limit_reached);

        if (has_likes) {
            constexpr float LIKES_SECTION_TOP_OFFSET = 128.f;
            const std::string likes_text = get_activity_likes_text(gui, *selected_activity);
            const ImVec2 LIKES_BOX_SIZE(626.f * SCALE.x, get_activity_likes_box_height(likes_text, (626.f * SCALE.x) - (28.f * SCALE.x), SCALE));
            const float likes_offset_x = card_offset_x + comment_avatar_offset_x + avatar_frame_size.x + comment_box_gap_x;
            const float likes_gap = is_own_activity ? 0.f : 4.f * SCALE.x;
            const ImVec2 LIKES_ACTION_SIZE(is_own_activity ? 0.f : std::max(0.f, (card_offset_x + card_width) - likes_offset_x - LIKES_BOX_SIZE.x - likes_gap), LIKES_BOX_SIZE.y);
            const float likes_total_width = LIKES_BOX_SIZE.x + (is_own_activity ? 0.f : likes_gap + LIKES_ACTION_SIZE.x);

            ImGui::SetCursorScreenPos(ImVec2(selected_activity_card_min.x + comment_avatar_offset_x + avatar_frame_size.x + comment_box_gap_x, selected_activity_card_min.y + (LIKES_SECTION_TOP_OFFSET * SCALE.y)));
            ImGui::Dummy(LIKES_BOX_SIZE);
            const ImVec2 likes_row_cursor = ImGui::GetCursorPos();
            const ImVec2 likes_row_min = ImGui::GetItemRectMin();
            const ImVec2 likes_box_max(likes_row_min.x + LIKES_BOX_SIZE.x, likes_row_min.y + LIKES_BOX_SIZE.y);

            draw_soft_section(child_draw_list, likes_row_min, likes_box_max, activity_frame_border, SCALE);
            draw_shadowed_text(child_draw_list,
                ImVec2(likes_row_min.x + (14.f * SCALE.x), likes_row_min.y + (18.f * SCALE.y)),
                IM_COL32(255, 255, 255, 255), body_font_size, likes_text, LIKES_BOX_SIZE.x - (28.f * SCALE.x));

            if (!is_own_activity) {
                const ImVec2 action_min(likes_box_max.x + likes_gap, likes_row_min.y);
                const ImTextureID action_icon = gui.vita_icons.contains(is_liked_by_current_user ? "unlike" : "like") ? gui.vita_icons[is_liked_by_current_user ? "unlike" : "like"] : ImTextureID{};
                const std::string action_label = is_liked_by_current_user ? common_lang["unlike"] : common_lang["like"];
                const ImVec4 action_text_color = like_action_disabled ? ImVec4(0.72f, 0.72f, 0.72f, 0.9f) : GUI_COLOR_TEXT;
                const ImVec4 action_icon_tint = like_action_disabled ? ImGui::ColorConvertU32ToFloat4(scale_activity_color(IM_COL32_WHITE, 1.f, 0.45f)) : ImVec4(1.f, 1.f, 1.f, 1.f);

                ImGui::SetCursorScreenPos(action_min);
                ImGui::BeginDisabled(like_action_disabled);
                const bool action_clicked = ImGui::InvisibleButton("##activity_toggle_like", LIKES_ACTION_SIZE);
                ImGui::EndDisabled();
                if (action_icon) {
                    const ImVec2 ACTION_ICON_SIZE(58.f * SCALE.x, 58.f * SCALE.y);
                    const ImVec2 ACTION_ICON_POS(action_min.x + (10.f * SCALE.x), action_min.y);
                    ImGui::SetCursorScreenPos(ACTION_ICON_POS);
                    ImGui::Image(action_icon, ACTION_ICON_SIZE, ImVec2(0, 0), ImVec2(1, 1), action_icon_tint);
                    ImGui::SetItemAllowOverlap();
                }
                draw_shadowed_ellipsis_text(action_label, LIKES_ACTION_SIZE.x, ImVec2(action_min.x + (LIKES_ACTION_SIZE.x / 2.f) - (1.f * SCALE.x), action_min.y + (42.f * SCALE.y)), ImVec2(0.f, 1.f), action_text_color, body_font_size * 0.88f, 2);

                if (action_clicked)
                    queue_activity_refresh(*selected_activity, is_liked_by_current_user ? ActivityInteractionAction::Unlike : ActivityInteractionAction::Like);
            }

            ImGui::SetCursorPos(likes_row_cursor);

            ImGui::Dummy(ImVec2(0.f, activity_card_spacing));
        } else if (!is_own_activity) {
            const ImVec2 like_button_size(320.f * SCALE.x, 46.f * SCALE.y);
            const float like_button_offset_x = std::max(0.f, (CHILD_SIZE.x - like_button_size.x) / 2.f);
            ImGui::SetCursorPosX(like_button_offset_x);
            ImGui::BeginDisabled(like_action_disabled);
            const bool like_clicked = ImGui::InvisibleButton("##activity_like_button", like_button_size);
            ImGui::EndDisabled();
            ImGui::SetItemAllowOverlap();
            const ImVec2 like_button_cursor = ImGui::GetCursorPos();
            const ImVec2 like_button_min = ImGui::GetItemRectMin();
            const ImVec2 like_button_max = ImGui::GetItemRectMax();
            child_draw_list->AddRect(like_button_min, like_button_max, activity_frame_border, 12.f * SCALE.x, 0, 1.5f * SCALE.x);

            if (gui.vita_icons.contains("small_like")) {
                const ImVec2 icon_pos(like_button_min.x + (98.f * SCALE.x), like_button_min.y + (1.f * SCALE.y));
                ImGui::SetCursorScreenPos(icon_pos);
                ImGui::Image(gui.vita_icons["small_like"], ImVec2(46.5f * SCALE.x, 46.5f * SCALE.y), ImVec2(0, 0), ImVec2(1, 1), like_action_disabled ? ImGui::ColorConvertU32ToFloat4(scale_activity_color(IM_COL32_WHITE, 1.f, 0.45f)) : ImVec4(1.f, 1.f, 1.f, 1.f));
                ImGui::SetItemAllowOverlap();
            }
            draw_shadowed_text(child_draw_list,
                ImVec2(like_button_min.x + (148.f * SCALE.x), like_button_min.y + (13.f * SCALE.y)),
                like_action_disabled ? IM_COL32(190, 190, 190, 255) : IM_COL32(255, 255, 255, 255), body_font_size, common_lang["like"]);

            if (like_clicked)
                queue_activity_refresh(*selected_activity, ActivityInteractionAction::Like);

            ImGui::SetCursorPos(like_button_cursor);

            ImGui::Dummy(ImVec2(0.f, activity_card_spacing));
        }

        for (size_t i = 0; i < selected_activity->comments_activity.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));

            const auto &comment = selected_activity->comments_activity[i];
            const auto comment_time = get_last_updated_activity_time(gui, emuenv, comment.created_at);
            const auto comment_avatar = get_activity_avatar_texture(gui, emuenv, profile_info, comment.online_id);
            const auto comment_panel = get_activity_panel_texture(gui, emuenv, profile_info, comment.online_id);
            const auto comment_panel_background = get_activity_panel_background(gui, emuenv, profile_info, comment.online_id, panel_background);
            const bool can_delete_comment = can_delete_activity_comment(*selected_activity, comment, user_info.online_id, profile_info.online_id);
            const ImVec2 comment_action_size(42.f * SCALE.x, 42.f * SCALE.y);
            const float comment_action_gap = can_delete_comment ? (10.f * SCALE.x) : 0.f;
            const float comment_box_width = 702.f * SCALE.x;
            const float comment_text_wrap_width = comment_box_width - (36.f * SCALE.x) - (can_delete_comment ? (comment_action_size.x + comment_action_gap) : 0.f);
            const float comment_box_height = get_activity_comment_box_height(comment.online_id, comment.comment, comment_text_wrap_width, SCALE);
            const float comment_card_height = std::max(comment_box_height, avatar_frame_size.y);

            ImGui::SetCursorPosX(card_offset_x);
            ImGui::Dummy(ImVec2(card_width, comment_card_height));
            const ImVec2 comment_cursor_after_button = ImGui::GetCursorPos();
            const ImVec2 card_min = ImGui::GetItemRectMin();
            const ImVec2 avatar_frame_pos(card_min.x + comment_avatar_offset_x, card_min.y);
            const ImVec2 avatar_frame_end(avatar_frame_pos.x + avatar_frame_size.x, avatar_frame_pos.y + avatar_frame_size.y);
            const ImVec2 comment_box_min(avatar_frame_end.x + comment_box_gap_x, card_min.y);
            const ImVec2 comment_box_max(comment_box_min.x + comment_box_width, comment_box_min.y + comment_box_height);
            const float online_id_height = ImGui::CalcTextSize(comment.online_id.c_str()).y;
            const float comment_text_y = comment_box_min.y + (14.f * SCALE.y) + online_id_height + (12.f * SCALE.y);

            child_draw_list->AddRectFilled(comment_box_min, comment_box_max, comment_panel_background, card_rounding);
            child_draw_list->AddRect(comment_box_min, comment_box_max, activity_frame_border, card_rounding, 0, 2.f * SCALE.x);

            if (comment_panel)
                draw_profile_panel_image(child_draw_list, comment_panel, comment_box_min, comment_box_max, PANEL_SIZE, card_rounding, comment_panel_background, SCALE);

            child_draw_list->AddRectFilled(avatar_frame_pos, avatar_frame_end, IM_COL32(178, 178, 178, 255), avatar_frame_rounding, ImDrawFlags_RoundCornersAll);
            if (comment_avatar) {
                child_draw_list->AddImageRounded(comment_avatar, avatar_frame_pos, avatar_frame_end, ImVec2(0.f, 0.f), ImVec2(1.f, 1.f), IM_COL32_WHITE, avatar_frame_rounding, ImDrawFlags_RoundCornersAll);
            } else {
                child_draw_list->AddRectFilled(avatar_frame_pos, avatar_frame_end, IM_COL32(210, 210, 210, 255), avatar_frame_rounding, ImDrawFlags_RoundCornersAll);
            }
            child_draw_list->AddRect(avatar_frame_pos, avatar_frame_end, IM_COL32(255, 255, 255, 165), avatar_frame_rounding, 0, 1.5f * SCALE.x);

            draw_shadowed_text(child_draw_list,
                ImVec2(comment_box_min.x + (18.f * SCALE.x), comment_box_min.y + (14.f * SCALE.y)),
                IM_COL32(255, 255, 255, 255), body_font_size, comment.online_id);

            if (!comment_time.empty()) {
                const auto time_size = ImGui::CalcTextSize(comment_time.c_str());
                draw_shadowed_text(child_draw_list,
                    ImVec2(comment_box_max.x - (14.f * SCALE.x) - (can_delete_comment ? (comment_action_size.x + comment_action_gap) : 0.f) - time_size.x, comment_box_min.y + (14.f * SCALE.y)),
                    IM_COL32(255, 255, 255, 245), meta_font_size, comment_time);
            }

            if (can_delete_comment) {
                const ImVec2 action_pos(comment_box_max.x - (14.f * SCALE.x) - comment_action_size.x, comment_box_min.y + ((comment_box_height - comment_action_size.y) / 2.f));
                ImGui::SetCursorScreenPos(action_pos);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, comment_action_size.y * 0.45f);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.96f, 0.96f, 0.92f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.98f, 0.98f, 0.98f, 0.98f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.88f, 0.88f, 0.88f, 0.98f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.18f, 0.18f, 0.18f, 1.f));
                if (ImGui::Button("...", comment_action_size) && !activity_interaction_pending.load()) {
                    const float popup_width = 200.f * SCALE.x;
                    const float popup_height = (44.f * SCALE.y) + (10.f * SCALE.y);
                    const ImVec2 action_min = ImGui::GetItemRectMin();
                    select_friend_activity_comment(comment);
                    selected_comment_popup_pos = ImVec2(action_min.x - popup_width - (8.f * SCALE.x), action_min.y - ((popup_height - comment_action_size.y) * 0.5f));
                    show_comment_options_popup = true;
                }
                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar();
            }

            draw_shadowed_text(child_draw_list,
                ImVec2(comment_box_min.x + (18.f * SCALE.x), comment_text_y),
                IM_COL32(255, 255, 255, 255), body_font_size, comment.comment, comment_text_wrap_width);

            ImGui::SetCursorPos(comment_cursor_after_button);

            if ((i + 1) < selected_activity->comments_activity.size())
                ImGui::Dummy(ImVec2(0.f, comment_card_spacing));

            ImGui::PopID();
        }
    } else {
        for (size_t i = 0; i < user_activities.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            const auto activity_avatar = get_activity_avatar_texture(gui, emuenv, profile_info, user_activities[i].online_id.empty() ? profile_info.online_id : user_activities[i].online_id);
            if (draw_activity_card(user_activities[i], "##activity_card", activity_avatar, true))
                select_friend_activity(user_activities[i]);
            if ((i + 1) < user_activities.size())
                ImGui::Dummy(ImVec2(0.f, activity_card_spacing));
            ImGui::PopID();
        }
    }

    if (show_activity_detail && scroll_friend_activity_to_bottom.exchange(false)) {
        ImGui::Dummy(ImVec2(0.f, 0.f));
        ImGui::SetScrollHereY(1.f);
    }

    ImGui::ScrollWhenDragging();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    const float BACK_SIZE = 56.f * SCALE.x;
    const ImVec2 back_pos(WINDOW_POS.x + 6.f * SCALE.x, WINDOW_POS.y + WINDOW_SIZE.y - BACK_SIZE - 8.f * SCALE.y);
    ImGui::SetCursorScreenPos(back_pos);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, BACK_SIZE);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.45f, 0.45f, 0.45f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.52f, 0.52f, 0.52f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.38f, 0.38f, 0.38f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
    ImGui::SetWindowFontScale(1.6f * RES_SCALE.y);
    if (ImGui::Button("<", ImVec2(BACK_SIZE, BACK_SIZE))) {
        if (show_activity_detail)
            reset_friend_activity_details();
        else
            gui.vita_area.friend_activity = false;
    }
    ImGui::SetWindowFontScale(1.f);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    if (show_activity_detail) {
        const bool comment_limit_reached = has_reached_activity_comment_limit(*selected_activity);
        draw_list->AddRectFilled(composer_pos, composer_end, self_panel_background, 8.f * SCALE.x);
        draw_list->AddRect(composer_pos, composer_end, activity_frame_border, 8.f * SCALE.x, 0, 1.5f * SCALE.x);

        const float input_height = composer_size.y - (24.f * SCALE.y);
        const float send_button_height = 42.f * SCALE.y;
        const float send_width = 170.f * SCALE.x;
        const float input_width = composer_size.x - send_width - (28.f * SCALE.x);
        const bool has_comment_text = activity_comment_buffer[0] != '\0';
        const bool comment_input_disabled = comment_limit_reached || activity_interaction_pending.load();
        const ImVec2 input_pos(composer_pos.x + 10.f * SCALE.x, composer_pos.y + 12.f * SCALE.y);
        const ImVec2 input_size(input_width, input_height);

        ImGui::SetCursorScreenPos(input_pos);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, comment_limit_reached ? ImVec4(0.78f, 0.78f, 0.78f, 0.96f) : ImVec4(0.93f, 0.95f, 0.92f, 0.98f));
        ImGui::PushStyleColor(ImGuiCol_Text, comment_limit_reached ? ImVec4(0.48f, 0.48f, 0.48f, 1.f) : ImVec4(0.25f, 0.25f, 0.25f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.55f, 0.55f, 0.55f, 1.f));
        ImGui::BeginDisabled(comment_input_disabled);
        ImGui::InputTextMultiline("##activity_comment_input", activity_comment_buffer.data(), ACTIVITY_COMMENT_MAX_LENGTH, input_size);
        ImGui::EndDisabled();
        if (normalize_activity_comment_buffer() && ImGui::IsItemActive()) {
            if (auto *input_state = ImGui::GetInputTextState(ImGui::GetItemID()))
                input_state->ReloadUserBufAndKeepSelection();
        }
        if ((activity_comment_buffer[0] == '\0') && !ImGui::IsItemActive()) {
            const ImVec2 hint_pos(ImGui::GetItemRectMin().x + (12.f * SCALE.x), ImGui::GetItemRectMin().y + (10.f * SCALE.y));
            draw_list->AddText(hint_pos, IM_COL32(140, 140, 140, 255), common_lang["write_comment"].c_str());
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        ImGui::SetCursorScreenPos(ImVec2(composer_pos.x + composer_size.x - send_width - (10.f * SCALE.x), composer_pos.y + 12.f * SCALE.y));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f * SCALE.x);
        ImGui::PushStyleColor(ImGuiCol_Button, scale_activity_color(self_panel_background, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, self_panel_background_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, self_panel_background_active);
        ImGui::PushStyleColor(ImGuiCol_Border, activity_frame_border);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
        ImGui::BeginDisabled(!has_comment_text || comment_input_disabled);
        const bool submit_with_button = ImGui::Button(profile_lang["send"].c_str(), ImVec2(send_width, send_button_height));
        ImGui::EndDisabled();
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);

        if (submit_with_button && has_comment_text && !comment_input_disabled) {
            normalize_activity_comment_buffer();
            const std::string comment = activity_comment_buffer.data();
            activity_comment_buffer.fill('\0');
            queue_activity_refresh(*selected_activity, ActivityInteractionAction::Comment, comment);
        }
    }

    constexpr float COMMENT_POPUP_W = 200.f;
    constexpr float COMMENT_POPUP_H = 60.f;
    if (show_comment_options_popup) {
        ImGui::OpenPopup("##activity_comment_options");
        show_comment_options_popup = false;
    }
    {
        const float popup_x = std::clamp(selected_comment_popup_pos.x, WINDOW_POS.x + (8.f * SCALE.x), (WINDOW_POS.x + WINDOW_SIZE.x) - (COMMENT_POPUP_W * SCALE.x) - (8.f * SCALE.x));
        const float popup_y = std::clamp(selected_comment_popup_pos.y, HEADER_END.y + (8.f * SCALE.y), (WINDOW_POS.y + WINDOW_SIZE.y) - (COMMENT_POPUP_H * SCALE.y) - (8.f * SCALE.y));
        ImGui::SetNextWindowPos(ImVec2(popup_x, popup_y), ImGuiCond_Appearing);
        ImGui::SetNextWindowSize(ImVec2(COMMENT_POPUP_W * SCALE.x, COMMENT_POPUP_H * SCALE.y), ImGuiCond_Appearing);
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.f, 0.f, 0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.f * SCALE.x, 4.f * SCALE.y));
        if (ImGui::BeginPopupModal("##activity_comment_options", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
            if (ImGui::Button(::lang::get(::lang::str::delete_).c_str(), ImVec2((COMMENT_POPUP_W * SCALE.x) - (8.f * SCALE.x), (COMMENT_POPUP_H * SCALE.y) - (8.f * SCALE.y)))) {
                ImGui::CloseCurrentPopup();
                show_comment_confirm_dialog = true;
            }
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseHoveringRect(ImVec2(popup_x, popup_y), ImVec2(popup_x + (COMMENT_POPUP_W * SCALE.x), popup_y + (COMMENT_POPUP_H * SCALE.y))))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();
    }

    if (show_comment_confirm_dialog) {
        ImGui::OpenPopup("##activity_comment_delete_confirm");
        show_comment_confirm_dialog = false;
    }
    const ImVec2 COMMENT_DLG_SIZE(760.f * SCALE.x, 436.f * SCALE.y);
    const ImVec2 COMMENT_DLG_POS(VIEWPORT_POS.x + (VIEWPORT_SIZE.x - COMMENT_DLG_SIZE.x) / 2.f, VIEWPORT_POS.y + (VIEWPORT_SIZE.y - COMMENT_DLG_SIZE.y) / 2.f);
    ImGui::SetNextWindowPos(COMMENT_DLG_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(COMMENT_DLG_SIZE, ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.f, 0.f, 0.f, 0.2f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.f * SCALE.x);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f * SCALE.x);
    if (ImGui::BeginPopupModal("##activity_comment_delete_confirm", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
        const auto *selected_comment = selected_activity ? get_selected_friend_activity_comment(*selected_activity) : nullptr;

        ImGui::SetWindowFontScale(1.45f * RES_SCALE.y);
        const ImVec2 BTN_SIZE(320.f * SCALE.x, 46.f * SCALE.y);
        const ImVec2 BTN_CANCEL_POS((COMMENT_DLG_SIZE.x / 2.f) - BTN_SIZE.x - (20.f * SCALE.x), COMMENT_DLG_SIZE.y - BTN_SIZE.y - (22.f * SCALE.y));
        const ImVec2 BTN_OK_POS((COMMENT_DLG_SIZE.x / 2.f) + (20.f * SCALE.x), COMMENT_DLG_SIZE.y - BTN_SIZE.y - (22.f * SCALE.y));
        const auto ct_sz = ImGui::CalcTextSize(common_lang["delete_comment"].c_str());
        ImGui::SetCursorPos(ImVec2((COMMENT_DLG_SIZE.x - ct_sz.x) / 2.f, (BTN_CANCEL_POS.y / 2.f) - ct_sz.y));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", common_lang["delete_comment"].c_str());

        ImGui::SetCursorPos(BTN_CANCEL_POS);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f * SCALE.x);
        if (ImGui::Button(::lang::get(::lang::str::cancel).c_str(), BTN_SIZE)) {
            selected_comment_online_id.clear();
            selected_comment_created_at = 0;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SetCursorPos(BTN_OK_POS);
        ImGui::BeginDisabled((selected_comment == nullptr) || activity_interaction_pending.load());
        if (ImGui::Button(::lang::get(::lang::str::ok).c_str(), BTN_SIZE) && (selected_comment != nullptr)) {
            queue_activity_refresh(*selected_activity, ActivityInteractionAction::DeleteComment, {}, *selected_comment);
            selected_comment_online_id.clear();
            selected_comment_created_at = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::PopStyleVar();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void draw_friend_profile(GuiState &gui, EmuEnvState &emuenv) {
    commit_friend_profile(gui, emuenv);
    const auto &viewport = sdl::get_viewport();
    const ImVec2 VIEWPORT_POS(viewport.logical_x, viewport.logical_y);
    const ImVec2 VIEWPORT_SIZE(viewport.logical_width, viewport.logical_height);
    const auto RES_SCALE = ImVec2(viewport.scale_x, viewport.scale_y);
    const auto SCALE = ImVec2(RES_SCALE.x * viewport.manual_dpi_scale, RES_SCALE.y * viewport.manual_dpi_scale);
    const float INDICATOR_SIZE(32.f * SCALE.y);

    auto &profile = emuenv.v3kn.profile_state;

    const auto &user_info = emuenv.v3kn.account_state.user_info;
    const auto &profile_info = profile.profile_info;
    ImU32 panel_background = IM_COL32(152, 152, 152, 255);
    if (gui.friend_profile_panel_bg_color && gui.friend_profile_panel_online_id == profile_info.online_id)
        panel_background = *gui.friend_profile_panel_bg_color;

    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INDICATOR_SIZE);
    const ImVec2 WINDOW_POS(VIEWPORT_POS.x, VIEWPORT_POS.y + INDICATOR_SIZE);

    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::Begin("##friend_profile", &gui.vita_area.friend_profile, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();

    auto &lang = gui.lang.friend_profile;

    static std::string friend_notice_text;
    static std::string confirm_dialog_action;
    static std::string confirm_dialog_text;

    const auto &draw_list = ImGui::GetWindowDrawList();
    const auto draw_glass_section = [&](const ImVec2 &section_pos, const ImVec2 &section_end, const ImVec2 &section_size, ImU32 border_color) {
        const float section_rounding = 15.f * SCALE.x;
        draw_list->AddRect(section_pos, section_end, border_color, section_rounding, 0, 2.f * SCALE.x);
        draw_list->AddRectFilled(section_pos, section_end, IM_COL32(255, 255, 255, 12), section_rounding, ImDrawFlags_RoundCornersAll);

        const ImVec2 glass_min(section_pos.x + (2.f * SCALE.x), section_pos.y + (2.f * SCALE.y));
        const ImVec2 glass_max(section_end.x - (2.f * SCALE.x), section_pos.y + (section_size.y * 0.34f));
        draw_list->AddRectFilled(glass_min, glass_max, IM_COL32(255, 255, 255, 14), section_rounding - (2.f * SCALE.x), ImDrawFlags_RoundCornersTop);

        const ImVec2 lower_glass_min(section_pos.x + (4.f * SCALE.x), section_pos.y + (section_size.y * 0.36f));
        const ImVec2 lower_glass_max(section_end.x - (4.f * SCALE.x), section_end.y - (4.f * SCALE.y));
        draw_list->AddRectFilled(lower_glass_min, lower_glass_max, IM_COL32(255, 255, 255, 4), section_rounding - (4.f * SCALE.x), ImDrawFlags_RoundCornersBottom);
    };

    // Background
    draw_list->AddRectFilled(WINDOW_POS, ImVec2(WINDOW_POS.x + WINDOW_SIZE.x, WINDOW_POS.y + WINDOW_SIZE.y), panel_background);

    // Header bar
    const float HEADER_HEIGHT = 80.f * SCALE.y;
    const ImVec2 HEADER_END(WINDOW_POS.x + WINDOW_SIZE.x, WINDOW_POS.y + HEADER_HEIGHT);
    draw_list->AddRectFilled(WINDOW_POS, HEADER_END, panel_background);

    // Close button (X)
    const float CLOSE_SIZE = 46.f * SCALE.x;
    const ImVec2 close_pos(WINDOW_POS.x + 10.f * SCALE.x, WINDOW_POS.y + 10.f * SCALE.y);
    ImGui::SetCursorScreenPos(close_pos);
    const ImU32 close_button_color = panel_background;
    const ImU32 close_button_color_hover = scale_activity_color(panel_background, 1.06f);
    const ImU32 close_button_color_active = scale_activity_color(panel_background, 0.92f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f * SCALE.x);
    ImGui::PushStyleColor(ImGuiCol_Button, close_button_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, close_button_color_hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, close_button_color_active);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.96f, 0.96f, 0.96f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
    ImGui::SetWindowFontScale(1.5f * RES_SCALE.y);
    if (ImGui::Button("X", ImVec2(CLOSE_SIZE, CLOSE_SIZE))) {
        gui.vita_area.friend_activity = false;
        gui.vita_area.friend_profile = false;
        profile.action_result_message.clear();
        show_profile_options_popup = false;
        show_profile_confirm_dialog = false;
        friend_notice_text.clear();
        reset_friend_activity_details();
        clear_friend_profile_assets(gui, emuenv);
        refresh_current_friends_screen(gui, emuenv);
    }
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar();

    const bool profile_loaded = !profile.is_loading.load();

    if (!profile_loaded) {
        ImGui::End();
        ImGui::PopStyleVar(2);
        return;
    }

    // Determine relationship status
    const bool is_friend = profile_loaded && (profile_info.relation == FriendRelation::FRIEND);
    const bool is_request_sent = profile_loaded && (profile_info.relation == FriendRelation::REQUEST_SENT);
    const bool is_request_received = profile_loaded && (profile_info.relation == FriendRelation::REQUEST_RECEIVED);
    const bool is_blocked = profile_loaded && (profile_info.relation == FriendRelation::BLOCKED);
    const bool is_self = profile_loaded && (profile_info.relation == FriendRelation::SELF);
    const bool is_none = profile_loaded && (profile_info.relation == FriendRelation::NONE);

    const bool show_invite_section = profile_loaded && (profile_info.relation != FriendRelation::SELF && profile_info.relation != FriendRelation::FRIEND);

    const ImVec2 FRIENDS_BTN_SIZE(96.f * SCALE.x, 64.f * SCALE.y);
    const ImVec2 FRIENDS_BTN_POS(WINDOW_SIZE.x - (254.f * SCALE.x), 14.f * SCALE.y);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.65f, 0.65f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.7f, 0.7f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.6f, 0.6f, 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
    ImGui::SetCursorPos(FRIENDS_BTN_POS);
    if (ImGui::Button("##friend_list", FRIENDS_BTN_SIZE)) {
        // todo - open friend list
    }
    const ImVec2 friends_btn_min = ImGui::GetItemRectMin();
    const ImVec2 friends_btn_max = ImGui::GetItemRectMax();
    const bool friends_btn_hovered = ImGui::IsItemHovered();
    const bool friends_btn_active = ImGui::IsItemActive();
    const ImU32 icon_color = IM_COL32(255, 255, 255, 255);
    const ImU32 hole_color = IM_COL32(120, 120, 120, 255);
    const float face = 18.f * SCALE.x;
    const float face_offset = face / 2.f;
    const ImVec2 face_base(friends_btn_min.x + ((FRIENDS_BTN_SIZE.x - face) / 2.f),
        friends_btn_min.y + (FRIENDS_BTN_SIZE.y / 2.f) - face);
    const ImVec2 face_back(face_base.x + face_offset, face_base.y - face_offset);
    const auto draw_face = [&](const ImVec2 &face_pos) {
        draw_list->AddRectFilled(face_pos, ImVec2(face_pos.x + face, face_pos.y + face), icon_color, 4.f * SCALE.x, ImDrawFlags_RoundCornersAll);
        draw_list->AddCircleFilled(ImVec2(face_pos.x + face * 0.32f, face_pos.y + face * 0.36f), face * 0.07f, hole_color);
        draw_list->AddCircleFilled(ImVec2(face_pos.x + face * 0.68f, face_pos.y + face * 0.36f), face * 0.07f, hole_color);
        const ImVec2 mouth_start(face_pos.x + face * 0.34f, face_pos.y + face * 0.66f);
        const ImVec2 mouth_end(face_pos.x + face * 0.66f, face_pos.y + face * 0.66f);
        const ImVec2 mouth_ctrl1(face_pos.x + face * 0.44f, face_pos.y + face * 0.70f);
        const ImVec2 mouth_ctrl2(face_pos.x + face * 0.56f, face_pos.y + face * 0.70f);
        const float mouth_thickness = 2.f * SCALE.x;
        draw_list->AddBezierCubic(mouth_start, mouth_ctrl1, mouth_ctrl2, mouth_end, hole_color, mouth_thickness);
        draw_list->AddCircleFilled(mouth_start, mouth_thickness * 0.5f, hole_color);
        draw_list->AddCircleFilled(mouth_end, mouth_thickness * 0.5f, hole_color);
    };

    ImGui::SetWindowFontScale(1.14f * RES_SCALE.y);
    ImGui::PopStyleVar();

    const ImVec2 PANEL_SIZE(400.f * SCALE.x, 80.f * SCALE.y);
    const ImVec2 PANEL_POS(WINDOW_POS.x + WINDOW_SIZE.x - PANEL_SIZE.x, WINDOW_POS.y);
    const ImVec2 PANEL_END(PANEL_POS.x + PANEL_SIZE.x, PANEL_POS.y + PANEL_SIZE.y);
    draw_list->AddRectFilled(PANEL_POS, PANEL_END, panel_background);
    if (gui.friend_profile_panel && gui.friend_profile_panel_online_id == profile_info.online_id) {
        ImGui::SetCursorScreenPos(PANEL_POS);
        ImGui::Image(gui.friend_profile_panel, PANEL_SIZE);
        draw_profile_panel_edge_fade(draw_list, PANEL_POS, PANEL_END, panel_background, SCALE);
    }

    const ImU32 friends_btn_top = friends_btn_active ? IM_COL32(255, 255, 255, 70) : (friends_btn_hovered ? IM_COL32(255, 255, 255, 92) : IM_COL32(255, 255, 255, 58));
    const ImU32 friends_btn_bottom = friends_btn_active ? IM_COL32(120, 120, 120, 70) : (friends_btn_hovered ? IM_COL32(120, 120, 120, 86) : IM_COL32(120, 120, 120, 54));
    draw_list->AddRectFilledMultiColor(friends_btn_min, friends_btn_max, friends_btn_top, friends_btn_top, friends_btn_bottom, friends_btn_bottom);
    draw_list->AddRect(friends_btn_min, friends_btn_max, IM_COL32(255, 255, 255, 105), 10.f * SCALE.x, ImDrawFlags_RoundCornersAll, 1.2f * SCALE.x);
    draw_list->AddLine(ImVec2(friends_btn_min.x + (2.f * SCALE.x), friends_btn_min.y + (1.5f * SCALE.y)),
        ImVec2(friends_btn_max.x - (2.f * SCALE.x), friends_btn_min.y + (1.5f * SCALE.y)), IM_COL32(255, 255, 255, 95), 1.f * SCALE.x);
    draw_face(face_back);
    draw_face(face_base);

    if (is_friend || is_self) {
        const auto count_text = fmt::format("{}", profile_info.friends.size());
        const auto count_size = ImGui::CalcTextSize(count_text.c_str());
        const ImVec2 COUNT_POS(friends_btn_min.x + (FRIENDS_BTN_SIZE.x - count_size.x) / 2.f,
            friends_btn_max.y - (FRIENDS_BTN_SIZE.y / 4.f) - (count_size.y / 2.f));
        ImGui::SetCursorScreenPos(COUNT_POS);
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", count_text.c_str());
    } else {
        const char *lock_text = "LOCK";
        const auto lock_size = ImGui::CalcTextSize(lock_text);
        const ImVec2 lock_pos(friends_btn_min.x + (FRIENDS_BTN_SIZE.x - lock_size.x) / 2.f,
            friends_btn_max.y - lock_size.y - 8.f * SCALE.y);
        ImGui::SetCursorScreenPos(lock_pos);
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", lock_text);
    }

    const float REFRESH_SIZE = 46.f * SCALE.x;
    const ImVec2 refresh_pos(WINDOW_SIZE.x - (54.f * SCALE.x), 24.f * SCALE.y);
    ImGui::SetCursorPos(refresh_pos);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, REFRESH_SIZE);
    ImGui::PushStyleColor(ImGuiCol_Button, close_button_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, close_button_color_hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, close_button_color_active);
    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT);
    ImGui::SetWindowFontScale(1.3f * RES_SCALE.y);
    if (ImGui::Button("R", ImVec2(REFRESH_SIZE, REFRESH_SIZE))) {
        refresh_friend_profile(gui, emuenv, profile_info.online_id, true);
    }
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    // Self diamond (between close button and avatar, like Vita)
    const float DIAMOND_HALF = 13.f * SCALE.x; // demi-diagonale = 13px

    // Avatar
    const float AVATAR_SIZE = 64.f * SCALE.x;
    const ImVec2 AVATAR_POS(WINDOW_POS.x + 150.f * SCALE.x, WINDOW_POS.y + 14.f * SCALE.y);
    const ImVec2 AVATAR_CENTER(AVATAR_POS.x + (AVATAR_SIZE / 2.f), AVATAR_POS.y + (AVATAR_SIZE / 2.f));

    if (is_self) {
        const ImVec2 DM_POS(WINDOW_POS.x + (70.f * SCALE.x) + DIAMOND_HALF, AVATAR_CENTER.y);

        draw_list->AddQuadFilled(
            ImVec2(DM_POS.x, DM_POS.y - DIAMOND_HALF),
            ImVec2(DM_POS.x + DIAMOND_HALF, DM_POS.y),
            ImVec2(DM_POS.x, DM_POS.y + DIAMOND_HALF),
            ImVec2(DM_POS.x - DIAMOND_HALF, DM_POS.y),
            IM_COL32(255, 255, 255, 255));
    }

    const ImVec2 AVATAR_END(AVATAR_POS.x + AVATAR_SIZE, AVATAR_POS.y + AVATAR_SIZE);

    if (gui.friend_profile_avatar && gui.friend_profile_avatar_online_id == profile_info.online_id) {
        ImGui::SetCursorScreenPos(AVATAR_POS);
        ImGui::Image(gui.friend_profile_avatar, ImVec2(AVATAR_SIZE, AVATAR_SIZE));
    } else {
        draw_list->AddRectFilled(AVATAR_POS, AVATAR_END, IM_COL32(180, 180, 180, 255));
    }

    // Status indicator / self diamond between avatar and name
    if (is_self || is_friend) {
        const float STATUS_DOT_RAD = 11.f * SCALE.x;
        const ImVec2 CENTER_DOT_POS(AVATAR_END.x + ((58.f * SCALE.x) / 2.f), AVATAR_POS.y + (AVATAR_SIZE / 2.f));
        draw_friend_status_dot(draw_list, CENTER_DOT_POS, SCALE, profile_info.presence_status);
    }

    // Online ID
    ImGui::SetWindowFontScale(1.4f * RES_SCALE.y);
    const ImVec2 online_id_POS(AVATAR_END.x + (58.f * SCALE.x), AVATAR_POS.y + (AVATAR_SIZE - ImGui::GetFontSize()) / 2.f);
    ImGui::SetCursorScreenPos(online_id_POS);
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", profile_info.online_id.c_str());

    // Content
    const float PAD = 20.f * SCALE.x;
    float cursor_y = WINDOW_POS.y + HEADER_HEIGHT + 12.f * SCALE.y;
    const float separator_y = WINDOW_POS.y + 96.f * SCALE.y;
    draw_list->AddLine(ImVec2(WINDOW_POS.x, separator_y), ImVec2(WINDOW_POS.x + WINDOW_SIZE.x, separator_y), IM_COL32(160, 160, 160, 255));
    cursor_y = separator_y + 8.f * SCALE.y;

    const auto section_border_color = IM_COL32(198, 198, 198, 255);

    const std::string now_playing = is_self ? v3kn::get_friend_now_playing_title(emuenv.v3kn.friends_state) : profile_info.now_playing;
    if ((is_self || is_friend) && !now_playing.empty()) {
        const float NOW_H = 80.f * SCALE.y;
        const float now_w = 760.f * SCALE.x;
        const ImVec2 NOW_POS(WINDOW_POS.x + (WINDOW_SIZE.x - now_w) / 2.f, cursor_y);
        const ImVec2 NOW_END(NOW_POS.x + now_w, cursor_y + NOW_H);

        draw_soft_section(draw_list, NOW_POS, NOW_END, section_border_color, SCALE);

        constexpr float icon_aspect_ratio = 1.35f;
        const float icon_height = 40 * SCALE.y;
        const ImVec2 ICON_SIZE(icon_height * icon_aspect_ratio, icon_height);
        const ImVec2 ICON_POS(NOW_POS.x + (22.f * SCALE.x), NOW_POS.y + (5.f * SCALE.y));
        if (gui.vita_icons.contains("playing_game")) {
            ImGui::SetCursorScreenPos(ICON_POS);
            ImGui::Image(gui.vita_icons["playing_game"], ICON_SIZE);
        }

        ImGui::SetWindowFontScale(1.52f * RES_SCALE.y);
        ImGui::SetCursorScreenPos(ImVec2(NOW_POS.x + (90.f * SCALE.x), NOW_POS.y + (12.f * SCALE.y)));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", now_playing.c_str());

        cursor_y = NOW_END.y + 12.f * SCALE.y;
    }

    // Relation / action section (not shown for SELF or FRIEND)
    if (show_invite_section) {
        float section_h = 0.f;

        // Calculate section height based on content
        if (is_request_sent || is_blocked)
            section_h = 80.f * SCALE.y;
        else
            section_h = 130.f * SCALE.y;

        const float section_w = 760.f * SCALE.x;
        const ImVec2 SEC_POS(WINDOW_POS.x + (WINDOW_SIZE.x - section_w) / 2.f, cursor_y);
        const ImVec2 SEC_END(SEC_POS.x + section_w, cursor_y + section_h);

        draw_soft_section(draw_list, SEC_POS, SEC_END, section_border_color, SCALE);

        const float sec_w = SEC_END.x - SEC_POS.x;
        const bool has_actions = (profile_info.relation == FriendRelation::NONE || profile_info.relation == FriendRelation::REQUEST_RECEIVED);
        const float BTN_W = 354.f * SCALE.x;
        const float BTN_H = 46.f * SCALE.y;
        const float BTN_Y = SEC_END.y - 16.f * SCALE.y - BTN_H;

        // Relation text
        ImGui::SetWindowFontScale(1.0f * RES_SCALE.y);
        std::string rel_text;
        switch (profile_info.relation) {
        case FriendRelation::NONE: rel_text = lang["not_a_friend_yet"]; break;
        case FriendRelation::REQUEST_SENT: rel_text = lang["waiting_friend_response"]; break;
        case FriendRelation::REQUEST_RECEIVED: rel_text = lang["friend_request_from_player"]; break;
        case FriendRelation::BLOCKED: rel_text = lang["blocked"]; break;
        default: break;
        }
        const auto rel_sz = ImGui::CalcTextSize(rel_text.c_str());
        float rel_y = SEC_POS.y + 8.f * SCALE.y;
        if (has_actions) {
            const float text_area_height = BTN_Y - SEC_POS.y;
            rel_y = SEC_POS.y + (text_area_height - rel_sz.y) / 2.f;
        } else if (is_request_sent || is_blocked) {
            rel_y = SEC_POS.y + (section_h - rel_sz.y) / 2.f;
        }
        ImGui::SetCursorScreenPos(ImVec2(SEC_POS.x + (sec_w - rel_sz.x) / 2.f, rel_y));
        const auto rel_color = ImVec4(1.f, 1.f, 1.f, 1.f);
        ImGui::TextColored(rel_color, "%s", rel_text.c_str());

        if (has_actions) {
            // Action buttons
            const bool disabled = profile.is_action_pending.load() || !v3kn::is_v3kn_logged_in();

            ImGui::SetWindowFontScale(0.95f * RES_SCALE.y);

            switch (profile_info.relation) {
            case FriendRelation::NONE: {
                ImGui::SetCursorScreenPos(ImVec2(SEC_POS.x + (sec_w - BTN_W) / 2.f, BTN_Y));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f * SCALE.x);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.7f, 0.7f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.78f, 0.78f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.62f, 0.62f, 0.62f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
                ImGui::SetWindowFontScale(0.85f * RES_SCALE.y);
                ImGui::BeginDisabled(disabled);
                if (ImGui::Button(lang["send_friend_request"].c_str(), ImVec2(BTN_W, BTN_H))) {
                    profile.is_action_pending.store(true);
                    profile.action_result_message.clear();
                    gui.vita_area.please_wait = true;
                    const std::string online_id = profile_info.online_id;
                    std::thread([&gui, &emuenv, online_id]() {
                        auto &p = emuenv.v3kn.profile_state;
                        const auto res = v3kn::v3kn_friend_add(emuenv.v3kn.account_state.user_info, online_id);
                        v3kn::handle_v3kn_status(emuenv, res);
                        std::string error_message;
                        if (res.body != "OK:FriendAdded" && res.body != "OK:RequestSent")
                            error_message = get_v3kn_error_message(gui.lang, res);
                        gui.vita_area.please_wait = false;
                        refresh_friend_profile(gui, emuenv, online_id);
                        if (!error_message.empty())
                            p.action_result_message = error_message;
                        p.is_action_pending.store(false);
                    }).detach();
                }
                ImGui::EndDisabled();
                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar();
                break;
            }
            case FriendRelation::REQUEST_RECEIVED: {
                const float btn_w = std::min(BTN_W * 2.f + 12.f * SCALE.x, sec_w - 20.f * SCALE.x);
                const float HALF = (btn_w - 12.f * SCALE.x) / 2.f;
                ImGui::SetCursorScreenPos(ImVec2(SEC_POS.x + (sec_w - btn_w) / 2.f, BTN_Y));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f * SCALE.x);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.7f, 0.7f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.78f, 0.78f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.62f, 0.62f, 0.62f, 1.f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
                ImGui::SetWindowFontScale(0.85f * RES_SCALE.y);
                ImGui::BeginDisabled(disabled);
                if (ImGui::Button(lang["cancel_friend_request"].c_str(), ImVec2(HALF, BTN_H))) {
                    profile.is_action_pending.store(true);
                    const std::string online_id = profile_info.online_id;
                    std::thread([&gui, &emuenv, online_id]() {
                        gui.vita_area.please_wait = true;
                        auto &p = emuenv.v3kn.profile_state;
                        const auto res = v3kn::v3kn_friend_reject(emuenv.v3kn.account_state.user_info, online_id);
                        v3kn::handle_v3kn_status(emuenv, res);
                        std::string error_message;
                        if (!res.body.starts_with("OK:") && res.body != "ERR:NoRequestFound")
                            error_message = get_v3kn_error_message(gui.lang, res);
                        gui.vita_area.please_wait = false;
                        refresh_friend_profile(gui, emuenv, online_id);
                        if (!error_message.empty())
                            p.action_result_message = error_message;
                        else
                            p.action_result_message = "Request canceled !";
                        p.is_action_pending.store(false);
                    }).detach();
                }
                ImGui::EndDisabled();
                ImGui::SameLine(0.f, 12.f * SCALE.x);
                ImGui::BeginDisabled(disabled);
                if (ImGui::Button(lang["accept"].c_str(), ImVec2(HALF, BTN_H))) {
                    profile.is_action_pending.store(true);
                    const std::string online_id = profile_info.online_id;
                    std::thread([&gui, &emuenv, online_id]() {
                        gui.vita_area.please_wait = true;
                        auto &p = emuenv.v3kn.profile_state;
                        const auto res = v3kn::v3kn_friend_accept(emuenv.v3kn.account_state.user_info, online_id);
                        v3kn::handle_v3kn_status(emuenv, res);
                        std::string error_message;
                        if (!res.body.starts_with("OK:") && res.body != "ERR:NoRequestFound")
                            error_message = get_v3kn_error_message(gui.lang, res);
                        gui.vita_area.please_wait = false;
                        refresh_friend_profile(gui, emuenv, online_id);
                        if (!error_message.empty())
                            p.action_result_message = error_message;
                        else if (res.body.starts_with("OK:"))
                            p.action_result_message = "Request accepted !";
                        else if (res.body == "ERR:NoRequestFound")
                            friend_notice_text = gui.lang.friends["friend_request_already_canceled"];
                        p.is_action_pending.store(false);
                    }).detach();
                }
                ImGui::EndDisabled();
                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar();
                break;
            }
            default: break;
            }
        }

        cursor_y = SEC_END.y + 12.f * SCALE.y;
    }

    if (profile_info.last_updated_activity > 0) {
        // "Activity" section (like Vita)
        const ImVec2 ACTIVITY_SIZE(760.f * SCALE.x, 80.f * SCALE.y);
        const ImVec2 ACT_POS(WINDOW_POS.x + (WINDOW_SIZE.x - ACTIVITY_SIZE.x) / 2.f, cursor_y);
        const ImVec2 ACT_END(ACT_POS.x + ACTIVITY_SIZE.x, cursor_y + ACTIVITY_SIZE.y);

        ImGui::SetWindowFontScale(1.1f * RES_SCALE.y);
        ImGui::SetCursorScreenPos(ImVec2(ACT_POS.x + 16.f * SCALE.x, ACT_POS.y + (ACTIVITY_SIZE.y - ImGui::GetFontSize()) / 2.f));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["activity"].c_str());

        const auto last_activity_time = get_last_updated_activity_time(gui, emuenv, profile_info.last_updated_activity);
        const auto last_activity_size = ImGui::CalcTextSize(last_activity_time.c_str());
        const auto last_activity_pos = ImVec2(ACT_END.x - 16.f * SCALE.x - last_activity_size.x, ACT_POS.y + (ACTIVITY_SIZE.y - last_activity_size.y) / 2.f);
        ImGui::SetCursorScreenPos(last_activity_pos);
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", last_activity_time.c_str());

        draw_glass_section(ACT_POS, ACT_END, ACTIVITY_SIZE, section_border_color);

        ImGui::SetCursorScreenPos(ACT_POS);
        if (ImGui::InvisibleButton("activity_time_btn", ACTIVITY_SIZE)) {
            reset_friend_activity_details();
            v3kn::get_user_activities(emuenv, profile_info.online_id);
            gui.vita_area.friend_activity = true;
        }

        cursor_y = ACT_END.y + 12.f * SCALE.y;
    }

    // Trophy section
    const ImVec2 TROPHY_SECTION_SIZE(760.f * SCALE.x, 80.f * SCALE.y);
    const ImVec2 TR_POS(WINDOW_POS.x + (WINDOW_SIZE.x - TROPHY_SECTION_SIZE.x) / 2.f, cursor_y);
    const ImVec2 TR_END(TR_POS.x + TROPHY_SECTION_SIZE.x, cursor_y + TROPHY_SECTION_SIZE.y);

    const float TR_LEVEL_ICON = 54.f * SCALE.x;
    const ImVec2 tr_center(TR_POS.x + (20.f * SCALE.x) + (TR_LEVEL_ICON / 2.f), TR_POS.y + TROPHY_SECTION_SIZE.y / 2.f);
    const ImVec2 tr_min(tr_center.x - (TR_LEVEL_ICON / 2.f), tr_center.y - (TR_LEVEL_ICON / 2.f));
    const ImVec2 tr_max(tr_center.x + TR_LEVEL_ICON / 2.f, tr_center.y + TR_LEVEL_ICON / 2.f);
    if (gui.vita_icons.contains("trophy_level")) {
        ImGui::SetCursorScreenPos(tr_min);
        ImGui::Image(gui.vita_icons["trophy_level"], ImVec2(TR_LEVEL_ICON, TR_LEVEL_ICON));
    } else {
        draw_list->AddCircleFilled(tr_center, TR_LEVEL_ICON / 2.f, IM_COL32(255, 215, 0, 255));
    }
    const float trophy_text_y = TR_POS.y + 8.f * SCALE.y;
    const ImVec2 PROGRESS_BAR_SIZE(210.f * SCALE.x, 12.f * SCALE.y);
    const ImVec2 PROGRESS_BAR_POS(TR_POS.x + 88.f * SCALE.x, TR_END.y - 12.f * SCALE.y - PROGRESS_BAR_SIZE.y);

    const float CNT_Y = TR_POS.y + 32.f * SCALE.y;
    const ImVec2 COUNTS_BOX_SIZE(432.f * SCALE.x, 62.f * SCALE.y);
    const ImVec2 COUNTS_BOX_POS(TR_END.x - (18.f * SCALE.x) - COUNTS_BOX_SIZE.x, TR_POS.y + (TROPHY_SECTION_SIZE.y - COUNTS_BOX_SIZE.y) / 2.f);
    const ImVec2 COUNTS_BOX_END(COUNTS_BOX_POS.x + COUNTS_BOX_SIZE.x, COUNTS_BOX_POS.y + COUNTS_BOX_SIZE.y);

    draw_list->AddRect(COUNTS_BOX_POS, COUNTS_BOX_END, IM_COL32(188, 188, 188, 255), 15.f * SCALE.x, 0, 1.f * SCALE.x);

    ImGui::SetWindowFontScale(0.9f * RES_SCALE.y);
    const auto level_label_size = ImGui::CalcTextSize(lang["level"].c_str());
    ImGui::SetCursorScreenPos(ImVec2(PROGRESS_BAR_POS.x, PROGRESS_BAR_POS.y - (12.f * SCALE.y) - level_label_size.y));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["level"].c_str());
    ImGui::SetWindowFontScale(1.46f * RES_SCALE.y);
    ImGui::SetCursorScreenPos(ImVec2(PROGRESS_BAR_POS.x + level_label_size.x + (6.f * SCALE.x), PROGRESS_BAR_POS.y - (12.f * SCALE.y) - ImGui::CalcTextSize(std::to_string(profile_info.trophy_info.level).c_str()).y));
    ImGui::TextColored(GUI_COLOR_TEXT, "%u", profile_info.trophy_info.level);
    ImGui::SetCursorScreenPos(PROGRESS_BAR_POS);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f * SCALE.x);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, IM_COL32(214, 182, 90, 255));
    ImGui::ProgressBar(profile_info.trophy_info.progress / 100.f, PROGRESS_BAR_SIZE, "");
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    const auto progress_text = fmt::format("{}%", profile_info.trophy_info.progress);
    ImGui::SetWindowFontScale(1.02f * RES_SCALE.y);
    const auto progress_size = ImGui::CalcTextSize(progress_text.c_str());
    ImGui::SetCursorScreenPos(ImVec2(PROGRESS_BAR_POS.x + PROGRESS_BAR_SIZE.x - progress_size.x, PROGRESS_BAR_POS.y - (12.f * SCALE.y) - progress_size.y));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", progress_text.c_str());

    ImGui::SetWindowFontScale(0.9f * RES_SCALE.y);
    ImGui::SetCursorScreenPos(ImVec2(COUNTS_BOX_POS.x + (10.f * SCALE.x), COUNTS_BOX_POS.y + (8.f * SCALE.y)));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["trophies"].c_str());
    ImGui::SetWindowFontScale(1.f * RES_SCALE.y);
    ImGui::SetCursorScreenPos(ImVec2(COUNTS_BOX_POS.x + (12.f * SCALE.x), COUNTS_BOX_POS.y + (35.f * SCALE.y)));
    ImGui::TextColored(GUI_COLOR_TEXT, "%u", profile_info.trophy_info.total_unlocked);
    const float tr_start = COUNTS_BOX_POS.x + (114.f * SCALE.x);
    const float tr_sp = 78.f * SCALE.x;
    const float count_y = COUNTS_BOX_POS.y + 38.f * SCALE.y;
    const ImU32 tcols[] = { IM_COL32(180, 200, 220, 255), IM_COL32(255, 215, 0, 255), IM_COL32(192, 192, 192, 255), IM_COL32(205, 127, 50, 255) };
    static const std::array<std::string, 4> trophy_icon_keys = { "platinum", "gold", "silver", "bronze" };
    const uint32_t tvals[] = { profile_info.trophy_info.platinum, profile_info.trophy_info.gold, profile_info.trophy_info.silver, profile_info.trophy_info.bronze };
    const float TROPHY_SIZE = 36.f * SCALE.x;
    for (int t = 0; t < 4; t++) {
        const float x = tr_start + (t * tr_sp);
        const float dot_x = x + TROPHY_SIZE;
        const float dot_y = count_y - 18.f * SCALE.y;
        const auto filename = "trophy_" + trophy_icon_keys[t];
        if (gui.vita_icons.contains(filename)) {
            const ImVec2 icon_min(dot_x - (TROPHY_SIZE / 2.f), dot_y - (TROPHY_SIZE / 2.f));
            const ImVec2 icon_max(dot_x + (TROPHY_SIZE / 2.f), dot_y + (TROPHY_SIZE / 2.f));
            ImGui::SetCursorScreenPos(icon_min);
            ImGui::Image(gui.vita_icons[filename], ImVec2(TROPHY_SIZE, TROPHY_SIZE));
        } else {
            draw_list->AddCircleFilled(ImVec2(dot_x, dot_y), TROPHY_SIZE / 2.f, tcols[t]);
        }
        const auto count_text = std::to_string(tvals[t]);
        const auto count_size = ImGui::CalcTextSize(count_text.c_str());
        ImGui::SetCursorScreenPos(ImVec2(dot_x - (count_size.x / 2.f), count_y));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", count_text.c_str());
    }

    draw_glass_section(TR_POS, TR_END, TROPHY_SECTION_SIZE, section_border_color);

    draw_list->AddRect(COUNTS_BOX_POS, COUNTS_BOX_END, IM_COL32(255, 255, 255, 75), 15.f * SCALE.x, 0, 1.f * SCALE.x);

    // About Me section (like Vita)
    const ImVec2 ABOUT_ME_SECTION_SIZE(760.f * SCALE.x, 80.f * SCALE.y);
    const ImVec2 ABOUT_ME_SECTION_POS(WINDOW_POS.x + (WINDOW_SIZE.x - ABOUT_ME_SECTION_SIZE.x) / 2.f, TR_END.y + 12.f * SCALE.y);
    const ImVec2 ABOUT_ME_SECTION_END(ABOUT_ME_SECTION_POS.x + ABOUT_ME_SECTION_SIZE.x, ABOUT_ME_SECTION_POS.y + ABOUT_ME_SECTION_SIZE.y);
    draw_soft_section(draw_list, ABOUT_ME_SECTION_POS, ABOUT_ME_SECTION_END, section_border_color, SCALE);
    ImGui::SetWindowFontScale(0.96f * RES_SCALE.y);
    ImGui::SetCursorScreenPos(ImVec2(ABOUT_ME_SECTION_POS.x + 16.f * SCALE.x, ABOUT_ME_SECTION_POS.y + (10.f * SCALE.y)));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", lang["about_me"].c_str());
    ImGui::SetWindowFontScale(1.32f * RES_SCALE.y);
    ImGui::SetCursorScreenPos(ImVec2(ABOUT_ME_SECTION_POS.x + 16.f * SCALE.x, ABOUT_ME_SECTION_POS.y + (38.f * SCALE.y)));
    ImGui::PushTextWrapPos(ABOUT_ME_SECTION_END.x - (16.f * SCALE.x));
    ImGui::TextColored(GUI_COLOR_TEXT, "%s", profile_info.about_me.c_str());
    ImGui::PopTextWrapPos();

    cursor_y = ABOUT_ME_SECTION_END.y + 12.f * SCALE.y;

    // "..." options button (bottom-right, like Vita)
    if (profile_loaded) {
        const float DOT_BTN = 46.f * SCALE.x;
        const ImVec2 dot_pos(WINDOW_POS.x + WINDOW_SIZE.x - DOT_BTN - 16.f * SCALE.x, WINDOW_POS.y + WINDOW_SIZE.y - DOT_BTN - 16.f * SCALE.y);
        ImGui::SetCursorScreenPos(dot_pos);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.25f, 0.25f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.35f, 0.35f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.15f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
        ImGui::SetWindowFontScale(1.8f * RES_SCALE.y);
        if (ImGui::Button("...", ImVec2(DOT_BTN, DOT_BTN)))
            show_profile_options_popup = true;
        ImGui::PopStyleColor(4);
    }

    // Options popup menu (dark, like Vita)
    if (profile_loaded && show_profile_options_popup) {
        const float POPUP_W = (is_request_received ? 404.f : 320.f) * SCALE.x;
        float popup_item_count = 1.f;
        if (is_friend || is_request_sent)
            popup_item_count = 2.f;
        const float POPUP_ITEM_H = (is_request_received ? 60.f : 44.f) * SCALE.y;
        const float POPUP_H = POPUP_ITEM_H * popup_item_count + 10.f * SCALE.y;
        const ImVec2 POPUP_POS(WINDOW_POS.x + WINDOW_SIZE.x - POPUP_W - 20.f * SCALE.x,
            WINDOW_POS.y + WINDOW_SIZE.y - 70.f * SCALE.y - POPUP_H);
        const ImVec2 POPUP_END(POPUP_POS.x + POPUP_W, POPUP_POS.y + POPUP_H);

        draw_list->AddRectFilled(POPUP_POS, POPUP_END, IM_COL32(50, 50, 50, 240), 6.f * SCALE.x);
        draw_list->AddRect(POPUP_POS, POPUP_END, IM_COL32(80, 80, 80, 255), 6.f * SCALE.x);

        ImGui::SetWindowFontScale(1.0f * RES_SCALE.y);
        float item_y = POPUP_POS.y + 5.f * SCALE.y;

        if (is_friend) {
            ImGui::SetCursorScreenPos(ImVec2(POPUP_POS.x + 4.f * SCALE.x, item_y));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 1.f, 1.f, 0.25f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
            if (ImGui::Button(lang["remove_from_friends"].c_str(), ImVec2(POPUP_W - 8.f * SCALE.x, POPUP_ITEM_H))) {
                show_profile_options_popup = false;
                show_profile_confirm_dialog = true;
                confirm_dialog_action = "remove";
                confirm_dialog_text = lang["player_removed_from_friends"];
            }
            ImGui::PopStyleColor(4);

            draw_list->AddLine(ImVec2(POPUP_POS.x + 10.f * SCALE.x, item_y + POPUP_ITEM_H),
                ImVec2(POPUP_END.x - 10.f * SCALE.x, item_y + POPUP_ITEM_H), IM_COL32(100, 100, 100, 180));
            item_y += POPUP_ITEM_H;

            ImGui::SetCursorScreenPos(ImVec2(POPUP_POS.x + 4.f * SCALE.x, item_y));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 1.f, 1.f, 0.25f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
            if (ImGui::Button(lang["block"].c_str(), ImVec2(POPUP_W - 8.f * SCALE.x, POPUP_ITEM_H))) {
                show_profile_options_popup = false;
                show_profile_confirm_dialog = true;
                confirm_dialog_action = "block";
                confirm_dialog_text = lang["warn_block_friend"];
            }
            ImGui::PopStyleColor(4);
        } else if (is_request_sent) {
            ImGui::SetCursorScreenPos(ImVec2(POPUP_POS.x + 4.f * SCALE.x, item_y));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 1.f, 1.f, 0.25f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
            if (ImGui::Button(lang["cancel_friend_request"].c_str(), ImVec2(POPUP_W - 8.f * SCALE.x, POPUP_ITEM_H))) {
                show_profile_options_popup = false;
                show_profile_confirm_dialog = true;
                confirm_dialog_action = "cancel";
                confirm_dialog_text = lang["friend_request_canceled"];
            }
            ImGui::PopStyleColor(4);

            draw_list->AddLine(ImVec2(POPUP_POS.x + 10.f * SCALE.x, item_y + POPUP_ITEM_H),
                ImVec2(POPUP_END.x - 10.f * SCALE.x, item_y + POPUP_ITEM_H), IM_COL32(100, 100, 100, 180));
            item_y += POPUP_ITEM_H;

            ImGui::SetCursorScreenPos(ImVec2(POPUP_POS.x + 4.f * SCALE.x, item_y));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 1.f, 1.f, 0.25f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
            if (ImGui::Button(lang["block"].c_str(), ImVec2(POPUP_W - 8.f * SCALE.x, POPUP_ITEM_H))) {
                show_profile_options_popup = false;
                show_profile_confirm_dialog = true;
                confirm_dialog_action = "block";
                confirm_dialog_text = lang["warn_block_player"];
            }
            ImGui::PopStyleColor(4);
        } else if (is_request_received) {
            ImGui::SetCursorScreenPos(ImVec2(POPUP_POS.x + 4.f * SCALE.x, item_y));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 1.f, 1.f, 0.25f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
            if (ImGui::Button(lang["block"].c_str(), ImVec2(POPUP_W - 8.f * SCALE.x, POPUP_ITEM_H))) {
                show_profile_options_popup = false;
                show_profile_confirm_dialog = true;
                confirm_dialog_action = "block";
                confirm_dialog_text = lang["warn_block_player"];
            }
            ImGui::PopStyleColor(4);
        } else if (is_none) {
            ImGui::SetCursorScreenPos(ImVec2(POPUP_POS.x + 4.f * SCALE.x, item_y));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 1.f, 1.f, 0.25f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
            if (ImGui::Button(lang["block"].c_str(), ImVec2(POPUP_W - 8.f * SCALE.x, POPUP_ITEM_H))) {
                show_profile_options_popup = false;
                show_profile_confirm_dialog = true;
                confirm_dialog_action = "block";
                confirm_dialog_text = lang["warn_block_player"];
            }
            ImGui::PopStyleColor(4);
        } else if (is_blocked) {
            ImGui::SetCursorScreenPos(ImVec2(POPUP_POS.x + 4.f * SCALE.x, item_y));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.f, 1.f, 1.f, 0.25f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
            if (ImGui::Button(lang["unblock"].c_str(), ImVec2(POPUP_W - 8.f * SCALE.x, POPUP_ITEM_H))) {
                show_profile_options_popup = false;
                show_profile_confirm_dialog = true;
                confirm_dialog_action = "unblock";
                confirm_dialog_text = lang["unblock_player"];
            }
            ImGui::PopStyleColor(4);
        }

        // Close popup on click outside
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseHoveringRect(POPUP_POS, POPUP_END))
            show_profile_options_popup = false;
    }

    // Confirmation dialog (dark overlay + centered panel, like Vita)
    if (profile_loaded && show_profile_confirm_dialog) {
        const ImVec2 OVL_POS = WINDOW_POS;
        const ImVec2 OVL_END(WINDOW_POS.x + WINDOW_SIZE.x, WINDOW_POS.y + WINDOW_SIZE.y);
        draw_list->AddRectFilled(OVL_POS, OVL_END, IM_COL32(0, 0, 0, 160));

        const float DLG_W = 760.f * SCALE.x;
        const float DLG_H = 440.f * SCALE.y;
        const ImVec2 DLG_POS(WINDOW_POS.x + (WINDOW_SIZE.x - DLG_W) / 2.f, WINDOW_POS.y + (WINDOW_SIZE.y - DLG_H) / 2.f);
        const ImVec2 DLG_END(DLG_POS.x + DLG_W, DLG_POS.y + DLG_H);

        draw_list->AddRectFilled(DLG_POS, DLG_END, IM_COL32(50, 50, 50, 245), 8.f * SCALE.x);
        draw_list->AddRect(DLG_POS, DLG_END, IM_COL32(80, 80, 80, 255), 8.f * SCALE.x);

        // Avatar + online_id at top
        const float DLG_AVATAR = 64.f * SCALE.x;
        const ImVec2 dlg_av_pos(DLG_POS.x + 50.f * SCALE.x, DLG_POS.y + 20.f * SCALE.y);
        const ImVec2 dlg_av_end(dlg_av_pos.x + DLG_AVATAR, dlg_av_pos.y + DLG_AVATAR);
        if (gui.friend_profile_avatar && gui.friend_profile_avatar_online_id == profile_info.online_id) {
            ImGui::SetCursorScreenPos(dlg_av_pos);
            ImGui::Image(gui.friend_profile_avatar, ImVec2(DLG_AVATAR, DLG_AVATAR));
        } else {
            draw_list->AddRectFilled(dlg_av_pos, dlg_av_end, IM_COL32(150, 150, 150, 255));
        }

        ImGui::SetWindowFontScale(1.3f * RES_SCALE.y);
        ImGui::SetCursorScreenPos(ImVec2(DLG_POS.x + 132.f * SCALE.x, DLG_POS.y + 32.f * SCALE.y));
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", profile_info.online_id.c_str());

        // Separator
        const float sep_y = dlg_av_end.y + 14.f * SCALE.y;
        draw_list->AddLine(ImVec2(DLG_POS.x + 10.f * SCALE.x, sep_y),
            ImVec2(DLG_END.x - 10.f * SCALE.x, sep_y), IM_COL32(100, 100, 100, 180));

        // Confirmation text
        ImGui::SetWindowFontScale(1.05f * RES_SCALE.y);
        const float text_area_top = sep_y + 10.f * SCALE.y;
        const float btn_area_top = DLG_END.y - 56.f * SCALE.y - 20.f * SCALE.y;
        const float text_pad_x = 40.f * SCALE.x;
        const float wrap_width = DLG_W - (text_pad_x * 2.f);
        const auto text_size = ImGui::CalcTextSize(confirm_dialog_text.c_str(), nullptr, false, wrap_width);
        const float text_x = DLG_POS.x + (DLG_W - text_size.x) * 0.5f;
        const float text_y = text_area_top + (btn_area_top - text_area_top - text_size.y) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(text_x, text_y));
        ImGui::PushTextWrapPos(text_x + text_size.x);
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.f), "%s", confirm_dialog_text.c_str());
        ImGui::PopTextWrapPos();

        // Annuler / OK buttons
        const float BTN_W = 310.f * SCALE.x;
        const float BTN_H = 46.f * SCALE.y;
        const float BTN_Y = DLG_END.y - 22.f * SCALE.y - BTN_H;
        const float btn_total_w = BTN_W * 2.f + 40.f * SCALE.x;
        const float btn_start_x = DLG_POS.x + (DLG_W - btn_total_w) / 2.f;

        const bool block_dialog = (confirm_dialog_action == "block" || confirm_dialog_action == "unblock");
        ImGui::SetCursorScreenPos(ImVec2(btn_start_x, BTN_Y));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.25f, 0.25f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
        ImGui::SetWindowFontScale(1.2f * RES_SCALE.y);
        if (ImGui::Button((block_dialog ? ::lang::get(::lang::str::no) : ::lang::get(::lang::str::cancel)).c_str(), ImVec2(BTN_W, BTN_H))) {
            show_profile_confirm_dialog = false;
            confirm_dialog_action.clear();
        }
        ImGui::PopStyleColor(4);

        ImGui::SameLine(0.f, 40.f * SCALE.x);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.25f, 0.25f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
        ImGui::BeginDisabled(profile.is_action_pending.load());
        if (ImGui::Button((block_dialog ? ::lang::get(::lang::str::yes) : ::lang::get(::lang::str::ok)).c_str(), ImVec2(BTN_W, BTN_H))) {
            profile.is_action_pending.store(true);
            gui.vita_area.please_wait = true;
            const std::string online_id = profile_info.online_id;
            const std::string action = confirm_dialog_action;
            std::thread([&gui, &emuenv, online_id, action]() {
                auto &p = emuenv.v3kn.profile_state;
                net_utils::WebResponse res;
                if (action == "remove")
                    res = v3kn::v3kn_friend_remove(emuenv.v3kn.account_state.user_info, online_id);
                else if (action == "cancel")
                    res = v3kn::v3kn_friend_cancel(emuenv.v3kn.account_state.user_info, online_id);
                else if (action == "reject")
                    res = v3kn::v3kn_friend_reject(emuenv.v3kn.account_state.user_info, online_id);
                else if (action == "block")
                    res = v3kn::v3kn_friend_block(emuenv.v3kn.account_state.user_info, online_id);
                else if (action == "unblock")
                    res = v3kn::v3kn_friend_unblock(emuenv.v3kn.account_state.user_info, online_id);
                v3kn::handle_v3kn_status(emuenv, res);
                std::string error_message;
                if (!res.body.starts_with("OK:"))
                    error_message = get_v3kn_error_message(gui.lang, res);
                gui.vita_area.please_wait = false;
                refresh_friend_profile(gui, emuenv, online_id);
                if (!error_message.empty())
                    p.action_result_message = error_message;
                p.is_action_pending.store(false);
            }).detach();
            show_profile_confirm_dialog = false;
            confirm_dialog_action.clear();
        }
        ImGui::EndDisabled();
        ImGui::PopStyleColor(4);
    }

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
        ImGui::SetWindowFontScale(0.8f);
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
    ImGui::PopStyleVar(2);
}

} // namespace gui