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

#include <v3kn/account.h>
#include <v3kn/friend.h>
#include <v3kn/state.h>

#include <config/state.h>
#include <emuenv/state.h>
#include <io/state.h>

#include <algorithm>
#include <optional>

#include <util/log.h>
#include <util/net_utils.h>

#include <nlohmann/json.hpp>

namespace v3kn {

static std::string get_sys_lang_query_value(const EmuEnvState &emuenv) {
    return fmt::format("{:02d}", emuenv.cfg.sys_lang);
}

static std::optional<ImageData> fetch_v3kn_avatar(EmuEnvState &emuenv, const std::string &target_online_id, net_utils::CurlSession *session = nullptr) {
    const auto &user_info = emuenv.v3kn.account_state.user_info;

    try {
        auto res = v3kn_avatar_download(user_info, target_online_id, session);
        if ((res.curl_res != 0) || res.body.empty() || res.body.starts_with("ERR:")) {
            LOG_ERROR_IF(res.body != "ERR:NoAvatar", "Failed to load avatar for {}: {}", target_online_id, get_v3kn_error_message(emuenv, res));
            return std::nullopt;
        }

        return ImageData{ std::move(res.body) };
    } catch (const std::exception &e) {
        LOG_ERROR("Failed to load avatar for {}: {}", target_online_id, e.what());
        return std::nullopt;
    }
}

static void store_pending_shared_avatar(FriendsState &state, const std::string &online_id, ImageData avatar_data) {
    std::lock_guard<std::mutex> lock(state.images_mutex);
    state.pending_friends_avatar[online_id] = std::move(avatar_data);
    state.loaded_avatar_ids.insert(online_id);
}

static void store_pending_profile_avatar(FriendsState &state, const std::string &online_id, ImageData avatar_data) {
    std::lock_guard<std::mutex> lock(state.images_mutex);
    state.pending_friend_profile_avatar = std::move(avatar_data);
    state.pending_friend_profile_avatar_online_id = online_id;
}

static void store_pending_self_avatar(FriendsState &state, ImageData avatar_data) {
    std::lock_guard<std::mutex> lock(state.images_mutex);
    state.pending_v3kn_avatar = std::move(avatar_data);
}

static std::vector<std::string> collect_missing_shared_avatar_online_ids(FriendsState &state, const std::vector<std::string> &online_ids, const std::string &self_online_id) {
    std::vector<std::string> unique_online_ids;

    std::lock_guard<std::mutex> lock(state.images_mutex);
    for (const auto &online_id : online_ids) {
        if (online_id.empty() || (online_id == self_online_id) || state.loaded_avatar_ids.contains(online_id)
            || (std::find(unique_online_ids.begin(), unique_online_ids.end(), online_id) != unique_online_ids.end()))
            continue;

        unique_online_ids.push_back(online_id);
    }

    return unique_online_ids;
}

net_utils::WebResponse v3kn_friend_add(const UserInfo &user_info, const std::string &target_online_id) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/friends/add");
    const std::string post_data = fmt::format("target_online_id={}", target_online_id);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);
    return res;
}

net_utils::WebResponse v3kn_friend_accept(const UserInfo &user_info, const std::string &target_online_id) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/friends/accept");
    const std::string post_data = fmt::format("target_online_id={}", target_online_id);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);
    return res;
}

net_utils::WebResponse v3kn_friend_reject(const UserInfo &user_info, const std::string &target_online_id) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/friends/reject");
    const std::string post_data = fmt::format("target_online_id={}", target_online_id);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);
    return res;
}

net_utils::WebResponse v3kn_friend_remove(const UserInfo &user_info, const std::string &target_online_id) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/friends/remove");
    const std::string post_data = fmt::format("target_online_id={}", target_online_id);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);
    return res;
}

net_utils::WebResponse v3kn_friend_cancel(const UserInfo &user_info, const std::string &target_online_id) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/friends/cancel");
    const std::string post_data = fmt::format("target_online_id={}", target_online_id);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);
    return res;
}

net_utils::WebResponse v3kn_friend_block(const UserInfo &user_info, const std::string &target_online_id) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/friends/block");
    const std::string post_data = fmt::format("target_online_id={}", target_online_id);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);
    return res;
}

net_utils::WebResponse v3kn_friend_unblock(const UserInfo &user_info, const std::string &target_online_id) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/friends/unblock");
    const std::string post_data = fmt::format("target_online_id={}", target_online_id);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);
    return res;
}

