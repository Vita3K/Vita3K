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

#include <v3kn/account.h>
#include <v3kn/activity.h>
#include <v3kn/friend.h>
#include <v3kn/messages.h>
#include <v3kn/storage.h>

#include <util/net_utils.h>

#include <atomic>
#include <condition_variable>
#include <ctime>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

struct AccountState {
    // Account updates can call status handling and persistence while holding this lock.
    std::recursive_mutex user_info_mutex;
    UserInfo user_info;
    std::atomic<bool> is_logged_in{ false };
    v3kn::V3knServer selected_server = v3kn::V3knServer::Official;
};

struct StorageState {
    OnlineStorageState online_storage_state = ONLINE_STORAGE_SELECT;
    std::mutex savedata_info_mutex;
    std::map<SaveDataType, SaveDataInfo> savedata_info;
    std::atomic<float> progress{ 0 };
    std::atomic<uint64_t> remaining{ 0 };
    std::atomic<uint64_t> bytes_done{ 0 };
    net_utils::ProgressState progress_state{};

    uint64_t progress_done_timestamp = 0;
    uint64_t please_wait_timestamp = 0;
    std::mutex mutex_progress;
    std::condition_variable cv_progress;
    std::atomic<bool> please_wait_done{ false };
    std::atomic<bool> operation_finished{ true };
};

struct ActivityState {
    std::recursive_mutex cache_mutex;
    std::vector<ActivityPostStatus> cache;
    std::vector<ActivityPostStatus> posted_activities;
    std::atomic<uint64_t> last_created_at_ms{ 0 };
    std::mutex language_mutex;
    std::map<std::string, std::string> language;
};

struct FriendsState {
    std::atomic<bool> stop_friends_polling{ false };
    std::mutex presence_mutex;
    std::mutex images_mutex;
    std::unordered_set<std::string> loaded_avatar_ids;
    std::unordered_map<std::string, v3kn::ImageData> pending_friends_avatar;
    std::unordered_map<std::string, v3kn::ImageData> pending_friends_panel;
    std::optional<v3kn::ImageData> pending_v3kn_avatar;
    std::optional<v3kn::ImageData> pending_v3kn_panel;
    std::optional<v3kn::ImageData> pending_friend_profile_avatar;
    std::string pending_friend_profile_avatar_online_id;
    std::optional<v3kn::ImageData> pending_friend_profile_panel;
    std::string pending_friend_profile_panel_online_id;
    std::vector<FriendInfo> friends_list;
    std::vector<FriendInfo> requests_sent;
    std::vector<FriendInfo> requests_received;
    std::vector<FriendInfo> blocked_players;
    std::condition_variable presence_cv;
    std::string presence_title_id;
    std::string presence_title_name;
    std::mutex friends_polling_mutex;
    std::condition_variable friends_polling_cv;
    std::atomic<bool> stop_presence{ false };
    FriendInfo self_info;
    std::atomic<PresenceStatus> presence_status{ PresenceStatus::NotAvailable };
    std::atomic<bool> is_loading_friends{ false };
    std::atomic<bool> has_loaded_friends_avatars{ false };
    std::atomic<bool> has_loaded_friends_panels{ false };
    std::atomic<bool> has_loaded_requests_avatars{ false };
    std::atomic<bool> has_loaded_blocked_avatars{ false };
    std::atomic<bool> is_polling_friends{ false };
    std::atomic<bool> is_presence_active{ false };
};

struct MessagesState {
    std::atomic<bool> stop_message_polling{ false };
    std::vector<Conversation> conversations_list;
    std::vector<Message> current_conversation_messages;
    std::string current_conversation_online_id;
    std::vector<std::string> current_group_participants;
    std::string current_group_name;
    bool is_current_user_creator = false;
    std::atomic<bool> is_loading_messages{ false };
    std::atomic<bool> is_polling_messages{ false };
    std::atomic<bool> stop_polling{ false };
    std::mutex messages_polling_mutex;
    std::condition_variable messages_polling_cv;
};

struct ProfileState {
    FriendProfileInfo profile_info;
    std::atomic<bool> is_loading{ false };
    std::atomic<bool> is_action_pending{ false };
    std::string action_result_message;
    std::vector<SearchResultEntry> search_results;
    std::atomic<bool> is_searching{ false };
    std::string last_search_query;
};

enum class SocialEventType { Friend,
    Activity,
    Message,
    MessagePollError };

struct SocialEvent {
    SocialEventType type;
    std::string group;
    std::string online_id;
    time_t created_at = 0;
    net_utils::WebResponse response;
};

struct V3KNState {
    AccountState account_state;
    ActivityState activity_state;
    FriendsState friends_state;
    MessagesState messages_state;
    StorageState storage_state;
    ProfileState profile_state;

    void publish_social_event(SocialEvent event) {
        std::lock_guard lock(social_events_mutex);
        social_events.push_back(std::move(event));
    }

    std::vector<SocialEvent> take_social_events() {
        std::lock_guard lock(social_events_mutex);
        std::vector<SocialEvent> events;
        events.swap(social_events);
        return events;
    }

private:
    std::mutex social_events_mutex;
    std::vector<SocialEvent> social_events;
};
