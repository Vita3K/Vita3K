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

#include <config/state.h>
#include <emuenv/state.h>
#include <io/state.h>

#include <algorithm>
#include <map>
#include <thread>

#include <util/fs.h>
#include <util/log.h>
#include <util/net_utils.h>
#include <util/system.h>

#include <nlohmann/json.hpp>
#include <pugixml.hpp>

#include <v3kn/account.h>
#include <v3kn/activity.h>
#include <v3kn/friend.h>
#include <v3kn/state.h>

void from_json(const nlohmann::json &j, CommentActivity &c) {
    c.online_id = j.value("online_id", "");
    c.comment = j.value("comment", "");
    c.created_at = j.value("created_at", static_cast<time_t>(0));
}

namespace v3kn {

static std::vector<std::string> collect_recent_activity_likes(const nlohmann::json &activity, uint32_t &like_count) {
    constexpr size_t max_displayed_likes = 20;

    like_count = 0;
    if (!activity.contains("likes") || !activity["likes"].is_array())
        return {};

    const auto &likes = activity["likes"];
    like_count = static_cast<uint32_t>(likes.size());

    std::vector<std::string> recent_likes;
    const size_t first_index = likes.size() > max_displayed_likes ? (likes.size() - max_displayed_likes) : 0;
    recent_likes.reserve(likes.size() - first_index);

    for (size_t i = first_index; i < likes.size(); ++i) {
        if (!likes[i].is_string())
            continue;

        const auto online_id = likes[i].get<std::string>();
        if (!online_id.empty())
            recent_likes.push_back(online_id);
    }

    return recent_likes;
}

static std::string get_activity_target_online_id(const ActivityPostStatus &status, const std::string &fallback_online_id = {}) {
    return !status.online_id.empty() ? status.online_id : fallback_online_id;
}

time_t get_current_activity_time_ms(EmuEnvState &emuenv) {
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto &last_time = emuenv.v3kn.activity_state.last_created_at_ms;
    auto previous = last_time.load();
    uint64_t next;
    do {
        next = std::max(static_cast<uint64_t>(now_ms), previous + 1);
    } while (!last_time.compare_exchange_weak(previous, next));
    return static_cast<time_t>(next);
}

static bool v3kn_activity_post_status(EmuEnvState &emuenv, const ActivityPostStatus &status) {
    // Create the JSON payload
    nlohmann::json payload;
    payload["type"] = status.type;
    payload["online_id"] = status.online_id;
    payload["title_id"] = status.title_id;
    payload["message"] = status.message;
    if (status.type == "game") {
        payload["content_id"] = status.game_activity.content_id;
        payload["action_uri"] = status.game_activity.action_uri;
    } else if (status.type.starts_with("trophy")) {
        payload["npcomm_id"] = status.trophy_activity.npcomm_id;
        payload["id"] = status.trophy_activity.id;
        if (status.type.find("hidden") == std::string::npos) {
            payload["grade"] = status.trophy_activity.grade;
        }
    }

    payload["created_at"] = status.created_at;

    auto &user_info = emuenv.v3kn.account_state.user_info;

    // URL of server endpoint
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/activity/post");

    // Send the POST request with the JSON payload and authentication token
    const auto res = net_utils::get_web_response_ex(url, user_info.token, payload.dump());

    v3kn::handle_v3kn_status(emuenv, res);

    if (!res.body.starts_with("OK:")) {
        const auto error_message = v3kn::get_v3kn_error_message(emuenv, res);
        LOG_ERROR("Server returned error {}", error_message);
        return false;
    }

    LOG_INFO("Activity posted successfully");
    return true;
}

static bool v3kn_activity_action(EmuEnvState &emuenv, const std::string &route, const ActivityPostStatus &status) {
    auto &user_info = emuenv.v3kn.account_state.user_info;
    if (user_info.online_id.empty())
        return false;

    const std::string post_data = fmt::format("target_online_id={}&created_at={}", get_activity_target_online_id(status, user_info.online_id), status.created_at);

    const std::string url = get_v3kn_server_url(user_info.host, route);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);

    v3kn::handle_v3kn_status(emuenv, res);

