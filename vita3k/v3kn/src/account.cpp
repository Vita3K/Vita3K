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

#include <emuenv/state.h>
#include <io/state.h>
#include <util/fs.h>
#include <util/log.h>
#include <util/net_utils.h>
#include <util/vector_utils.h>

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <regex>

#include <pugixml.hpp>

namespace v3kn {

constexpr char V3KN_VERSION[] = "0.01";

std::atomic<bool> is_v3kn_user_logged_in{ false };
std::condition_variable v3kn_login_cv;
std::mutex v3kn_login_mutex;
bool v3kn_login_init_completed = false;

void set_v3kn_login_init_completed(bool completed) {
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

Error get_v3kn_error(const net_utils::WebResponse &response) {
    return {
        .response = response,
        .message = fmt::format("cURL {}, HTTP {}: {}", response.curl_res, response.http_code, response.body),
    };
}

std::string get_v3kn_server_url(const std::string &host, const std::string &path) {
    if (V3KN_SERVER_PROTOCOL.find(host) == V3KN_SERVER_PROTOCOL.end()) {
        LOG_WARN("V3KN: Unknown server host {}, defaulting to https", host);
        return fmt::format("https://{}/{}", host, path);
    }

    return V3KN_SERVER_PROTOCOL.at(host) + host + "/" + path;
}

UserInfo get_v3kn_user_info(EmuEnvState &emuenv) {
    std::lock_guard lock(emuenv.v3kn.account_state.user_info_mutex);
    return emuenv.v3kn.account_state.user_info;
}

void handle_v3kn_status(EmuEnvState &emuenv, const net_utils::WebResponse &response, const UserInfo *request_user_info) {
    auto &account_state = emuenv.v3kn.account_state;
    std::lock_guard lock(account_state.user_info_mutex);
    if (request_user_info && (request_user_info->host != account_state.user_info.host || request_user_info->token != account_state.user_info.token)) {
        LOG_DEBUG("V3KN: Ignoring account status from a request for a previous session");
        return;
    }
    if (!response.is_success()) {
        if (response.http_code == 0 || response.http_code >= 500 || (response.curl_res != 0 && response.curl_res != 22))
            set_v3kn_logged_in(false);
        return;
    }

    if (response.body.starts_with("OK:")) {
        set_v3kn_logged_in(true);
    } else if (response.body.starts_with("ERR:")) {
        if (response.body == "ERR:InvalidToken") {
            set_v3kn_logged_in(false);
            emuenv.v3kn.account_state.user_info.token.clear();
            save_v3kn_user_info(emuenv);
        }
    } else if (!response.body.empty()) {
        set_v3kn_logged_in(true);
    }
}

net_utils::WebResponse v3kn_create(const UserInfo &user_info, const std::string &online_id, const std::string &password) {
    const std::string post_data = fmt::format("version={}&online_id={}&password={}", net_utils::url_encode(V3KN_VERSION), net_utils::url_encode(online_id), net_utils::url_encode(password));
    return net_utils::get_web_response_ex(get_v3kn_server_url(user_info.host, "v3kn/create"), "", post_data);
}

net_utils::WebResponse v3kn_delete(const UserInfo &user_info, const std::string &password) {
    return net_utils::get_web_response_ex(get_v3kn_server_url(user_info.host, "v3kn/delete"), user_info.token, fmt::format("password={}", net_utils::url_encode(password)));
}

net_utils::WebResponse v3kn_login(const UserInfo &user_info, const std::string &online_id, const std::string &password) {
    const std::string post_data = fmt::format("version={}&online_id={}&password={}", net_utils::url_encode(V3KN_VERSION), net_utils::url_encode(online_id), net_utils::url_encode(password));
    return net_utils::get_web_response_ex(get_v3kn_server_url(user_info.host, "v3kn/login"), "", post_data);
}

net_utils::WebResponse v3kn_change_online_id(const UserInfo &user_info, const std::string &new_online_id) {
    return net_utils::get_web_response_ex(get_v3kn_server_url(user_info.host, "v3kn/change_online_id"), user_info.token, fmt::format("new_online_id={}", net_utils::url_encode(new_online_id)));
}

net_utils::WebResponse v3kn_change_password(const UserInfo &user_info, const std::string &old_password, const std::string &new_password) {
    return net_utils::get_web_response_ex(get_v3kn_server_url(user_info.host, "v3kn/change_password"), user_info.token, fmt::format("old_password={}&new_password={}", net_utils::url_encode(old_password), net_utils::url_encode(new_password)));
}

net_utils::WebResponse v3kn_change_about_me(const UserInfo &user_info, const std::string &about_me) {
    return net_utils::get_web_response_ex(get_v3kn_server_url(user_info.host, "v3kn/change_about_me"), user_info.token, fmt::format("about_me={}", net_utils::url_encode(about_me)));
}

net_utils::WebResponse v3kn_avatar_upload(const UserInfo &user_info, const std::string &file_path) {
    return net_utils::upload_file(get_v3kn_server_url(user_info.host, "v3kn/avatar"), file_path, user_info.token, "");
}

net_utils::WebResponse v3kn_avatar_upload(const UserInfo &user_info, const std::vector<unsigned char> &png_data) {
    return net_utils::upload_data(get_v3kn_server_url(user_info.host, "v3kn/avatar"), png_data, "Avatar.png", user_info.token);
}

net_utils::WebResponse v3kn_avatar_download(const UserInfo &user_info, const std::string &target_online_id, net_utils::CurlSession *session) {
    return net_utils::get_web_response_ex(get_v3kn_server_url(user_info.host, fmt::format("v3kn/avatar?target_online_id={}", net_utils::url_encode(target_online_id))), user_info.token, "", session);
}

net_utils::WebResponse v3kn_panel_upload(const UserInfo &user_info, const std::vector<unsigned char> &png_data) {
    return net_utils::upload_data(get_v3kn_server_url(user_info.host, "v3kn/panel"), png_data, "Panel.png", user_info.token);
}

net_utils::WebResponse v3kn_panel_download(const UserInfo &user_info, const std::string &target_online_id, net_utils::CurlSession *session) {
    return net_utils::get_web_response_ex(get_v3kn_server_url(user_info.host, fmt::format("v3kn/panel?target_online_id={}", net_utils::url_encode(target_online_id))), user_info.token, "", session);
}

void save_v3kn_user_info(EmuEnvState &emuenv) {
    std::lock_guard lock(emuenv.v3kn.account_state.user_info_mutex);
    const fs::path user_info_path{ emuenv.vita_fs_path / "ux0/user" / emuenv.io.user_id / "v3kn.xml" };
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

void init_v3kn_user_info(EmuEnvState &emuenv, Error &error) {
    error = {};
    auto &account_state = emuenv.v3kn.account_state;
    UserInfo request_user_info;
    {
        std::lock_guard lock(account_state.user_info_mutex);
        auto &user_info = account_state.user_info;
        user_info = {};
        account_state.selected_server = V3knServer::Official;
        set_v3kn_logged_in(false);
        set_v3kn_login_init_completed(false);

        const fs::path user_info_path{ emuenv.vita_fs_path / "ux0/user" / emuenv.io.user_id / "v3kn.xml" };
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_file(user_info_path.string().c_str());
        if (!result) {
            set_v3kn_login_init_completed(true);
            return;
        }

        const pugi::xml_node user_node = doc.child("user");
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

        user_info.host = user_node.child("host").text().as_string(V3KN_SERVER_LIST_URL[static_cast<uint32_t>(V3knServer::Official)]);
        user_info.online_id = user_node.child("online_id").text().as_string();
        user_info.password = user_node.child("password").text().as_string();
        user_info.token = user_node.child("token").text().as_string();

        const auto server_index = static_cast<uint32_t>(vector_utils::find_index(V3KN_SERVER_LIST_URL, user_info.host));
        account_state.selected_server = server_index < V3KN_SERVER_LIST_URL.size()
            ? static_cast<V3knServer>(server_index)
            : V3knServer::Official;

        if (user_info.token.empty()) {
            set_v3kn_login_init_completed(true);
            return;
        }

        request_user_info = user_info;
    }

    // The check runs without the lock, so the GUI doesn't wait for the server
    const std::string url = get_v3kn_server_url(request_user_info.host, "v3kn/check?version=" + std::string(V3KN_VERSION));
    const auto response = net_utils::get_web_response_ex(url, request_user_info.token);

    std::lock_guard lock(account_state.user_info_mutex);
    auto &user_info = account_state.user_info;
    if (user_info.host != request_user_info.host || user_info.token != request_user_info.token) {
        LOG_DEBUG("V3KN user info: account changed during the check, ignoring its result");
        set_v3kn_login_init_completed(true);
        return;
    }

    handle_v3kn_status(emuenv, response, &request_user_info);

    if (!response.is_success() || !response.body.starts_with("OK:")) {
        error = get_v3kn_error(response);
        LOG_ERROR("V3KN user info: {}", error.message);
        set_v3kn_login_init_completed(true);
        return;
    }

    std::smatch match;
    const std::regex regex_pattern(R"(OK:Connected:([^:]+):(\d+):(\d+):(\d+))");
    if (std::regex_match(response.body, match, regex_pattern) && (match.size() == 5)) {
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
        set_v3kn_logged_in(false);
        set_v3kn_login_init_completed(true);
        return;
    }

    set_v3kn_logged_in(true);

    set_v3kn_login_init_completed(true);
    LOG_INFO("V3KN user info: successfully logged in as {}", user_info.online_id);
}

} // namespace v3kn
