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

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <util/net_utils.h>
#include <vector>

struct EmuEnvState;
struct MessagesState;

struct Message {
    std::string from;
    std::string msg;
    time_t timestamp = 0;
};

struct Conversation {
    std::string online_id;
    uint32_t count = 0;
    Message last_message;
    std::vector<std::string> participants;
    std::string group_name;
    std::string creator;
};

namespace v3kn {
net_utils::WebResponse v3kn_messages_create(const UserInfo &user_info, const std::vector<std::string> &participants, const std::string &message);
net_utils::WebResponse v3kn_messages_create(UserInfo &user_info, const std::vector<std::string> &participants, const std::string &message);
net_utils::WebResponse v3kn_messages_send(const UserInfo &user_info, const std::string &conversation_id, const std::string &message);
net_utils::WebResponse v3kn_messages_conversations(const UserInfo &user_info);
net_utils::WebResponse v3kn_messages_read(const UserInfo &user_info, const std::string &target_online_id);
net_utils::WebResponse v3kn_messages_poll(const UserInfo &user_info, const time_t since, net_utils::CurlSession *session = nullptr, const std::atomic<bool> *cancel_flag = nullptr);
net_utils::WebResponse v3kn_messages_delete(const UserInfo &user_info, const std::string &conversation_id, const std::vector<time_t> &timestamps);
net_utils::WebResponse v3kn_group_add_member(const UserInfo &user_info, const std::string &group_id, const std::string &target_online_id);
net_utils::WebResponse v3kn_group_remove_member(const UserInfo &user_info, const std::string &group_id, const std::string &target_online_id);
net_utils::WebResponse v3kn_group_leave(const UserInfo &user_info, const std::string &group_id);
net_utils::WebResponse v3kn_group_rename(const UserInfo &user_info, const std::string &group_id, const std::string &new_name);

void load_conversations_list(EmuEnvState &emuenv);
void load_conversation_messages(EmuEnvState &emuenv, const std::string &target_online_id, const std::vector<std::string> &participants = {}, const std::string &group_name = "", const std::string &creator = "");
void start_messages_polling(EmuEnvState &emuenv);
void stop_messages_polling(MessagesState &messages_state);
void cleanup_messages_state(MessagesState &messages_state);

} // namespace v3kn