net_utils::WebResponse v3kn_friend_list(const UserInfo &user_info, const std::string &group, const std::string &sys_lang) {
    const std::string url = get_v3kn_server_url(user_info.host, fmt::format("v3kn/friends/list?group={}&sys_lang={}", group, sys_lang));
    const auto res = net_utils::get_web_response_ex(url, user_info.token);
    return res;
}

net_utils::WebResponse v3kn_friend_search(const UserInfo &user_info, const std::string &query) {
    const std::string url = get_v3kn_server_url(user_info.host, fmt::format("v3kn/friends/search?query={}", query));
    const auto res = net_utils::get_web_response_ex(url, user_info.token);
    return res;
}

net_utils::WebResponse v3kn_friend_presence(const UserInfo &user_info, const std::string &status, const std::string &now_playing, net_utils::CurlSession *session, const std::atomic<bool> *cancel_flag) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/friends/presence");
    std::string post_data = fmt::format("status={}", status);
    if (!now_playing.empty())
        post_data += fmt::format("&now_playing={}", now_playing);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data, session, cancel_flag);
    return res;
}

net_utils::WebResponse v3kn_friend_poll(const UserInfo &user_info, const time_t since, net_utils::CurlSession *session, const std::atomic<bool> *cancel_flag) {
    const std::string url = get_v3kn_server_url(user_info.host, fmt::format("v3kn/friends/poll?since={}", since));
    const auto res = net_utils::get_web_response_ex(url, user_info.token, "", session, cancel_flag);
    return res;
}

net_utils::WebResponse v3kn_friend_profile(const UserInfo &user_info, const std::string &target_online_id, const std::string &sys_lang) {
    const std::string url = get_v3kn_server_url(user_info.host, fmt::format("v3kn/friends/profile?target_online_id={}&sys_lang={}", target_online_id, sys_lang));
    const auto res = net_utils::get_web_response_ex(url, user_info.token);
    return res;
}

void load_friend_profile_avatar(EmuEnvState &emuenv, const std::string &online_id) {
    if (auto avatar_data = fetch_v3kn_avatar(emuenv, online_id))
        store_pending_profile_avatar(emuenv.v3kn.friends_state, online_id, std::move(*avatar_data));
}

void load_friend_profile_panel(EmuEnvState &emuenv, const std::string &target_online_id) {
    const auto &user_info = emuenv.v3kn.account_state.user_info;

    try {
        auto res = v3kn_panel_download(user_info, target_online_id);
        if (res.curl_res != 0 || res.body.empty() || res.body.starts_with("ERR:"))
            return;

        auto &state = emuenv.v3kn.friends_state;
        std::lock_guard<std::mutex> lock(state.images_mutex);
        state.pending_friend_profile_panel = ImageData{ std::move(res.body) };
        state.pending_friend_profile_panel_online_id = target_online_id;
    } catch (const std::exception &e) {
        LOG_ERROR("Failed to load profile panel for {}: {}", target_online_id, e.what());
    }
}

