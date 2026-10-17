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

struct EmuEnvState;

struct GameActivity {
    std::string content_id;
    std::string action_uri;
};

struct TrophyActivity {
    std::string npcomm_id;
    std::string id;
    std::string grade;
    std::string name;
};

struct CommentActivity {
    std::string online_id;
    std::string comment;
    time_t created_at;
};

struct ActivityPostStatus {
    std::string type;
    std::string online_id;
    std::string title_id;
    std::string message;
    time_t created_at;

    std::string title_name;
    std::string friend_online_id;

    GameActivity game_activity;
    TrophyActivity trophy_activity;

    uint32_t comment_count = 0;
    uint32_t like_count = 0;
    std::vector<CommentActivity> comments_activity; // List of comments on this activity
    std::vector<std::string> likes_online_ids; // List of online IDs of users who liked this activity
};

namespace v3kn {
void create_and_post_activity_status(EmuEnvState &emuenv, ActivityPostStatus &status);
time_t get_current_activity_time_ms(EmuEnvState &emuenv);
bool v3kn_activity_comment(EmuEnvState &emuenv, const ActivityPostStatus &status, const std::string &comment);
bool delete_activity_comment(EmuEnvState &emuenv, const ActivityPostStatus &status, const CommentActivity &comment);
void get_user_activities(EmuEnvState &emuenv, const std::string &online_id);
bool like_activity(EmuEnvState &emuenv, const ActivityPostStatus &status);
void load_activity_cache(EmuEnvState &emuenv);
void save_activity_cache(EmuEnvState &emuenv);
void flush_activity_cache(EmuEnvState &emuenv);
bool unlike_activity(EmuEnvState &emuenv, const ActivityPostStatus &status);

} // namespace v3kn
