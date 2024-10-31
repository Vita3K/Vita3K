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
#include <v3kn/state.h>

#include <config/state.h>
#include <dialog/state.h>
#include <emuenv/state.h>
#include <gui/state.h>
#include <io/state.h>

#include <util/fs.h>
#include <util/log.h>
#include <util/net_utils.h>
#include <util/safe_time.h>

#include <pugixml.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace v3kn {

static std::atomic<bool> is_v3kn_user_logged_in(false);
static std::condition_variable v3kn_login_cv;
static std::mutex v3kn_login_mutex;
static bool v3kn_login_init_completed = false;
constexpr double V3KN_VERSION = 0.01;

static void set_v3kn_login_init_completed(bool completed) {
    {
        std::lock_guard<std::mutex> lock(v3kn_login_mutex);
        v3kn_login_init_completed = completed;
    }

    v3kn_login_cv.notify_all();
}

void set_v3kn_logged_in(bool logged_in) {
    is_v3kn_user_logged_in.store(logged_in);
    v3kn_login_cv.notify_all();
}

bool is_v3kn_logged_in() {
    return is_v3kn_user_logged_in.load();
}

bool wait_for_v3kn_login(uint32_t timeout_ms) {
    std::unique_lock<std::mutex> lock(v3kn_login_mutex);
    v3kn_login_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [] {
        return is_v3kn_user_logged_in.load() || v3kn_login_init_completed;
    });

    return is_v3kn_user_logged_in.load();
}

std::string get_v3kn_error_message(EmuEnvState &emuenv, const net_utils::WebResponse &response) {
    std::string error_msg;
    if (response.body.starts_with("ERR:") || !response.body.starts_with("OK:")) {
        if (response.body == "ERR:MissingToken")
            error_msg = emuenv.common_dialog.lang.common["must_sign_in_vita3k_network"];
        else if (response.body == "ERR:InvalidToken")
            error_msg = "Your session on Vita3K Network has expired. Please sign in again.";
        else if (response.body == "ERR:MissingTitleID")
            error_msg = "V3KN get error Missing title ID.";
        else if (response.body == "ERR:UserExists")
            error_msg = "V3KN create account error User already exists.";
        else if (response.body == "ERR:MissingPassword")
            error_msg = "V3KN get error Missing password.";
        else if (response.body == "ERR:InvalidOnlineID")
            error_msg = "Invalid Online ID for V3KN.\nMake sure it follows the required format.";
        else if (response.body == "ERR:InvalidPassword")
            error_msg = "V3KN login/change password/delete error Invalid password.";
        else if (response.body == "ERR:ServerError")
            error_msg = "V3KN server error.";
        else if (response.body == "ERR:QuotaExceeded")
            error_msg = "V3KN error Quota exceeded.";
        else if (response.body == "ERR:UserNotFound")
            error_msg = "V3KN login error User not found.";
        else if (response.body == "ERR:InvalidType")
            error_msg = "V3KN error Invalid type of download file.";
        else if (response.body == "ERR:InvalidID")
            error_msg = "V3KN error Invalid ID of App.";
        else if (response.body == "ERR:FileNotFound")
            error_msg = "V3KN error File not found.";
        else if (response.body == "ERR:MissingNewPassword")
            error_msg = "V3KN change password error Missing new password.";
        else if (response.body == "ERR:MissingOldPassword")
            error_msg = "V3KN change password error Missing old password.";
        else if (response.body == "ERR:SamePassword")
            error_msg = "V3KN change password error New password cannot be same as the old password.";
        else if (response.body == "ERR:MissingTargetOnlineId")
            error_msg = "V3KN error Missing target online ID.";
        else if (response.body == "ERR:AlreadyFriends")
            error_msg = "V3KN error Already friends with this user.";
        else if (response.body == "ERR:RequestAlreadySent")
            error_msg = "V3KN error Friend request already sent to this user.";
        else if (response.body == "ERR:NoRequestFound")
            error_msg = "V3KN error No friend request found from this user.";
        else if (response.body == "ERR:CannotAddYourself")
            error_msg = "V3KN error Cannot add yourself as a friend.";
        else if (response.body == "ERR:AboutMeTooLong")
            error_msg = "V3KN error 'About Me' text is too long.";
        else if (response.body == "ERR:OutdatedClient")
            error_msg = "Outdated client version. Please update Vita3K to the latest version.";
        else if (response.body.empty()) {
            if (response.curl_res == 3)
                error_msg = "V3KN get error Malformed URL, check server URL.";
            else if (response.curl_res == 6)
                error_msg = emuenv.common_dialog.lang.common["could_not_connect_internet"];
            else if (response.curl_res == 7)
                error_msg = emuenv.common_dialog.lang.common["could_not_connect_server"];
            else if ((response.curl_res == 55) || (response.curl_res == 56))
                error_msg = emuenv.common_dialog.lang.common["vita3k_network_connection_lost"];
            else
                error_msg = fmt::format("V3KN get Unexpected server error: {}", response.curl_res);
        } else {
            error_msg = fmt::format("V3KN: Unexpected server response: {}", response.body);
        }
    }

    return error_msg;
}