bool get_friend_profile_info(EmuEnvState &emuenv, const std::string &target_online_id, FriendProfileInfo &info) {
    info = {};
    info.online_id = target_online_id;

    const auto parse_presence_status = [](const nlohmann::json &entry) {
        const auto status = entry.value("status", "offline");
        if (status == "online")
            return PresenceStatus::Online;
        if (status == "not_available")
            return PresenceStatus::NotAvailable;
        return PresenceStatus::Offline;
    };

    auto &user_info = emuenv.v3kn.account_state.user_info;
    if (!is_v3kn_logged_in())
        return false;

    try {
        const auto profile_res = v3kn_friend_profile(user_info, target_online_id, get_sys_lang_query_value(emuenv));
        handle_v3kn_status(emuenv, profile_res);
        if (profile_res.curl_res != 0 || profile_res.body.starts_with("ERR:"))
            return false;

        auto json = nlohmann::json::parse(profile_res.body);
        const auto relationship = json.value("relationship", "none");
        if (relationship == "self")
            info.relation = FriendRelation::SELF;
        else if (relationship == "friends")
            info.relation = FriendRelation::FRIEND;
        else if (relationship == "request_sent")
            info.relation = FriendRelation::REQUEST_SENT;
        else if (relationship == "request_received")
            info.relation = FriendRelation::REQUEST_RECEIVED;
        else if (relationship == "blocked")
            info.relation = FriendRelation::BLOCKED;
        else
            info.relation = FriendRelation::NONE;

        if ((info.relation == FriendRelation::FRIEND) || (info.relation == FriendRelation::SELF)) {
            if (json.contains("friends") && json["friends"].is_array()) {
                for (const auto &entry : json["friends"]) {
                    const auto online_id_value = entry.value("online_id", "");
                    if (!online_id_value.empty())
                        info.friends.push_back(online_id_value);
                }
            }
            if (json.contains("now_playing"))
                info.now_playing = json.value("now_playing", "");
            info.presence_status = parse_presence_status(json);
            if (info.presence_status == PresenceStatus::Offline)
                info.now_playing.clear();
        }
        if (json.contains("trophies") && json["trophies"].is_object()) {
            const auto &trophies = json["trophies"];
            info.trophy_info.level = trophies.value("level", info.trophy_info.level);
            info.trophy_info.progress = trophies.value("progress", info.trophy_info.progress);
            info.trophy_info.total_unlocked = trophies.value("total", info.trophy_info.total_unlocked);
            info.trophy_info.bronze = trophies.value("bronze", info.trophy_info.bronze);
            info.trophy_info.silver = trophies.value("silver", info.trophy_info.silver);
            info.trophy_info.gold = trophies.value("gold", info.trophy_info.gold);
            info.trophy_info.platinum = trophies.value("platinum", info.trophy_info.platinum);
        }

        if (json.contains("last_updated_activity"))
            info.last_updated_activity = json.value("last_updated_activity", static_cast<time_t>(0));

        if (json.contains("about_me"))
            info.about_me = json.value("about_me", "");

        return true;
    } catch (const std::exception &e) {
        LOG_ERROR("Failed to parse friend profile for {}: {}", target_online_id, e.what());
        return false;
    }
}

void search_friend(EmuEnvState &emuenv, const std::string &query) {
    auto &profile = emuenv.v3kn.profile_state;
    profile.is_searching.store(true);
    profile.search_results.clear();

    const auto res = v3kn_friend_search(emuenv.v3kn.account_state.user_info, query);
    handle_v3kn_status(emuenv, res);

    if (res.body.starts_with("ERR:") || res.curl_res != 0) {
        profile.last_search_query = query;
        profile.is_searching.store(false);
        return;
    }

    try {
        auto json = nlohmann::json::parse(res.body);

        const auto &entries = json.is_array() ? json : json.value("results", nlohmann::json::array());

        for (const auto &entry : entries) {
            SearchResultEntry e;
            e.online_id = entry.value("online_id", "");
            e.created_at = entry.value("created_at", static_cast<time_t>(0));
            if (!e.online_id.empty())
                profile.search_results.push_back(e);
        }
    } catch (const std::exception &e) {
        LOG_ERROR("Failed to parse search results: {}", e.what());
    }

    profile.last_search_query = query;
    profile.is_searching.store(false);
}

void load_search_avatars(EmuEnvState &emuenv) {
    const auto &results = emuenv.v3kn.profile_state.search_results;
    std::vector<std::string> online_ids;
    online_ids.reserve(results.size());

    for (const auto &entry : results)
        online_ids.push_back(entry.online_id);

    load_user_avatars(emuenv, online_ids);
}

void load_user_avatars(EmuEnvState &emuenv, const std::vector<std::string> &online_ids, net_utils::CurlSession *session) {
    const auto &user_info = emuenv.v3kn.account_state.user_info;
    net_utils::CurlSession local_session{};
    net_utils::CurlSession *active_session = session;
    if (!active_session) {
        local_session = net_utils::init_curl_download_session(user_info.token);
        active_session = &local_session;
    }

    auto &friends_state = emuenv.v3kn.friends_state;
    const auto unique_online_ids = collect_missing_shared_avatar_online_ids(friends_state, online_ids, user_info.online_id);

    for (const auto &online_id : unique_online_ids) {
        if (auto avatar_data = fetch_v3kn_avatar(emuenv, online_id, active_session))
            store_pending_shared_avatar(friends_state, online_id, std::move(*avatar_data));
    }

    if (!session)
        net_utils::cleanup_curl_session(local_session);
}