    if (!res.body.starts_with("OK:")) {
        const auto error_message = v3kn::get_v3kn_error_message(emuenv, res);
        LOG_ERROR("Server returned error {}", error_message);
        return false;
    }

    return true;
}

bool v3kn_activity_comment(EmuEnvState &emuenv, const ActivityPostStatus &status, const std::string &comment) {
    auto &user_info = emuenv.v3kn.account_state.user_info;
    if (user_info.online_id.empty() || comment.empty())
        return false;

    nlohmann::json payload;
    payload["target_online_id"] = get_activity_target_online_id(status, user_info.online_id);
    payload["online_id"] = user_info.online_id;
    payload["comment"] = comment;
    payload["created_at"] = status.created_at;

    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/activity/comment");
    const auto res = net_utils::get_web_response_ex(url, user_info.token, payload.dump());

    v3kn::handle_v3kn_status(emuenv, res);

    if (!res.body.starts_with("OK:")) {
        const auto error_message = v3kn::get_v3kn_error_message(emuenv, res);
        LOG_ERROR("Server returned error {}", error_message);
        return false;
    }

    return true;
}

bool delete_activity_comment(EmuEnvState &emuenv, const ActivityPostStatus &status, const CommentActivity &comment) {
    auto &user_info = emuenv.v3kn.account_state.user_info;
    if (user_info.online_id.empty() || (comment.created_at == 0))
        return false;

    const std::string post_data = fmt::format("target_online_id={}&created_at={}&comment_created_at={}",
        get_activity_target_online_id(status, user_info.online_id), status.created_at, comment.created_at);

    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/activity/uncomment");
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);

    v3kn::handle_v3kn_status(emuenv, res);

    if (!res.body.starts_with("OK:")) {
        const auto error_message = v3kn::get_v3kn_error_message(emuenv, res);
        LOG_ERROR("Server returned error {}, res: {}", error_message, res.body);
        return false;
    }

    return true;
}

void create_and_post_activity_status(EmuEnvState &emuenv, ActivityPostStatus &status) {
    std::lock_guard account_lock(emuenv.v3kn.account_state.user_info_mutex);
    std::lock_guard cache_lock(emuenv.v3kn.activity_state.cache_mutex);
    if (emuenv.v3kn.account_state.user_info.online_id.empty()) {
        return;
    }

    std::map<std::string, std::string> language;
    const bool requires_localized_message = status.type == "trophy_acquisition_platinum"
        || status.type == "trophy_acquisition_hidden"
        || status.type == "trophy_acquisition"
        || status.type == "game_start";
    if (requires_localized_message) {
        std::lock_guard lock(emuenv.v3kn.activity_state.language_mutex);
        language = emuenv.v3kn.activity_state.language;
        if (language.empty()) {
            LOG_ERROR("V3KN activity language has not been initialized");
            return;
        }
    }

    if (!emuenv.v3kn.activity_state.cache.empty())
        flush_activity_cache(emuenv);

    status.online_id = emuenv.v3kn.account_state.user_info.online_id;
    status.created_at = get_current_activity_time_ms(emuenv);
    if (status.type == "trophy_acquisition_platinum")
        status.message = fmt::format("{} {}", fmt::format(fmt::runtime(language.at("has_earned_platinum_trophy")), emuenv.current_app_title), status.trophy_activity.name);
    else if (status.type == "trophy_acquisition_hidden")
        status.message = fmt::format("{} ({})", fmt::format(fmt::runtime(language.at("has_earned_trophy")), emuenv.current_app_title), language.at("hidden_trophy"));
    else if (status.type == "trophy_acquisition") {
        status.message = fmt::format("{} {} ({})", fmt::format(fmt::runtime(language.at("has_earned_trophy")), emuenv.current_app_title),
            status.trophy_activity.name, language.at(status.trophy_activity.grade));
    } else if (status.type == "game_start")
        status.message = fmt::format(fmt::runtime(language.at("first_time_playing")), emuenv.current_app_title);

    // If local cache has less than 30 entries, add the new status to the cache, otherwise discard it (Vita limit is 30 entries)
    if (!v3kn_activity_post_status(emuenv, status) && emuenv.v3kn.activity_state.cache.size() < 30) {
        emuenv.v3kn.activity_state.cache.insert(emuenv.v3kn.activity_state.cache.begin(), std::move(status));
        save_activity_cache(emuenv);
    }
}

