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
#include <condition_variable>
#include <ctime>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct EmuEnvState;

namespace indicator {

struct ContentList {
    std::string id;
};

struct TrophyList {
    std::string id;
};

struct NoticeList {
    std::string type;
    std::string id;
    std::string group;
    time_t time = 0;
    bool is_new = false;

    ContentList content;
    TrophyList trophy;
};

struct NoticeInfo : NoticeList {
    std::string name;
    std::string msg;
};

enum class NoticeAction {
    ADD,
    REMOVE_FULL,
    REMOVE_ICON
};

struct NoticeTask {
    NoticeAction action;
    time_t time;
    std::string user_id;
};

struct DownloadProgress {
    std::atomic<float> progress{ 0 };
    std::atomic<uint64_t> remaining{ 0 };
    std::atomic<uint64_t> downloaded{ 0 };
    std::atomic<bool> is_paused{ false };
    std::atomic<bool> is_waiting{ false };
    net_utils::ProgressState state{};
};

struct DownloadTask {
    std::unique_ptr<DownloadProgress> download_progress;
    std::string id;
    fs::path path;
    std::thread thread;
    uint64_t total_size = 0;
    std::string url;
};

struct DownloadNoticeEvent {
    std::string id;
    bool completed = false;
};

struct IndicatorState {
    IndicatorState() = default;
    ~IndicatorState() = default;

    void get_notice_list(EmuEnvState &emuenv);
    void save_notice_list(EmuEnvState &emuenv);
    void clean_notice_info_new(const std::string &user_id);
    void clean_notice_info_new_and_save(EmuEnvState &emuenv, const std::string &user_id);
    std::vector<time_t> erase_app_notice(const std::string &title_id);
    void queue_download_task(const std::string &id, const std::string &group, const fs::path &path, const std::string &url, uint64_t total_size, bool is_paused);
    std::vector<time_t> remove_notice(time_t time, const std::string &user_id, const fs::path &pref_path);
    std::vector<time_t> remove_notice_by_type_and_content(const std::string &type, const std::string &content_id, const std::string &user_id);
    std::vector<time_t> remove_notice_by_type_and_id(const std::string &type, const std::string &id, const std::string &user_id);
    std::vector<time_t> remove_downloading_notice_by_group(const std::string &group);
    std::vector<time_t> remove_notice_by_type_and_content_and_save(const std::string &type, const std::string &content_id, const std::string &user_id, EmuEnvState &emuenv);
    std::vector<time_t> remove_notice_by_type_and_id_and_save(const std::string &type, const std::string &id, const std::string &user_id, EmuEnvState &emuenv);
    std::vector<time_t> remove_notice_and_save(time_t time, const std::string &user_id, const fs::path &pref_path, EmuEnvState &emuenv);
    void update_notice_type(time_t time, const std::string &user_id, const std::string &new_type);
    void set_download_paused_and_save(const std::string &group, time_t time, const std::string &user_id, bool pause, EmuEnvState &emuenv);
    void set_download_paused(const std::string &group, bool pause);
    void cancel_download(const std::string &group, bool pause_only);
    void resume_download_cancel(const std::string &group);
    NoticeList resume_failed_download_notice(time_t time);
    NoticeList resume_failed_download_notice_and_save(time_t time, EmuEnvState &emuenv);
    void set_all_downloads_paused(bool pause);
    void set_all_downloads_paused_and_save(bool pause, EmuEnvState &emuenv);
    void push_notice_task(NoticeAction action, time_t time, const std::string &user_id);
    std::vector<NoticeTask> take_notice_tasks();
    void destroy_download_manager();
    void init_notice_info(EmuEnvState &emuenv);
    void add_notice(NoticeList info, EmuEnvState &emuenv);
    std::vector<DownloadNoticeEvent> take_download_notice_events();

    std::mutex notice_mutex;
    std::vector<NoticeTask> notice_tasks;
    std::atomic<int64_t> last_notice_time_ms{ 0 };

    std::map<std::string, std::vector<NoticeList>> notice_list;
    std::map<std::string, int> notice_list_count_new;
    int notice_info_count_new = 0;
    std::vector<NoticeInfo> notice_info;

    std::mutex download_mutex;
    std::condition_variable download_cv;
    std::map<std::string, std::shared_ptr<DownloadTask>> download_tasks;
    std::mutex download_notice_events_mutex;
    std::vector<DownloadNoticeEvent> download_notice_events;

    std::atomic<bool> download_manager_running{ false };
    std::thread download_manager_thread;
    std::mutex download_manager_mutex;

    bool notice_info_state = false;
};

IndicatorState &get_state();

} // namespace indicator