std::string get_v3kn_server_url(const std::string &host, const std::string &path) {
    if (V3KN_SERVER_PROTOCOL.find(host) == V3KN_SERVER_PROTOCOL.end()) {
        LOG_WARN("V3KN: Unknown server host {}, defaulting to https", host);
        return fmt::format("https://{}/{}", host, path);
    }

    return fmt::format("{}{}/{}", V3KN_SERVER_PROTOCOL[host], host, path);
}

void handle_v3kn_status(EmuEnvState &emuenv, const net_utils::WebResponse &response) {
    if (response.body.starts_with("OK:")) {
        set_v3kn_logged_in(true);
    } else if (response.body.starts_with("ERR:")) {
        if (response.body == "ERR:InvalidToken") {
            set_v3kn_logged_in(false);
            emuenv.v3kn.account_state.user_info.token.clear();
            save_v3kn_user_info(emuenv);
        }
    } else if (!response.body.empty() && response.curl_res == 0) {
        set_v3kn_logged_in(true);
    }
}

net_utils::WebResponse v3kn_create(const UserInfo &user_info, const std::string &online_id, const std::string &password) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/create");
    const std::string post_data = fmt::format("version={}&online_id={}&password={}", V3KN_VERSION, online_id, password);

    const auto res = net_utils::get_web_response_ex(url, "", post_data);

    return res;
}

net_utils::WebResponse v3kn_delete(const UserInfo &user_info, const std::string &password) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/delete");
    const std::string post_data = fmt::format("password={}", password);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);

    return res;
}

net_utils::WebResponse v3kn_login(const UserInfo &user_info, const std::string &online_id, const std::string &password) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/login");
    const std::string post_data = fmt::format("version={}&online_id={}&password={}", V3KN_VERSION, online_id, password);

    const auto res = net_utils::get_web_response_ex(url, "", post_data);

    return res;
}

net_utils::WebResponse v3kn_change_online_id(const UserInfo &user_info, const std::string &new_online_id) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/change_online_id");
    const std::string post_data = fmt::format("new_online_id={}", new_online_id);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);

    return res;
}

net_utils::WebResponse v3kn_change_password(const UserInfo &user_info, const std::string &old_password, const std::string &new_password) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/change_password");
    const std::string post_data = fmt::format("old_password={}&new_password={}", old_password, new_password);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);

    return res;
}

net_utils::WebResponse v3kn_change_about_me(const UserInfo &user_info, const std::string &about_me) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/change_about_me");
    const std::string post_data = fmt::format("about_me={}", about_me);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);

    return res;
}

net_utils::WebResponse v3kn_avatar_upload(const UserInfo &user_info, const std::string &file_path) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/avatar");
    const auto res = net_utils::upload_file(url, file_path, user_info.token, "");

    return res;
}

net_utils::WebResponse v3kn_avatar_upload(const UserInfo &user_info, const std::vector<unsigned char> &png_data) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/avatar");
    const auto res = net_utils::upload_data(url, png_data, "Avatar.png", user_info.token);
    return res;
}

net_utils::WebResponse v3kn_avatar_download(const UserInfo &user_info, const std::string &target_online_id, net_utils::CurlSession *session) {
    const std::string path = fmt::format("v3kn/avatar?target_online_id={}", target_online_id);
    const std::string url = get_v3kn_server_url(user_info.host, path);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, "", session);
    return res;
}

net_utils::WebResponse v3kn_panel_upload(const UserInfo &user_info, const std::vector<unsigned char> &png_data) {
    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/panel");
    const auto res = net_utils::upload_data(url, png_data, "Panel.png", user_info.token);
    return res;
}