static net_utils::WebResponse v3kn_get_activities(EmuEnvState &emuenv, const std::string &online_id) {
    auto &user_info = emuenv.v3kn.account_state.user_info;
    const std::string url = get_v3kn_server_url(user_info.host, fmt::format("v3kn/activity/get?online_id={}&sys_lang={}", online_id, static_cast<uint32_t>(emuenv.cfg.sys_lang)));
    const auto res = net_utils::get_web_response_ex(url, user_info.token);
    v3kn::handle_v3kn_status(emuenv, res);
    return res;
}

static std::vector<std::string> collect_activity_avatar_online_ids(const std::vector<ActivityPostStatus> &activities) {
    std::vector<std::string> online_ids;
    for (const auto &activity : activities) {
        if (!activity.online_id.empty())
            online_ids.push_back(activity.online_id);
        if (!activity.friend_online_id.empty())
            online_ids.push_back(activity.friend_online_id);

        for (const auto &comment : activity.comments_activity) {
            if (!comment.online_id.empty())
                online_ids.push_back(comment.online_id);
        }
    }

    return online_ids;
}

void get_user_activities(EmuEnvState &emuenv, const std::string &online_id) {
    emuenv.v3kn.activity_state.posted_activities.clear();

    const auto res = v3kn_get_activities(emuenv, online_id);
    if (res.body.starts_with("ERR:")) {
        LOG_ERROR("Failed to get activities for user {}: {}", online_id, v3kn::get_v3kn_error_message(emuenv, res));
        return;
    }

    nlohmann::json activities_json;
    try {
        activities_json = nlohmann::json::parse(res.body);
    } catch (const std::exception &e) {
        LOG_ERROR("Failed to parse activities JSON for user {}: {}", online_id, e.what());
        return;
    }

    if (!activities_json.contains("activities") || !activities_json["activities"].is_array()) {
        LOG_ERROR("Invalid activities JSON format for user {}: missing 'activities' array", online_id);
        return;
    }

    for (const auto &activity : activities_json["activities"]) {
        const auto comments = activity.value("comments", std::vector<CommentActivity>{});
        uint32_t like_count = 0;
        auto likes_online_ids = collect_recent_activity_likes(activity, like_count);
        ActivityPostStatus status{
            .type = activity.value("type", ""),
            .online_id = activity.value("online_id", ""),
            .title_id = activity.value("title_id", ""),
            .message = activity.value("message", ""),
            .created_at = activity.value("created_at", static_cast<time_t>(0)),
            .title_name = activity.value("title_name", ""),
            .friend_online_id = activity.value("friend_online_id", ""),

            .game_activity = {
                .content_id = activity.value("content_id", ""),
                .action_uri = activity.value("action_uri", "") },
            .trophy_activity = { .npcomm_id = activity.value("npcomm_id", ""), .id = activity.value("id", ""), .grade = activity.value("grade", "") },
            .comment_count = static_cast<uint32_t>(comments.size()),
            .like_count = like_count,
            .comments_activity = std::move(comments),
            .likes_online_ids = std::move(likes_online_ids)
        };

        emuenv.v3kn.activity_state.posted_activities.push_back(status);
    }

    const auto avatar_online_ids = collect_activity_avatar_online_ids(emuenv.v3kn.activity_state.posted_activities);
    if (!avatar_online_ids.empty()) {
        std::thread([&emuenv, avatar_online_ids]() {
            load_user_avatars(emuenv, avatar_online_ids);
        }).detach();
    }
}

bool like_activity(EmuEnvState &emuenv, const ActivityPostStatus &status) {
    return v3kn_activity_action(emuenv, "v3kn/activity/like", status);
}

static fs::path get_activity_cache_path(const EmuEnvState &emuenv) {
    return emuenv.vita_fs_path / "ux0" / "user" / emuenv.io.user_id / "activity_cache.xml";
}

