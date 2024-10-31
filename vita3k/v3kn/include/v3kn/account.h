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

#include <atomic>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct EmuEnvState;
struct GuiState;

inline const std::vector<const char *> V3KN_SERVER_LIST_NAME = { "Official V3KN Server", "Local V3KN Test Server" };
inline const std::vector<const char *> V3KN_SERVER_LIST_URL = { "np.vita3k.org", "localhost:3000" };
inline const std::map<std::string, std::string> V3KN_SERVER_PROTOCOL = {
    { "np.vita3k.org", "https://" },
    { "localhost:3000", "http://" },
};

struct UserInfo {
    std::string host = V3KN_SERVER_LIST_URL[0];
    std::string online_id;
    std::string password;
    std::string token;
    time_t created_at = 0;
    uint64_t quota_used = 0;
    uint64_t quota_total = 0;
};

namespace v3kn {

void set_v3kn_logged_in(bool logged_in);
bool is_v3kn_logged_in();
bool wait_for_v3kn_login(uint32_t timeout_ms);
void handle_v3kn_status(EmuEnvState &emuenv, const net_utils::WebResponse &response);
std::string get_v3kn_error_message(EmuEnvState &emuenv, const net_utils::WebResponse &response);
std::string get_v3kn_server_url(const std::string &host, const std::string &path);

net_utils::WebResponse v3kn_create(const UserInfo &user_info, const std::string &online_id, const std::string &password);
net_utils::WebResponse v3kn_login(const UserInfo &user_info, const std::string &online_id, const std::string &password);
net_utils::WebResponse v3kn_delete(const UserInfo &user_info, const std::string &password);
net_utils::WebResponse v3kn_change_online_id(const UserInfo &user_info, const std::string &new_online_id);
net_utils::WebResponse v3kn_change_password(const UserInfo &user_info, const std::string &old_password, const std::string &new_password);
net_utils::WebResponse v3kn_change_about_me(const UserInfo &user_info, const std::string &about_me);
net_utils::WebResponse v3kn_avatar_upload(const UserInfo &user_info, const std::string &file_path);
net_utils::WebResponse v3kn_avatar_upload(const UserInfo &user_info, const std::vector<unsigned char> &png_data);
net_utils::WebResponse v3kn_avatar_download(const UserInfo &user_info, const std::string &target_online_id, net_utils::CurlSession *session = nullptr);
net_utils::WebResponse v3kn_panel_upload(const UserInfo &user_info, const std::vector<unsigned char> &png_data);
net_utils::WebResponse v3kn_panel_download(const UserInfo &user_info, const std::string &target_online_id, net_utils::CurlSession *session = nullptr);
void save_v3kn_user_info(EmuEnvState &emuenv);
void init_v3kn_user_info(GuiState &gui, EmuEnvState &emuenv);

} // namespace v3kn