void load_friends_avatar(EmuEnvState &emuenv, const std::string &group, net_utils::CurlSession *session) {
    const auto &user_info = emuenv.v3kn.account_state.user_info;
    auto &friends_state = emuenv.v3kn.friends_state;
    net_utils::CurlSession local_session{};
    net_utils::CurlSession *active_session = session;
    if (!active_session) {
        local_session = net_utils::init_curl_download_session(user_info.token);
        active_session = &local_session;
    }

    // Load our own V3KN avatar
    if (group == "friends") {
        if (auto avatar_data = fetch_v3kn_avatar(emuenv, user_info.online_id, active_session))
            store_pending_self_avatar(friends_state, std::move(*avatar_data));
    }

    std::vector<std::string> online_ids;

    if (group == "friends") {
        for (const auto &f : friends_state.friends_list)
            online_ids.push_back(f.online_id);
    } else if (group == "friend_requests") {
        for (const auto &r : friends_state.requests_received)
            online_ids.push_back(r.online_id);

        for (const auto &s : friends_state.requests_sent)
            online_ids.push_back(s.online_id);
    } else if (group == "players_blocked") {
        for (const auto &b : friends_state.blocked_players)
            online_ids.push_back(b.online_id);
    }

    load_user_avatars(emuenv, online_ids, active_session);

    if (!session)
        net_utils::cleanup_curl_session(local_session);

    auto &mutable_friends_state = emuenv.v3kn.friends_state;
    if (group == "friends")
        mutable_friends_state.has_loaded_friends_avatars.store(true);
    else if (group == "friend_requests")
        mutable_friends_state.has_loaded_requests_avatars.store(true);
    else if (group == "players_blocked")
        mutable_friends_state.has_loaded_blocked_avatars.store(true);
}

void load_friends_panel(EmuEnvState &emuenv, const std::string &group, net_utils::CurlSession *session) {
    if (group != "friends")
        return;

    const auto &user_info = emuenv.v3kn.account_state.user_info;
    auto &friends_state = emuenv.v3kn.friends_state;
    net_utils::CurlSession local_session{};
    net_utils::CurlSession *active_session = session;
    if (!active_session) {
        local_session = net_utils::init_curl_download_session(user_info.token);
        active_session = &local_session;
    }

    const auto load_panel = [&](const std::string &target_online_id, bool is_self) {
        try {
            auto res = v3kn_panel_download(user_info, target_online_id, active_session);
            if ((res.curl_res != 0) || res.body.empty() || (res.body.starts_with("ERR:")))
                return;

            std::lock_guard<std::mutex> lock(friends_state.images_mutex);
            if (is_self)
                friends_state.pending_v3kn_panel = ImageData{ std::move(res.body) };
            else
                friends_state.pending_friends_panel[target_online_id] = ImageData{ std::move(res.body) };
        } catch (const std::exception &e) {
            LOG_ERROR("Failed to load panel for {}: {}", target_online_id, e.what());
        }
    };

    if (!user_info.online_id.empty())
        load_panel(user_info.online_id, true);

    for (const auto &f : friends_state.friends_list)
        load_panel(f.online_id, false);

    emuenv.v3kn.friends_state.has_loaded_friends_panels.store(true);
    if (!session)
        net_utils::cleanup_curl_session(local_session);
}