net_utils::WebResponse v3kn_panel_download(const UserInfo &user_info, const std::string &target_online_id, net_utils::CurlSession *session) {
    const std::string path = fmt::format("v3kn/panel?target_online_id={}", target_online_id);
    const std::string url = get_v3kn_server_url(user_info.host, path);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, "", session);
    return res;
}

void save_v3kn_user_info(EmuEnvState &emuenv) {
    const fs::path user_info_path{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "v3kn.xml" };
    auto &user_info = emuenv.v3kn.account_state.user_info;
    pugi::xml_document doc;
    pugi::xml_node user_node = doc.append_child("user");
    user_node.append_attribute("version").set_value(1);
    user_node.append_child("host").text().set(user_info.host.c_str());
    user_node.append_child("online_id").text().set(user_info.online_id.c_str());
    user_node.append_child("password").text().set(user_info.password.c_str());
    user_node.append_child("token").text().set(user_info.token.c_str());
    doc.save_file(user_info_path.string().c_str());
}

void init_v3kn_user_info(GuiState &gui, EmuEnvState &emuenv) {
    auto &account_state = emuenv.v3kn.account_state;
    auto &user_info = account_state.user_info;
    user_info = {};
    account_state.selected_server_index = 0;
    set_v3kn_logged_in(false);
    set_v3kn_login_init_completed(false);

    const fs::path user_info_path{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "v3kn.xml" };
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(user_info_path.string().c_str());
    if (!result) {
        set_v3kn_login_init_completed(true);
        return;
    }
    pugi::xml_node user_node = doc.child("user");
    if (!user_node) {
        LOG_WARN("V3KN user info: missing <user> node");
        set_v3kn_login_init_completed(true);
        return;
    }
    const auto version = user_node.attribute("version").as_uint();
    if (version != 1) {
        LOG_WARN("V3KN user info: unsupported version {}", version);
        set_v3kn_login_init_completed(true);
        return;
    }

    user_info.host = user_node.child("host").text().as_string(V3KN_SERVER_LIST_URL[0]);
    user_info.online_id = user_node.child("online_id").text().as_string();
    user_info.password = user_node.child("password").text().as_string();
    user_info.token = user_node.child("token").text().as_string();

    const auto server_it = std::find(V3KN_SERVER_LIST_URL.begin(), V3KN_SERVER_LIST_URL.end(), user_info.host);
    account_state.selected_server_index = static_cast<uint32_t>(server_it - V3KN_SERVER_LIST_URL.begin());
    if (server_it == V3KN_SERVER_LIST_URL.end())
        account_state.selected_server_index = 0;

    if (user_info.token.empty()) {
        set_v3kn_login_init_completed(true);
        return;
    }

    const std::string url = get_v3kn_server_url(user_info.host, "v3kn/check?version=" + std::to_string(V3KN_VERSION));
    const auto res = net_utils::get_web_response_ex(url, user_info.token, "");

    handle_v3kn_status(emuenv, res);

    if (!res.body.starts_with("OK:")) {
        gui.info_message.function = "v3kn_check";
        gui.info_message.title = emuenv.common_dialog.lang.common["error"];
        gui.info_message.level = spdlog::level::err;
        gui.info_message.msg = get_v3kn_error_message(emuenv, res);

        set_v3kn_login_init_completed(true);
        return;
    }

    std::smatch match;
    std::regex regex_pattern(R"(OK:Connected:([^:]+):([^:]+):([^:]+):([^]+))");
    if (std::regex_search(res.body, match, regex_pattern) && (match.size() == 5)) {
        const std::string current_online_id = match[1].str();
        user_info.created_at = std::strtoull(match[2].str().c_str(), nullptr, 10);
        user_info.quota_used = std::strtoull(match[3].str().c_str(), nullptr, 10);
        user_info.quota_total = std::strtoull(match[4].str().c_str(), nullptr, 10);
        if (current_online_id != user_info.online_id) {
            LOG_INFO("V3KN user info: online ID on server ({}) does not match local info ({}), updating local info", current_online_id, user_info.online_id);
            user_info.online_id = current_online_id;
            save_v3kn_user_info(emuenv);
        }
    } else {
        LOG_WARN("V3KN user info: unexpected server response, connecting again will be required");
        set_v3kn_login_init_completed(true);
        return;
    }

    set_v3kn_logged_in(true);

    set_v3kn_login_init_completed(true);
    LOG_INFO("V3KN user info: successfully logged in as {}", user_info.online_id);
}

} // namespace v3kn
