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

#include <indicator/state.h>

#include <emuenv/state.h>
#include <io/state.h>

#include <pugixml.hpp>

#include <algorithm>
#include <chrono>

#include <util/lock_and_find.h>

namespace indicator {

static indicator::IndicatorState state;

static std::string get_user_id_for_notice(const std::string &type, const EmuEnvState &emuenv) {
    if (type == "trophy")
        return emuenv.io.user_id;

    return "global";
}

static std::vector<time_t> erase_notices_if(const std::string &user_id, auto &&predicate, bool erase_files, const fs::path &pref_path = {}) {
    auto notice_list_it = state.notice_list.find(user_id);
    if (notice_list_it == state.notice_list.end())
        return {};

    std::vector<time_t> erased_times;
    auto &notices = notice_list_it->second;
    for (auto it = notices.begin(); it != notices.end();) {
        if (!predicate(*it)) {
            ++it;
            continue;
        }

        if (it->is_new) {
            --state.notice_list_count_new[user_id];
            --state.notice_info_count_new;
        }

        if (erase_files && (it->type != "download_failed") && !pref_path.empty()) {
            const auto bgdl_path = pref_path / "ux0/bgdl/t" / it->group;
            if (fs::exists(bgdl_path))
                fs::remove_all(bgdl_path);
        }

        erased_times.push_back(it->time);
        LOG_INFO("Notice content: user: {}, id: {}, group: {}, type: {}, time: {} has been removed.", user_id, it->id, it->group, it->type, it->time);
        it = notices.erase(it);
    }

    if (!erased_times.empty()) {
        std::erase_if(state.notice_info, [&](const NoticeInfo &notice) {
            return std::find(erased_times.begin(), erased_times.end(), notice.time) != erased_times.end();
        });
    }

    return erased_times;
}

indicator::IndicatorState &get_state() {
    return state;
}

static void destroy_download_threads(indicator::IndicatorState &state) {
    std::map<std::string, std::shared_ptr<DownloadTask>> tasks;
    {
        std::lock_guard<std::mutex> lock(state.download_mutex);
        tasks.swap(state.download_tasks);
    }

    for (auto &[group, task] : tasks) {
        auto &progress_state = task->download_progress->state;
        {
            std::unique_lock<std::mutex> lock(progress_state.mutex);
            progress_state.canceled = true;
            progress_state.pause = false;
            progress_state.cv.notify_all();
        }

        if (task->thread.joinable())
            task->thread.join();
    }
}

std::vector<time_t> IndicatorState::remove_notice(time_t time, const std::string &user_id, const fs::path &pref_path) {
    return erase_notices_if(user_id, [time](const NoticeList &notice) { return notice.time == time; }, true, pref_path);
}

std::vector<time_t> IndicatorState::remove_notice_by_type_and_content(const std::string &type, const std::string &content_id, const std::string &user_id) {
    return erase_notices_if(user_id, [&](const NoticeList &notice) { return (notice.type == type) && (notice.content.id == content_id); }, false);
}

std::vector<time_t> IndicatorState::remove_notice_by_type_and_id(const std::string &type, const std::string &id, const std::string &user_id) {
    return erase_notices_if(user_id, [&](const NoticeList &notice) { return (notice.type == type) && (notice.id == id); }, false);
}

std::vector<time_t> IndicatorState::remove_downloading_notice_by_group(const std::string &group) {
    return erase_notices_if("global", [&](const NoticeList &notice) { return (notice.type == "downloading") && (notice.group == group); }, false);
}

void IndicatorState::update_notice_type(time_t time, const std::string &user_id, const std::string &new_type) {
    auto notice_list_it = state.notice_list.find(user_id);
    if (notice_list_it == state.notice_list.end())
        return;

    auto &notices = notice_list_it->second;
    auto it = std::find_if(notices.begin(), notices.end(), [time](const NoticeList &notice) {
        return notice.time == time;
    });
    if (it != notices.end())
        it->type = new_type;

    auto info_it = std::find_if(state.notice_info.begin(), state.notice_info.end(), [time](const NoticeInfo &notice) {
        return notice.time == time;
    });
    if (info_it != state.notice_info.end())
        info_it->type = new_type;
}

void IndicatorState::set_download_paused_and_save(const std::string &group, time_t time, const std::string &user_id, bool pause, EmuEnvState &emuenv) {
    set_download_paused(group, pause);
    update_notice_type(time, user_id, pause ? "downloading_paused" : "downloading");
    save_notice_list(emuenv);
}

void IndicatorState::set_download_paused(const std::string &group, bool pause) {
    auto dt = lock_and_find(group, state.download_tasks, state.download_mutex);
    if (!dt)
        return;

    auto &dp = dt->download_progress;
    {
        std::unique_lock<std::mutex> lock(state.download_mutex);
        dp->is_paused = pause;
        dp->is_waiting = !pause;
    }

    if (pause) {
        std::unique_lock<std::mutex> lock(dp->state.mutex);
        dp->state.canceled = true;
    }
}

void IndicatorState::cancel_download(const std::string &group, bool pause_only) {
    auto dt = lock_and_find(group, state.download_tasks, state.download_mutex);
    if (!dt)
        return;

    auto &dp = dt->download_progress;
    std::unique_lock<std::mutex> lock(dp->state.mutex);
    if (pause_only) {
        if (!dp->state.canceled)
            dp->state.pause = true;
        return;
    }

    dp->is_paused = false;
    dp->is_waiting = false;
    dp->state.pause = false;
    dp->state.canceled = true;
    dp->state.cv.notify_one();
}

void IndicatorState::resume_download_cancel(const std::string &group) {
    auto dt = lock_and_find(group, state.download_tasks, state.download_mutex);
    if (!dt)
        return;

    auto &dp = dt->download_progress;
    std::unique_lock<std::mutex> lock(dp->state.mutex);
    if (dp->state.pause) {
        dp->state.pause = false;
        dp->state.cv.notify_one();
    }
}

NoticeList IndicatorState::resume_failed_download_notice(time_t time) {
    auto notice_list_it = state.notice_list.find("global");
    if (notice_list_it == state.notice_list.end())
        return {};

    auto &notices = notice_list_it->second;
    auto it = std::find_if(notices.begin(), notices.end(), [time](const NoticeList &notice) {
        return notice.time == time;
    });
    if (it == notices.end())
        return {};

    NoticeList new_notice = *it;
    new_notice.type = "downloading";
    new_notice.is_new = false;
    new_notice.time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    notices.push_back(new_notice);
    return new_notice;
}

NoticeList IndicatorState::resume_failed_download_notice_and_save(time_t time, EmuEnvState &emuenv) {
    auto notice = resume_failed_download_notice(time);
    if (notice.time != 0)
        save_notice_list(emuenv);

    return notice;
}

void IndicatorState::set_all_downloads_paused(bool pause) {
    {
        std::unique_lock<std::mutex> lock(state.download_mutex);
        for (auto &[group, dt] : state.download_tasks) {
            auto &dp = dt->download_progress;
            if (pause && dp->is_paused.load())
                continue;

            dp->is_paused.store(pause);
            dp->is_waiting.store(!pause);

            if (pause) {
                std::unique_lock<std::mutex> state_lock(dp->state.mutex);
                dp->state.canceled = true;
            }
        }
    }

    for (auto &info : state.notice_info) {
        if (info.type.find("downloading") != std::string::npos)
            info.type = pause ? "downloading_paused" : "downloading";
    }

    for (auto &notice : state.notice_list["global"]) {
        if (notice.type.find("downloading") != std::string::npos)
            notice.type = pause ? "downloading_paused" : "downloading";
    }
}

void IndicatorState::set_all_downloads_paused_and_save(bool pause, EmuEnvState &emuenv) {
    set_all_downloads_paused(pause);
    save_notice_list(emuenv);
}

void IndicatorState::push_notice_task(NoticeAction action, time_t time, const std::string &user_id) {
    state.notice_tasks.push_back({ action, time, user_id });
}

std::vector<NoticeTask> IndicatorState::take_notice_tasks() {
    std::vector<NoticeTask> tasks;
    tasks.swap(state.notice_tasks);
    return tasks;
}

static void download_update(indicator::IndicatorState &state, EmuEnvState &emuenv, const std::string &id, const std::string &group) {
    std::shared_ptr<DownloadTask> task;
    {
        std::lock_guard<std::mutex> lock(state.download_mutex);
        auto it = state.download_tasks.find(group);
        if (it == state.download_tasks.end()) {
            LOG_ERROR("DownloadTask not found for group '{}'", group);
            return;
        }

        task = it->second;
    }

    if (task->url.empty()) {
        LOG_ERROR("No URL provided for update download.");
        return;
    }

    auto &dp = task->download_progress;
    net_utils::ProgressCallback progress_callback = [&](float p, uint64_t r, uint64_t d) -> net_utils::ProgressState * {
        std::lock_guard<std::mutex> lock(state.download_mutex);
        dp->progress = p;
        dp->remaining = r;
        dp->downloaded = d;
        return &dp->state;
    };

    fs::create_directories(task->path.parent_path());
    const auto update_path_str = fs_utils::path_to_utf8(task->path);
    const bool success = net_utils::download_file(task->url, update_path_str, progress_callback);

    const auto is_download_paused = !success && dp->state.canceled && dp->is_paused.load();
    const auto is_download_cancelled = !success && dp->state.canceled && !dp->is_paused.load();
    const bool is_download_completed = success && !dp->state.canceled;
    const bool is_download_failed = !success && !is_download_paused && !is_download_cancelled;

    if (is_download_paused) {
        LOG_INFO("Download paused for group: {}", group);
    } else if (is_download_cancelled) {
        LOG_INFO("Download cancelled for group: {}", group);
    } else if (is_download_failed) {
        LOG_ERROR("Failed to download update from: {}, to: {}, downloaded: {}/{}", task->url, update_path_str, dp->downloaded.load(), task->total_size);
    } else {
        LOG_INFO("Update downloaded successfully: {}, {}/{}", task->url, dp->downloaded.load(), task->total_size);
    }

    if (!is_download_completed && !is_download_failed)
        return;

    {
        std::lock_guard<std::mutex> lock(state.download_notice_events_mutex);
        state.download_notice_events.push_back({ id, is_download_completed });
    }

    dp->is_paused = false;
    dp->is_waiting = false;
    {
        std::unique_lock<std::mutex> lock(dp->state.mutex);
        dp->state.canceled = true;
        dp->progress = 0;
        dp->remaining = 0;
        dp->downloaded = 0;
    }
}

static void download_manager(indicator::IndicatorState &state, EmuEnvState &emuenv) {
    while (state.download_manager_running.load()) {
        std::unique_lock<std::mutex> lock(state.download_manager_mutex);
        state.download_cv.wait(lock, [&state] {
            std::lock_guard<std::mutex> download_lock(state.download_mutex);
            return !state.download_manager_running.load() || !state.download_tasks.empty();
        });
        lock.unlock();

        if (!state.download_manager_running.load())
            break;

        {
            std::unique_lock<std::mutex> download_lock(state.download_mutex);
            for (auto it = state.download_tasks.begin(); it != state.download_tasks.end();) {
                auto &dt = it->second;
                auto &dp = dt->download_progress;

                const bool should_remove = !dp->is_paused.load() && !dp->is_waiting.load() && dp->state.canceled;
                if (should_remove) {
                    const auto should_join = dt->thread.joinable();
                    auto thread = std::move(dt->thread);
                    it = state.download_tasks.erase(it);
                    if (should_join) {
                        download_lock.unlock();
                        thread.join();
                        download_lock.lock();
                    }
                    continue;
                }

                ++it;
            }
        }

        bool has_active_download = false;
        {
            std::lock_guard<std::mutex> download_lock(state.download_mutex);
            has_active_download = std::any_of(state.download_tasks.begin(), state.download_tasks.end(), [](const auto &pair) {
                auto &dp = pair.second->download_progress;
                return !dp->is_waiting.load() && !dp->is_paused.load();
            });
        }

        if (has_active_download)
            continue;

        std::lock_guard<std::mutex> download_lock(state.download_mutex);
        auto it = std::find_if(state.download_tasks.begin(), state.download_tasks.end(), [](const auto &pair) {
            auto &dp = pair.second->download_progress;
            return dp->is_waiting.load();
        });

        if (it != state.download_tasks.end()) {
            auto &dt = it->second;
            auto &dp = dt->download_progress;
            dp->state.canceled = false;
            dp->is_waiting = false;
            dt->thread = std::thread(download_update, std::ref(state), std::ref(emuenv), dt->id, it->first);
        }
    }

    destroy_download_threads(state);
}

void IndicatorState::get_notice_list(EmuEnvState &emuenv) {
    auto &state = get_state();
    state.notice_list.clear();
    state.notice_list_count_new.clear();
    const auto notice_path = emuenv.pref_path / "ux0/user/notice.xml";

    if (!fs::exists(notice_path))
        return;

    pugi::xml_document notice_xml;
    if (!notice_xml.load_file(notice_path.c_str())) {
        LOG_ERROR("Notice XML found is corrupted on path: {}", notice_path);
        fs::remove(notice_path);
        return;
    }

    const auto notice_child = notice_xml.child("notice");
    if (notice_child.child("user").empty())
        return;

    for (const auto &user : notice_child) {
        const auto user_id = std::string(user.attribute("id").as_string());
        state.notice_list_count_new[user_id] = user.attribute("count_new").as_int();

        for (const auto &notice : user) {
            NoticeList notice_list;
            notice_list.type = notice.attribute("type").as_string();
            notice_list.group = notice.attribute("group").as_string();

            if (notice_list.type == "trophy") {
                notice_list.trophy.id = notice.attribute("trophy_id").as_string();
                if (notice_list.trophy.id.empty() && !notice.attribute("content_id").empty())
                    notice_list.trophy.id = notice.attribute("content_id").as_string();
            } else if ((notice_list.type == "content") || (notice_list.type.find("download") != std::string::npos) || (notice_list.type == "wait_install")) {
                notice_list.content.id = notice.attribute("content_id").as_string();
            }

            notice_list.id = notice.attribute("id").as_string();
            notice_list.time = !notice.attribute("time").empty() ? notice.attribute("time").as_ullong() : (notice.attribute("date").as_ullong() * 1000);
            notice_list.is_new = notice.attribute("new").as_bool();
            state.notice_list[user_id].push_back(notice_list);
        }
    }
}

void IndicatorState::save_notice_list(EmuEnvState &emuenv) {
    auto &state = get_state();
    pugi::xml_document notice_xml;
    auto declaration_user = notice_xml.append_child(pugi::node_declaration);
    declaration_user.append_attribute("version") = "1.0";
    declaration_user.append_attribute("encoding") = "utf-8";

    auto notice_child = notice_xml.append_child("notice");
    for (const auto &[user_id, notices] : state.notice_list) {
        auto user_child = notice_child.append_child("user");
        user_child.append_attribute("id") = user_id.c_str();
        user_child.append_attribute("count_new") = state.notice_list_count_new[user_id];

        auto sorted_notices = notices;
        std::sort(sorted_notices.begin(), sorted_notices.end(), [](const NoticeList &a, const NoticeList &b) {
            return a.time > b.time;
        });

        for (const auto &notice : sorted_notices) {
            auto info_child = user_child.append_child("info");
            info_child.append_attribute("type") = notice.type.c_str();
            info_child.append_attribute("id") = notice.id.c_str();
            info_child.append_attribute("group") = notice.group.c_str();

            if (notice.type == "trophy") {
                info_child.append_attribute("trophy_id") = notice.trophy.id.c_str();
            } else if ((notice.type == "content") || (notice.type.find("download") != std::string::npos) || (notice.type == "wait_install")) {
                info_child.append_attribute("content_id") = notice.content.id.c_str();
            }

            info_child.append_attribute("time") = static_cast<unsigned long long>(notice.time);
            info_child.append_attribute("new") = notice.is_new;
        }
    }

    const auto notice_path = emuenv.pref_path / "ux0/user/notice.xml";
    if (!notice_xml.save_file(notice_path.c_str()))
        LOG_ERROR("Fail save xml");
}

void IndicatorState::clean_notice_info_new(const std::string &user_id) {
    state.notice_info_count_new = 0;
    state.notice_list_count_new["global"] = 0;
    state.notice_list_count_new[user_id] = 0;

    for (auto &notice : state.notice_info)
        notice.is_new = false;

    for (auto &[user, lists] : state.notice_list) {
        if ((user == "global") || (user == user_id)) {
            for (auto &list : lists)
                list.is_new = false;
        }
    }
}

void IndicatorState::clean_notice_info_new_and_save(EmuEnvState &emuenv, const std::string &user_id) {
    clean_notice_info_new(user_id);
    save_notice_list(emuenv);
}

std::vector<time_t> IndicatorState::remove_notice_by_type_and_content_and_save(const std::string &type, const std::string &content_id, const std::string &user_id, EmuEnvState &emuenv) {
    auto erased_times = remove_notice_by_type_and_content(type, content_id, user_id);
    if (!erased_times.empty())
        save_notice_list(emuenv);

    return erased_times;
}

std::vector<time_t> IndicatorState::remove_notice_by_type_and_id_and_save(const std::string &type, const std::string &id, const std::string &user_id, EmuEnvState &emuenv) {
    auto erased_times = remove_notice_by_type_and_id(type, id, user_id);
    if (!erased_times.empty())
        save_notice_list(emuenv);

    return erased_times;
}

std::vector<time_t> IndicatorState::remove_notice_and_save(time_t time, const std::string &user_id, const fs::path &pref_path, EmuEnvState &emuenv) {
    auto erased_times = remove_notice(time, user_id, pref_path);
    if (!erased_times.empty())
        save_notice_list(emuenv);

    return erased_times;
}

std::vector<time_t> IndicatorState::erase_app_notice(const std::string &title_id) {
    auto notice_list_it = state.notice_list.find("global");
    if (notice_list_it == state.notice_list.end() || notice_list_it->second.empty()) {
        LOG_WARN("Notice list is empty.");
        return {};
    }

    std::vector<time_t> erased_times;
    auto &notice_global = notice_list_it->second;
    auto it = notice_global.begin();
    while (it != notice_global.end()) {
        if (it->id != title_id) {
            ++it;
            continue;
        }

        erased_times.push_back(it->time);
        std::erase_if(state.notice_info, [&](const NoticeInfo &notice) {
            return notice.time == it->time;
        });

        LOG_INFO("Notice content with title id: {} has been erased.", title_id);
        it = notice_global.erase(it);
    }

    return erased_times;
}

void IndicatorState::queue_download_task(const std::string &id, const std::string &group, const fs::path &path, const std::string &url, uint64_t total_size, bool is_paused) {
    auto task = std::make_shared<DownloadTask>();
    task->id = id;
    task->path = path;
    task->url = url;
    task->total_size = total_size;

    const uint64_t already_downloaded = fs::exists(task->path) ? fs::file_size(task->path) : 0;

    task->download_progress = std::make_unique<DownloadProgress>();
    auto &dp = *task->download_progress;
    dp.downloaded = already_downloaded;
    dp.progress = already_downloaded > 0 ? already_downloaded / static_cast<float>(task->total_size) : 0.0f;
    dp.state.canceled = false;
    dp.state.pause = false;
    dp.is_paused = is_paused;
    dp.is_waiting = !is_paused;

    {
        std::lock_guard<std::mutex> lock(state.download_mutex);
        state.download_tasks.emplace(group, task);
    }

    {
        std::lock_guard<std::mutex> lock(state.download_manager_mutex);
        state.download_cv.notify_all();
    }
}

void IndicatorState::destroy_download_manager() {
    {
        std::lock_guard<std::mutex> lock(state.download_manager_mutex);
        if (state.download_manager_running) {
            state.download_manager_running = false;
            state.download_cv.notify_all();
        }
    }

    if (state.download_manager_thread.joinable())
        state.download_manager_thread.join();
}

void IndicatorState::init_notice_info(EmuEnvState &emuenv) {
    state.notice_info.clear();
    state.notice_info_count_new = 0;
    state.notice_info_count_new = state.notice_list_count_new["global"] + state.notice_list_count_new[emuenv.io.user_id];

    if (!state.download_manager_thread.joinable()) {
        state.download_manager_running = true;
        state.download_manager_thread = std::thread(download_manager, std::ref(state), std::ref(emuenv));
    }
}

void IndicatorState::add_notice(NoticeList info, EmuEnvState &emuenv) {
    const auto user_id = get_user_id_for_notice(info.type, emuenv);
    const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto last_time_ms = static_cast<decltype(now_ms)>(state.last_notice_time_ms.load());
    const auto next_time = std::max(now_ms, last_time_ms + 1);
    state.last_notice_time_ms.store(next_time);
    info.time = static_cast<time_t>(next_time);

    const auto is_new = (info.type != "downloading") && (info.type != "content") && (info.type != "failed_install");
    if (is_new)
        info.is_new = true;

    state.notice_list[user_id].push_back(info);
    if (is_new) {
        ++state.notice_list_count_new[user_id];
        ++state.notice_info_count_new;
    }

    save_notice_list(emuenv);
}

std::vector<DownloadNoticeEvent> IndicatorState::take_download_notice_events() {
    std::vector<DownloadNoticeEvent> events;
    {
        std::lock_guard<std::mutex> lock(state.download_notice_events_mutex);
        events.swap(state.download_notice_events);
    }
    return events;
}

} // namespace indicator