void load_friends_list(EmuEnvState &emuenv, const std::string &group, bool load_avatars) {
    std::thread([&emuenv, group, load_avatars]() {
        auto &friends_state = emuenv.v3kn.friends_state;
        const auto parse_presence_status = [](const nlohmann::json &entry) {
            const auto status = entry.value("status", "not_available");
            if (status == "online")
                return PresenceStatus::Online;
            if (status == "offline")
                return PresenceStatus::Offline;
            return PresenceStatus::NotAvailable;
        };

        try {
            friends_state.is_loading_friends.store(true);
            const auto res = v3kn_friend_list(emuenv.v3kn.account_state.user_info, group, get_sys_lang_query_value(emuenv));
            handle_v3kn_status(emuenv, res);

            if (res.body.starts_with("ERR:")) {
                LOG_ERROR("Failed to load friends list: {}", get_v3kn_error_message(emuenv, res));
                friends_state.is_loading_friends.store(false);
                return;
            }

            auto json = nlohmann::json::parse(res.body);
            if (group == "friends") {
                friends_state.friends_list.clear();
                if (json.contains("friends") && json["friends"].is_array()) {
                    for (const auto &f : json["friends"]) {
                        FriendInfo info;
                        info.online_id = f["online_id"].get<std::string>();
                        info.since = f.value("since", 0);
                        info.last_activity = f.value("last_activity", 0);
                        info.now_playing = f.value("now_playing", "");
                        info.trophy_level = f.value("trophy_level", info.trophy_level);
                        info.presence_status = parse_presence_status(f);
                        if (info.presence_status == PresenceStatus::Offline)
                            info.now_playing.clear();
                        friends_state.friends_list.push_back(info);
                    }
                }
                if (json.contains("self") && json["self"].is_object()) {
                    const auto &self_entry = json["self"];
                    friends_state.self_info.online_id = self_entry.value("online_id", "");
                    friends_state.self_info.since = self_entry.value("since", 0);
                    friends_state.self_info.last_activity = self_entry.value("last_activity", 0);
                    friends_state.self_info.now_playing = self_entry.value("now_playing", "");
                    friends_state.self_info.trophy_level = self_entry.value("trophy_level", friends_state.self_info.trophy_level);
                    friends_state.self_info.presence_status = parse_presence_status(self_entry);
                    if (friends_state.self_info.presence_status == PresenceStatus::Offline)
                        friends_state.self_info.now_playing.clear();
                }
            } else if (group == "friend_requests") {
                friends_state.requests_sent.clear();
                friends_state.requests_received.clear();

                if (json.contains("friend_requests") && json["friend_requests"].is_object()) {
                    const auto &requests = json["friend_requests"];
                    if (requests.contains("sent") && requests["sent"].is_array()) {
                        for (const auto &r : requests["sent"]) {
                            FriendInfo info;
                            info.online_id = r.value("online_id", "");
                            info.since = r.value("sent_at", r.value("since", 0));
                            if (!info.online_id.empty())
                                friends_state.requests_sent.push_back(info);
                        }
                    }
                    if (requests.contains("received") && requests["received"].is_array()) {
                        for (const auto &r : requests["received"]) {
                            FriendInfo info;
                            info.online_id = r.value("online_id", "");
                            info.since = r.value("received_at", r.value("since", 0));
                            if (!info.online_id.empty())
                                friends_state.requests_received.push_back(info);
                        }
                    }
                }
            } else if (group == "players_blocked") {
                friends_state.blocked_players.clear();
                const auto add_blocked_entries = [&](const nlohmann::json &entries) {
                    for (const auto &entry : entries) {
                        FriendInfo info;
                        info.online_id = entry.value("online_id", "");
                        info.since = entry.value("blocked_at", entry.value("since", 0));
                        info.presence_status = PresenceStatus::Offline;
                        info.last_activity = 0;
                        info.now_playing.clear();
                        if (!info.online_id.empty())
                            friends_state.blocked_players.push_back(info);
                    }
                };

                if (json.contains("players_blocked") && json["players_blocked"].is_array())
                    add_blocked_entries(json["players_blocked"]);
            }

            friends_state.is_loading_friends.store(false);

            if (load_avatars) {
                const auto &user_info = emuenv.v3kn.account_state.user_info;
                auto download_session = net_utils::init_curl_download_session(user_info.token);
                load_friends_avatar(emuenv, group, &download_session);
                load_friends_panel(emuenv, group, &download_session);
                net_utils::cleanup_curl_session(download_session);
            }

            if (!friends_state.is_polling_friends)
                start_friend_polling(emuenv);
            if (!friends_state.is_presence_active)
                start_friend_presence(emuenv);
        } catch (const std::exception &e) {
            LOG_ERROR("FATAL: Exception in load_friends_list thread: {}", e.what());
        } catch (...) {
            LOG_ERROR("FATAL: Unknown exception in load_friends_list thread");
        }
    }).detach();
}