void load_activity_cache(EmuEnvState &emuenv) {
    std::lock_guard account_lock(emuenv.v3kn.account_state.user_info_mutex);
    std::lock_guard cache_lock(emuenv.v3kn.activity_state.cache_mutex);
    auto &cache = emuenv.v3kn.activity_state.cache;
    cache.clear();
    const fs::path cache_path = get_activity_cache_path(emuenv);
    if (!fs::exists(cache_path)) {
        return;
    }
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(cache_path.string().c_str());
    if (!result) {
        LOG_ERROR("Failed to load activity cache from {}: {}", cache_path.string(), result.description());
        return;
    }

    auto root = doc.child("pending");
    for (auto activity_node : root.children("activity")) {
        ActivityPostStatus entry;
        entry.type = activity_node.child("type").text().as_string();
        entry.online_id = activity_node.child("online_id").text().as_string();
        entry.title_id = activity_node.child("title_id").text().as_string();
        entry.message = activity_node.child("message").text().as_string();
        if (entry.type == "game") {
            entry.game_activity.content_id = activity_node.child("content_id").text().as_string();
            entry.game_activity.action_uri = activity_node.child("action_uri").text().as_string();
        } else if (entry.type.starts_with("trophy")) {
            entry.trophy_activity.npcomm_id = activity_node.child("npcomm_id").text().as_string();
            entry.trophy_activity.id = activity_node.child("trophy_id").text().as_string();
            if (entry.type.find("hidden") == std::string::npos)
                entry.trophy_activity.grade = activity_node.child("grade").text().as_string();
        }
        entry.created_at = static_cast<time_t>(activity_node.child("created_at").text().as_ullong());
        cache.push_back(entry);
    }
}

void save_activity_cache(EmuEnvState &emuenv) {
    std::lock_guard account_lock(emuenv.v3kn.account_state.user_info_mutex);
    std::lock_guard cache_lock(emuenv.v3kn.activity_state.cache_mutex);
    auto &cache = emuenv.v3kn.activity_state.cache;
    const fs::path cache_path = get_activity_cache_path(emuenv);
    pugi::xml_document doc;
    auto root = doc.append_child("pending");
    for (const auto &entry : cache) {
        auto activity_node = root.append_child("activity");
        // set all fields of entry as child nodes of activity_node
        activity_node.append_child("type").text().set(entry.type.c_str());
        activity_node.append_child("online_id").text().set(entry.online_id.c_str());
        activity_node.append_child("title_id").text().set(entry.title_id.c_str());
        activity_node.append_child("message").text().set(entry.message.c_str());
        if (entry.type == "game") {
            activity_node.append_child("content_id").text().set(entry.game_activity.content_id.c_str());
            activity_node.append_child("action_uri").text().set(entry.game_activity.action_uri.c_str());
        } else if (entry.type.starts_with("trophy")) {
            activity_node.append_child("npcomm_id").text().set(entry.trophy_activity.npcomm_id.c_str());
            activity_node.append_child("trophy_id").text().set(entry.trophy_activity.id.c_str());
            if (entry.type.find("hidden") == std::string::npos)
                activity_node.append_child("grade").text().set(entry.trophy_activity.grade.c_str());
        }
        activity_node.append_child("created_at").text().set(static_cast<unsigned long long>(entry.created_at));
    }

    if (!doc.save_file(cache_path.string().c_str()))
        LOG_ERROR("Failed to save activity cache to {}", cache_path.string());
}

void flush_activity_cache(EmuEnvState &emuenv) {
    std::lock_guard account_lock(emuenv.v3kn.account_state.user_info_mutex);
    std::lock_guard cache_lock(emuenv.v3kn.activity_state.cache_mutex);
    auto &cache = emuenv.v3kn.activity_state.cache;
    bool any_succes = false;
    for (auto it = cache.begin(); it != cache.end();) {
        if (v3kn_activity_post_status(emuenv, *it)) {
            it = cache.erase(it); // Remove from cache on success
            any_succes = true;
        } else {
            break; // Stop flushing on first failure to avoid potential rate limiting or other issues
        }
    }

    if (any_succes)
        save_activity_cache(emuenv);
}

bool unlike_activity(EmuEnvState &emuenv, const ActivityPostStatus &status) {
    return v3kn_activity_action(emuenv, "v3kn/activity/unlike", status);
}

} // namespace v3kn
