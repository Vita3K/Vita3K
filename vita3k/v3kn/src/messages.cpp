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
#include <v3kn/messages.h>
#include <v3kn/state.h>

#include <emuenv/state.h>

#include <util/log.h>
#include <util/net_utils.h>

#include <nlohmann/json.hpp>

namespace v3kn {

net_utils::WebResponse v3kn_messages_create(const UserInfo &user_info, const std::vector<std::string> &participants, const std::string &message) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/messages/create");

    nlohmann::json json_payload;
    json_payload["participants"] = participants;
    json_payload["message"] = message;
    json_payload["creator"] = user_info.online_id;

    const std::string post_data = json_payload.dump();
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);
    return res;
}

net_utils::WebResponse v3kn_messages_send(const UserInfo &user_info, const std::string &conversation_id, const std::string &message) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/messages/send");
    const std::string post_data = fmt::format("conversation_id={}&message={}", conversation_id, message);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);
    return res;
}

net_utils::WebResponse v3kn_messages_conversations(const UserInfo &user_info) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/messages/conversations");
    const auto res = net_utils::get_web_response_ex(url, user_info.token, "");
    return res;
}

net_utils::WebResponse v3kn_messages_read(const UserInfo &user_info, const std::string &target_online_id) {
    const std::string url = get_v3kn_server_url(user_info.host, fmt::format("v3kn/messages/read?target_online_id={}", target_online_id));
    const auto res = net_utils::get_web_response_ex(url, user_info.token, "");
    return res;
}

net_utils::WebResponse v3kn_messages_poll(const UserInfo &user_info, const time_t since, net_utils::CurlSession *session, const std::atomic<bool> *cancel_flag) {
    const std::string url = get_v3kn_server_url(user_info.host, fmt::format("v3kn/messages/poll?since={}", since));
    const auto res = net_utils::get_web_response_ex(url, user_info.token, "", session, cancel_flag);
    return res;
}

net_utils::WebResponse v3kn_messages_delete(const UserInfo &user_info, const std::string &conversation_id, const std::vector<time_t> &timestamps) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/messages/delete");

    nlohmann::json json_payload;
    json_payload["conversation_id"] = conversation_id;
    json_payload["timestamps"] = nlohmann::json::array();
    for (const auto &ts : timestamps) {
        json_payload["timestamps"].push_back(ts);
    }

    const std::string post_data = json_payload.dump();
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);
    return res;
}

net_utils::WebResponse v3kn_group_add_member(const UserInfo &user_info, const std::string &group_id, const std::string &target_online_id) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/messages/group/add");
    const std::string post_data = fmt::format("group_id={}&target_online_id={}", group_id, target_online_id);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);
    return res;
}

net_utils::WebResponse v3kn_group_remove_member(const UserInfo &user_info, const std::string &group_id, const std::string &target_online_id) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/messages/group/remove");
    const std::string post_data = fmt::format("group_id={}&target_online_id={}", group_id, target_online_id);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);
    return res;
}

net_utils::WebResponse v3kn_group_leave(const UserInfo &user_info, const std::string &group_id) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/messages/group/leave");
    const std::string post_data = fmt::format("group_id={}", group_id);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);
    return res;
}

net_utils::WebResponse v3kn_group_rename(const UserInfo &user_info, const std::string &group_id, const std::string &new_name) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/messages/group/rename");
    const std::string post_data = fmt::format("group_id={}&name={}", group_id, new_name);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);
    return res;
}

void load_conversations_list(EmuEnvState &emuenv) {
    std::thread([&emuenv]() {
        auto &messages = emuenv.v3kn.messages_state;
        messages.is_loading_messages.store(true);

        const auto &user_info = emuenv.v3kn.account_state.user_info;
        const auto res = v3kn_messages_conversations(user_info);
        handle_v3kn_status(emuenv, res);

        if (res.body.starts_with("ERR:") || res.body.empty() || res.curl_res != 0) {
            LOG_ERROR("Failed to load conversations: {}", get_v3kn_error_message(emuenv, res));
            messages.is_loading_messages.store(false);
            return;
        }

        try {
            auto json = nlohmann::json::parse(res.body);
            messages.conversations_list.clear();

            for (const auto &conv : json) {
                Conversation c;
                c.online_id = conv["online_id"].get<std::string>();
                c.count = conv["count"].get<uint32_t>();
                c.group_name = conv.value("group_name", "");
                c.creator = conv.value("creator", "");
                if (conv.contains("participants")) {
                    for (const auto &p : conv["participants"])
                        c.participants.push_back(p.get<std::string>());
                }
                if (conv.contains("last_message")) {
                    c.last_message.from = conv["last_message"]["from"].get<std::string>();
                    c.last_message.msg = conv["last_message"]["msg"].get<std::string>();
                    c.last_message.timestamp = conv["last_message"]["timestamp"].get<time_t>();
                }
                messages.conversations_list.push_back(c);
            }
        } catch (const std::exception &e) {
            LOG_ERROR("Failed to parse conversations list: {}", e.what());
        }

        messages.is_loading_messages.store(false);
    }).detach();
}

