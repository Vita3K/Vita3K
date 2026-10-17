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

#pragma once

#include <util/net_utils.h>
#include <v3kn/account.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

struct EmuEnvState;
struct FriendsState;

enum class FriendRelation {
    NONE,
    FRIEND,
    REQUEST_SENT,
    REQUEST_RECEIVED,
    BLOCKED,
    SELF,
};

enum class PresenceStatus {
    Offline,
    NotAvailable,
    Online,
};

struct FriendInfo {
    std::string online_id;
    time_t since = 0;
    time_t last_activity = 0;
    std::string now_playing;
    uint32_t trophy_level = 1;
    PresenceStatus presence_status = PresenceStatus::Offline;
};

struct SearchResultEntry {
    std::string online_id;
    time_t created_at = 0;
};

struct FriendTrophyInfo {
    uint32_t level = 1;
    uint32_t progress = 0;
    uint32_t total_unlocked = 0;
    uint32_t bronze = 0;
    uint32_t silver = 0;
    uint32_t gold = 0;
    uint32_t platinum = 0;
};

struct FriendProfileInfo {
    std::string online_id;
    std::vector<std::string> friends;
    FriendRelation relation = FriendRelation::NONE;
    FriendTrophyInfo trophy_info;
    PresenceStatus presence_status = PresenceStatus::Offline;
    std::string now_playing;
    time_t last_updated_activity = 0;
    std::string about_me;
};

namespace v3kn {

struct ImageData {
    std::string bytes;
};

net_utils::WebResponse v3kn_friend_add(const UserInfo &user_info, const std::string &target_online_id);
net_utils::WebResponse v3kn_friend_accept(const UserInfo &user_info, const std::string &target_online_id);
net_utils::WebResponse v3kn_friend_reject(const UserInfo &user_info, const std::string &target_online_id);
net_utils::WebResponse v3kn_friend_remove(const UserInfo &user_info, const std::string &target_online_id);
net_utils::WebResponse v3kn_friend_cancel(const UserInfo &user_info, const std::string &target_online_id);
net_utils::WebResponse v3kn_friend_block(const UserInfo &user_info, const std::string &target_online_id);
net_utils::WebResponse v3kn_friend_unblock(const UserInfo &user_info, const std::string &target_online_id);
net_utils::WebResponse v3kn_friend_list(const UserInfo &user_info, const std::string &group, const std::string &sys_lang);
net_utils::WebResponse v3kn_friend_search(const UserInfo &user_info, const std::string &query);
net_utils::WebResponse v3kn_friend_presence(const UserInfo &user_info, const std::string &status, const std::string &now_playing = "", net_utils::CurlSession *session = nullptr, const std::atomic<bool> *cancel_flag = nullptr);
net_utils::WebResponse v3kn_friend_poll(const UserInfo &user_info, const time_t since, net_utils::CurlSession *session = nullptr, const std::atomic<bool> *cancel_flag = nullptr);
net_utils::WebResponse v3kn_friend_profile(const UserInfo &user_info, const std::string &target_online_id, const std::string &sys_lang);

void load_friends_list(EmuEnvState &emuenv, const std::string &group = "friends", bool load_avatars = false);
void load_user_avatars(EmuEnvState &emuenv, const std::vector<std::string> &online_ids, net_utils::CurlSession *session = nullptr);
void load_friends_avatar(EmuEnvState &emuenv, const std::string &group, net_utils::CurlSession *session = nullptr);
void load_friends_panel(EmuEnvState &emuenv, const std::string &group, net_utils::CurlSession *session = nullptr);
void search_friend(EmuEnvState &emuenv, const std::string &query);
void load_search_avatars(EmuEnvState &emuenv);
void load_friend_profile_avatar(EmuEnvState &emuenv, const std::string &target_online_id);
void load_friend_profile_panel(EmuEnvState &emuenv, const std::string &target_online_id);
bool get_friend_profile_info(EmuEnvState &emuenv, const std::string &target_online_id, FriendProfileInfo &info);
void start_friend_polling(EmuEnvState &emuenv);
void start_friend_presence(EmuEnvState &emuenv);
void set_friend_presence_status(FriendsState &friends_state, PresenceStatus status);
void set_friend_now_playing(FriendsState &friends_state, const std::string &title_id, const std::string &title_name);
std::string get_friend_now_playing_title(FriendsState &friends_state);
void stop_friend_polling(FriendsState &friends_state);
void cleanup_friend_state(FriendsState &friends_state);

} // namespace v3kn