void start_friend_presence(EmuEnvState &emuenv) {
    auto &friends_state = emuenv.v3kn.friends_state;
    const auto user_info = get_v3kn_user_info(emuenv);
    {
        std::lock_guard lock(friends_state.presence_mutex);
        if (friends_state.is_presence_active.exchange(true))
            return;
        friends_state.stop_presence.store(false);
    }

    std::thread([&emuenv, user_info]() {
        auto &friends_state = emuenv.v3kn.friends_state;
        auto presence_session = net_utils::init_curl_download_session(user_info.token);

        const auto to_presence_string = [](PresenceStatus status) {
            switch (status) {
            case PresenceStatus::Online:
                return "online";
            case PresenceStatus::Offline:
                return "offline";
            default:
                return "not_available";
            }
        };

        const auto send_presence = [&](PresenceStatus status, const std::string &title_id) {
            const auto res = v3kn_friend_presence(user_info, to_presence_string(status),
                status == PresenceStatus::Offline ? std::string{} : title_id, &presence_session, &friends_state.stop_presence);
            if (friends_state.stop_presence.load())
                return;
            handle_v3kn_status(emuenv, res, &user_info);
            if (!res.is_success() || res.body.starts_with("ERR:"))
                LOG_ERROR("Failed to update friend presence: {}", get_v3kn_error_message(emuenv, res));
        };

        std::unique_lock<std::mutex> lock(friends_state.presence_mutex);
        PresenceStatus last_status = friends_state.presence_status.load();
        std::string last_title_id = friends_state.presence_title_id;
        lock.unlock();
        send_presence(last_status, last_title_id);

        while (!friends_state.stop_presence.load() && is_v3kn_logged_in()) {
            lock.lock();
            friends_state.presence_cv.wait_for(lock, std::chrono::seconds(15), [&] {
                return friends_state.stop_presence.load() || friends_state.presence_status.load() != last_status
                    || friends_state.presence_title_id != last_title_id;
            });

            if (friends_state.stop_presence.load()) {
                lock.unlock();
                break;
            }

            last_status = friends_state.presence_status.load();
            last_title_id = friends_state.presence_title_id;
            lock.unlock();
            send_presence(last_status, last_title_id);
        }

        v3kn_friend_presence(user_info, "offline", "", &presence_session);
        net_utils::cleanup_curl_session(presence_session);
        {
            std::lock_guard lock(friends_state.presence_mutex);
            friends_state.is_presence_active = false;
        }
        friends_state.presence_cv.notify_all();
    }).detach();
}

void set_friend_presence_status(FriendsState &friends_state, PresenceStatus status) {
    {
        std::lock_guard lock(friends_state.presence_mutex);
        friends_state.presence_status.store(status);
    }
    friends_state.presence_cv.notify_all();
}

void set_friend_now_playing(FriendsState &friends_state, const std::string &title_id, const std::string &title_name) {
    {
        std::lock_guard lock(friends_state.presence_mutex);
        friends_state.presence_title_id = title_id;
        friends_state.presence_title_name = title_name;
    }
    friends_state.presence_cv.notify_all();
}

std::string get_friend_now_playing_title(FriendsState &friends_state) {
    std::lock_guard lock(friends_state.presence_mutex);
    return friends_state.presence_status.load() == PresenceStatus::Offline ? std::string{} : friends_state.presence_title_name;
}