void load_conversation_messages(EmuEnvState &emuenv, const std::string &target_online_id, const std::vector<std::string> &participants, const std::string &group_name, const std::string &creator) {
    std::thread([&emuenv, target_online_id, participants, group_name, creator]() {
        auto &messages_state = emuenv.v3kn.messages_state;

        messages_state.current_conversation_online_id = target_online_id;
        messages_state.current_group_participants = participants;
        messages_state.current_group_name = group_name;
        auto &user_info = emuenv.v3kn.account_state.user_info;
        messages_state.is_current_user_creator = (creator == user_info.online_id) || (creator.empty() && participants.size() >= 2);
        messages_state.is_loading_messages.store(true);

        const auto res = v3kn_messages_read(user_info, target_online_id);
        handle_v3kn_status(emuenv, res);

        if (res.body.starts_with("ERR:")) {
            LOG_ERROR("Failed to load messages: {}", get_v3kn_error_message(emuenv, res));
            messages_state.is_loading_messages.store(false);
            return;
        }

        try {
            auto json = nlohmann::json::parse(res.body);
            messages_state.current_conversation_messages.clear();

            for (const auto &msg : json) {
                Message m;
                m.from = msg["from"].get<std::string>();
                m.msg = msg["msg"].get<std::string>();
                m.timestamp = msg["timestamp"].get<time_t>();
                messages_state.current_conversation_messages.push_back(m);
            }
        } catch (const std::exception &e) {
            LOG_ERROR("Failed to parse messages: {}", e.what());
        }

        messages_state.is_loading_messages.store(false);
    }).detach();
}

void start_messages_polling(EmuEnvState &emuenv) {
    std::thread([&emuenv]() {
        auto &messages_state = emuenv.v3kn.messages_state;
        if (messages_state.is_polling_messages)
            return;

        messages_state.is_polling_messages.store(true);
        messages_state.stop_polling.store(false);
        auto &user_info = emuenv.v3kn.account_state.user_info;
        auto poll_session = net_utils::init_curl_download_session(user_info.token);
        time_t last_poll = std::time(0);
        bool reported_poll_error = false;

        while (!messages_state.stop_polling.load() && is_v3kn_logged_in()) {
            const auto res = v3kn_messages_poll(user_info, last_poll, &poll_session, &messages_state.stop_polling);
            handle_v3kn_status(emuenv, res);

            if (messages_state.stop_polling.load())
                break;

            if (res.body.starts_with("ERR:") || res.body.empty() || res.curl_res != 0) {
                if (!res.body.empty())
                    LOG_WARN("Failed to poll messages: {}", get_v3kn_error_message(emuenv, res));
                else
                    LOG_WARN("Failed to poll messages: network error (code {})", res.curl_res);

                if (!reported_poll_error) {
                    emuenv.v3kn.publish_social_event({ SocialEventType::MessagePollError, {}, {}, 0, res });
                    reported_poll_error = true;
                }

                std::unique_lock<std::mutex> lock(messages_state.messages_polling_mutex);
                messages_state.messages_polling_cv.wait_for(lock, std::chrono::seconds(10), [&messages_state] {
                    return messages_state.stop_polling.load();
                });
                continue;
            }

            reported_poll_error = false;
            try {
                auto json = nlohmann::json::parse(res.body);

                if (!json.empty()) {
                    for (const auto &msg : json) {
                        if (!msg.contains("from") || !msg.contains("msg") || !msg.contains("timestamp")) {
                            LOG_WARN("Poll: skipping message with missing fields");
                            continue;
                        }

                        if (msg["from"].is_null() || msg["msg"].is_null()) {
                            LOG_WARN("Poll: skipping message with null fields");
                            continue;
                        }

                        const std::string from = msg["from"].get<std::string>();
                        const std::string message_text = msg["msg"].get<std::string>();

                        if (!messages_state.current_conversation_online_id.empty() && from == messages_state.current_conversation_online_id) {
                            Message m;
                            m.from = from;
                            m.msg = message_text;
                            m.timestamp = msg["timestamp"].get<time_t>();
                            messages_state.current_conversation_messages.push_back(m);
                        } else if (from != user_info.online_id) {
                            emuenv.v3kn.publish_social_event({ SocialEventType::Message, {}, from, 0, {} });
                            LOG_INFO("New message from {}", from);
                        }
                    }
                }

                last_poll = std::time(0);
            } catch (const std::exception &e) {
                LOG_ERROR("Failed to parse polled messages: {}", e.what());
            }

            std::unique_lock<std::mutex> lock(messages_state.messages_polling_mutex);
            messages_state.messages_polling_cv.wait_for(lock, std::chrono::seconds(2), [&messages_state] {
                return messages_state.stop_polling.load();
            });
        }

        net_utils::cleanup_curl_session(poll_session);
        messages_state.is_polling_messages.store(false);
        messages_state.messages_polling_cv.notify_all();
    }).detach();
}

void stop_messages_polling(MessagesState &messages_state) {
    messages_state.stop_polling.store(true);
    messages_state.messages_polling_cv.notify_one();

    {
        std::unique_lock<std::mutex> lock(messages_state.messages_polling_mutex);
        messages_state.messages_polling_cv.wait_for(lock, std::chrono::seconds(2), [&messages_state] {
            return !messages_state.is_polling_messages.load();
        });
    }
}

void cleanup_messages_state(MessagesState &messages_state) {
    messages_state.conversations_list.clear();
    messages_state.current_conversation_messages.clear();
    messages_state.current_conversation_online_id.clear();
    messages_state.current_group_participants.clear();
    messages_state.current_group_name.clear();
    messages_state.is_current_user_creator = false;
}

} // namespace v3kn
