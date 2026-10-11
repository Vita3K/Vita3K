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

#include <cstdint>
#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace v3kn {

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

} // namespace v3kn

using UserInfo = v3kn::UserInfo;
inline const auto &V3KN_SERVER_LIST_NAME = v3kn::V3KN_SERVER_LIST_NAME;
inline const auto &V3KN_SERVER_LIST_URL = v3kn::V3KN_SERVER_LIST_URL;
inline const auto &V3KN_SERVER_PROTOCOL = v3kn::V3KN_SERVER_PROTOCOL;