void start_friend_polling(EmuEnvState &emuenv) {
    std::thread([&emuenv]() {
        auto &friends_state = emuenv.v3kn.friends_state;
        if (friends_state.is_polling_friends)
            return;

        friends_state.is_polling_friends = true;
        friends_state.stop_friends_polling.store(false);
        time_t last_poll = std::time(0);
        auto poll_session = net_utils::init_curl_download_session(emuenv.v3kn.account_state.user_info.token);

        while (!friends_state.stop_friends_polling.load()) {
            if (!is_v3kn_logged_in()) {
                std::unique_lock<std::mutex> lock(friends_state.friends_polling_mutex);
                friends_state.friends_polling_cv.wait_for(lock, std::chrono::seconds(5), [&] {
                    return friends_state.stop_friends_polling.load() || is_v3kn_logged_in();
                });
                continue;
            }

            if (friends_state.presence_status.load() != PresenceStatus::Online) {
                std::unique_lock<std::mutex> lock(friends_state.presence_mutex);
                friends_state.presence_cv.wait_for(lock, std::chrono::seconds(5), [&] {
                    return friends_state.stop_friends_polling.load() || friends_state.presence_status.load() == PresenceStatus::Online;
                });
                continue;
            }

            const auto res = v3kn_friend_poll(emuenv.v3kn.account_state.user_info, last_poll, &poll_session, &friends_state.stop_friends_polling);

            if (friends_state.stop_friends_polling.load())
                break;

            handle_v3kn_status(emuenv, res);

            if (friends_state.stop_friends_polling.load())
                break;

            if (res.body.starts_with("ERR:") || res.curl_res != 0) {
                const auto error_message = get_v3kn_error_message(emuenv, res);
                LOG_ERROR("Failed to poll friends: {}", error_message);

                std::unique_lock<std::mutex> lock(friends_state.friends_polling_mutex);
                friends_state.friends_polling_cv.wait_for(lock, std::chrono::seconds(10), [&] {
                    return friends_state.stop_friends_polling.load();
                });
                continue;
            }

            try {
                auto json = nlohmann::json::parse(res.body);

                LOG_DEBUG_IF(!res.body.empty() && res.body != "{}", "Friends poll response: {}", res.body);

                if (json.contains("activity") && json["activity"].is_array()) {
                    for (const auto &notification : json["activity"]) {
                        const std::string group = notification.value("group", "");
                        if (group.empty())
                            continue;
                        const time_t created_at = notification.value("created_at", static_cast<time_t>(0));
                        if (created_at == 0)
                            continue;
                        const std::string online_id = notification.value("online_id", "");
                        emuenv.v3kn.publish_social_event({ SocialEventType::Activity, group, online_id, created_at, {} });
                        if (group == "commented_on_friend_activity")
                            LOG_INFO("Your Friend {} activity was commented", online_id);
                        else
                            LOG_INFO("Your activity was {}", group);
                    }
                }

                if (json.contains("friend") && json["friend"].is_array()) {
                    for (const auto &event : json["friend"]) {
                        const std::string group = event.value("group", "");
                        if (group.empty())
                            continue;
                        const std::string online_id = event.value("online_id", "");
                        const time_t at = event.value("at", static_cast<time_t>(0));
                        emuenv.v3kn.publish_social_event({ SocialEventType::Friend, group, online_id, at, {} });
                        if (group == "friends_request_received")
                            LOG_INFO("Friend request received from: {}", online_id);
                        else
                            LOG_INFO("Friend is now online: {}", online_id);
                    }
                }

                last_poll = std::time(0);
            } catch (const std::exception &e) {
                LOG_ERROR("Failed to parse polled friends: {}", e.what());
                // Brief wait on parse error before retrying
                std::unique_lock<std::mutex> lock(friends_state.friends_polling_mutex);
                friends_state.friends_polling_cv.wait_for(lock, std::chrono::seconds(5), [&] {
                    return friends_state.stop_friends_polling.load();
                });
            }
            // No wait needed here: the server long-polls (30s timeout)
        }

        net_utils::cleanup_curl_session(poll_session);

        friends_state.is_polling_friends.store(false);
        friends_state.friends_polling_cv.notify_all();
    }).detach();
}

void stop_friend_polling(FriendsState &friends_state) {
    friends_state.stop_friends_polling.store(true);
    {
        std::lock_guard lock(friends_state.presence_mutex);
        friends_state.stop_presence.store(true);
    }
    friends_state.presence_cv.notify_one();
    friends_state.friends_polling_cv.notify_one();

    {
        std::unique_lock<std::mutex> lock(friends_state.presence_mutex);
        friends_state.presence_cv.wait_for(lock, std::chrono::seconds(2), [&friends_state] {
            return !friends_state.is_presence_active.load();
        });
    }

    {
        std::unique_lock<std::mutex> lock(friends_state.friends_polling_mutex);
        friends_state.friends_polling_cv.wait_for(lock, std::chrono::seconds(2), [&friends_state] {
            return !friends_state.is_polling_friends.load();
        });
    }
}

void cleanup_friend_state(FriendsState &friends_state) {
    {
        std::lock_guard lock(friends_state.images_mutex);
        friends_state.loaded_avatar_ids.clear();
        friends_state.pending_friends_avatar.clear();
        friends_state.pending_friends_panel.clear();
        friends_state.pending_v3kn_avatar.reset();
        friends_state.pending_v3kn_panel.reset();
        friends_state.pending_friend_profile_avatar.reset();
        friends_state.pending_friend_profile_avatar_online_id.clear();
        friends_state.pending_friend_profile_panel.reset();
        friends_state.pending_friend_profile_panel_online_id.clear();
    }
    friends_state.self_info = {};
    friends_state.friends_list.clear();
    friends_state.requests_sent.clear();
    friends_state.requests_received.clear();
    friends_state.blocked_players.clear();
    friends_state.has_loaded_friends_avatars.store(false);
    friends_state.has_loaded_friends_panels.store(false);
    friends_state.has_loaded_requests_avatars.store(false);
    friends_state.has_loaded_blocked_avatars.store(false);
}

} // namespace v3kn
