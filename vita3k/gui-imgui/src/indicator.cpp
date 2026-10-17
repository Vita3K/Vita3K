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

#include "private.h"

#include <dialog/state.h>
#include <gui-imgui/functions.h>

#include <config/state.h>
#include <io/state.h>
#include <packages/sce_types.h>
#include <packages/sfo.h>
#include <util/net_utils.h>
#include <util/safe_time.h>
#include <util/string_utils.h>
#include <v3kn/state.h>

#include <io/VitaIoDevice.h>
#include <util/log.h>

#include <pugixml.hpp>
#include <stb_image.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

namespace gui {

struct ActivityList {
    std::string online_id;
    time_t created_at = 0;
};

struct FriendList {
    std::string online_id;
};

struct ContentList {
    std::string id;
};

struct TrophyList {
    std::string id;
};

struct NoticeList {
    std::string id;
    std::string group;
    std::string type;
    time_t time = 0;
    bool is_new = false;

    ActivityList activity;
    ContentList content;
    FriendList friend_info;
    TrophyList trophy;
};

struct NoticeInfo : NoticeList {
    std::string name;
    std::string msg;
};

static std::map<std::string, std::vector<NoticeList>> notice_list;
static std::map<std::string, int> notice_list_count_new;
static int notice_info_count_new = 0;
static std::vector<NoticeInfo> notice_info;

static time_t next_notice_time() {
    static std::atomic<uint64_t> last_notice_time_ms{ 0 };
    const auto now_ms = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    auto previous = last_notice_time_ms.load();
    while (!last_notice_time_ms.compare_exchange_weak(previous, std::max(now_ms, previous + 1))) {
    }
    return static_cast<time_t>(std::max(now_ms, previous + 1));
}

struct DownloadProgress {
    std::atomic<float> progress{ 0.f };
    std::atomic<uint64_t> remaining{ 0 };
    std::atomic<uint64_t> downloaded{ 0 };
    std::atomic<bool> is_paused{ false };
    std::atomic<bool> is_waiting{ false };
    std::atomic<bool> finished{ false };
    net_utils::ProgressState state{};
};

struct DownloadTask {
    std::unique_ptr<DownloadProgress> download_progress;
    std::string id;
    fs::path path;
    std::thread thread;
    uint64_t total_size = 0;
    std::string url;
    std::atomic<bool> remove_on_finish{ false };
};

struct PendingNoticeUpdate {
    std::string type;
    std::string id;
    time_t time = 0;
};

static std::mutex download_mutex;
static std::condition_variable download_cv;
static std::map<std::string, std::shared_ptr<DownloadTask>> download_tasks;
static std::atomic<bool> download_manager_running{ false };
static std::thread download_manager_thread;
static std::mutex pending_notice_mutex;
static std::vector<PendingNoticeUpdate> pending_notice_updates;

void queue_notice_info(const std::string &type, const std::string &id) {
    std::lock_guard lock(pending_notice_mutex);
    pending_notice_updates.push_back({ type, id, 0 });
}

static void queue_remove_notice(const time_t time) {
    std::lock_guard lock(pending_notice_mutex);
    pending_notice_updates.push_back({ "remove_notice", {}, time });
}

void erase_app_notice(GuiState &gui, const std::string &title_id) {
    auto &notice_global = notice_list["global"];

    // Check if the notice list is empty
    if (notice_global.empty()) {
        LOG_WARN("Notice list is empty.");
        return;
    }

    auto notice_list_it = notice_global.begin();
    while (notice_list_it != notice_global.end()) {
        if (notice_list_it->id != title_id) {
            ++notice_list_it;
            continue;
        }

        // Find and erase the corresponding entry in notice_info
        const auto notice_info_it = std::ranges::find_if(notice_info, [&](const NoticeInfo &n) {
            return n.time == notice_list_it->time;
        });
        if (notice_info_it != notice_info.end())
            notice_info.erase(notice_info_it); // Erase the entry from notice_info

        // Erase the entry from notice_info_icon
        gui.notice_info_icon.erase(notice_list_it->time);

        // Erase the item from notice_general and update the iterator
        notice_list_it = notice_global.erase(notice_list_it);

        LOG_INFO("Notice content with title id: {} has been erased.", title_id);
    }
}

static bool init_notice_icon(GuiState &gui, EmuEnvState &emuenv, const fs::path &content_path, const NoticeList &info) {
    gui.notice_info_icon.erase(info.time);
    int32_t width = 0;
    int32_t height = 0;
    vfs::FileBuffer buffer;

    const auto app = get_app_index(gui, emuenv, info.id);
    const bool is_app_icon = content_path.generic_string().starts_with("app/") && app;
    const bool found = is_app_icon
        ? vfs::read_app_file(buffer, emuenv.vita_fs_path, app_asset_path(app->path), "sce_sys/icon0.png", app->title_id)
        : vfs::read_file(VitaIoDevice::ux0, buffer, emuenv.vita_fs_path, content_path);
    if (!found) {
        if (info.type == "trophy") {
            LOG_WARN("Icon no found for trophy id: {} on NpComId: {}", info.trophy.id, info.id);
            return false;
        } else {
            if (!app || !vfs::read_app_file(buffer, emuenv.vita_fs_path, app_asset_path(app->path), "sce_sys/icon0.png", app->title_id)) {
                buffer = init_default_icon(gui, emuenv);
                if (buffer.empty()) {
                    LOG_WARN("Not found default icon for this notice content: {}", info.content.id);
                    return false;
                }
            }
        }
    }
    stbi_uc *data = buffer.empty() ? nullptr : stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
    if (!data) {
        LOG_ERROR("Invalid icon for notice id: {} in path {}.", info.id, content_path);
        return false;
    }
    gui.notice_info_icon[info.time] = ImGui_Texture(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);

    return gui.notice_info_icon.contains(info.time);
}

static void download_update(EmuEnvState &emuenv, const std::string &id, const std::string &group) {
    std::shared_ptr<DownloadTask> task;
    {
        std::lock_guard lock(download_mutex);
        const auto it = download_tasks.find(group);
        if (it == download_tasks.end()) {
            LOG_ERROR("Download task not found for group '{}'.", group);
            return;
        }
        task = it->second;
    }

    auto &progress = *task->download_progress;
    std::error_code error;
    fs::create_directories(task->path.parent_path(), error);
    if (error) {
        LOG_ERROR("Failed to create update download directory '{}': {}", task->path.parent_path(), error.message());
        queue_notice_info("download_failed", id);
        progress.finished = true;
        {
            std::lock_guard lock(progress.state.mutex);
            progress.state.canceled = true;
        }
        download_cv.notify_all();
        return;
    }

    net_utils::ProgressCallback progress_callback = [&](float value, uint64_t remaining, uint64_t downloaded) {
        progress.progress = value / 100.f;
        progress.remaining = remaining;
        progress.downloaded = downloaded;
        return &progress.state;
    };

    const auto download_path = fs_utils::path_to_utf8(task->path);
    LOG_INFO("Downloading app update from '{}' to '{}'.", task->url, download_path);
    const bool success = net_utils::download_file(task->url, download_path, progress_callback);
    const bool paused = progress.is_paused.load();
    bool canceled = false;
    {
        std::lock_guard lock(progress.state.mutex);
        canceled = progress.state.canceled;
        progress.state.pause = false;
        progress.state.cv.notify_all();
    }

    if (success && !canceled) {
        LOG_INFO("App update downloaded successfully: {}", download_path);
        queue_notice_info("wait_install", id);
    } else if (paused) {
        LOG_INFO("App update download paused for '{}'", id);
    } else if (!canceled) {
        LOG_ERROR("Failed to download app update from '{}' to '{}'.", task->url, download_path);
        queue_notice_info("download_failed", id);
    }

    progress.is_waiting = false;
    progress.finished = true;
    {
        std::lock_guard lock(progress.state.mutex);
        progress.state.canceled = true;
    }
    download_cv.notify_all();
}

static void destroy_download_threads() {
    std::map<std::string, std::shared_ptr<DownloadTask>> tasks;
    {
        std::lock_guard lock(download_mutex);
        tasks.swap(download_tasks);
    }

    for (auto &[group, task] : tasks) {
        auto &state = task->download_progress->state;
        {
            std::lock_guard lock(state.mutex);
            state.canceled = true;
            state.pause = false;
            state.cv.notify_all();
        }
    }
    for (auto &[group, task] : tasks) {
        if (task->thread.joinable())
            task->thread.join();
    }
}

static void download_manager(EmuEnvState &emuenv) {
    while (download_manager_running.load()) {
        struct FinishedTask {
            std::thread thread;
            fs::path directory;
            bool remove_data = false;
        };
        std::vector<FinishedTask> finished_tasks;
        {
            std::unique_lock lock(download_mutex);
            download_cv.wait_for(lock, std::chrono::milliseconds(100), [] {
                if (!download_manager_running.load())
                    return true;
                return std::any_of(download_tasks.begin(), download_tasks.end(), [](const auto &entry) {
                    const auto &progress = *entry.second->download_progress;
                    return progress.is_waiting.load() || progress.finished.load();
                });
            });
            if (!download_manager_running.load())
                break;

            for (auto it = download_tasks.begin(); it != download_tasks.end();) {
                auto &task = it->second;
                auto &progress = *task->download_progress;
                if (progress.finished.load()) {
                    FinishedTask finished;
                    if (task->thread.joinable())
                        finished.thread = std::move(task->thread);
                    if (!progress.is_paused.load() && !progress.is_waiting.load()) {
                        finished.directory = task->path.parent_path();
                        finished.remove_data = task->remove_on_finish.load();
                        finished_tasks.emplace_back(std::move(finished));
                        it = download_tasks.erase(it);
                        continue;
                    }
                    if (finished.thread.joinable())
                        finished_tasks.emplace_back(std::move(finished));
                }
                ++it;
            }

            const bool has_active_download = std::any_of(download_tasks.begin(), download_tasks.end(), [](const auto &entry) {
                const auto &progress = *entry.second->download_progress;
                return !progress.finished.load() && !progress.is_paused.load() && !progress.is_waiting.load();
            });
            if (!has_active_download) {
                const auto next = std::find_if(download_tasks.begin(), download_tasks.end(), [](const auto &entry) {
                    const auto &progress = *entry.second->download_progress;
                    return progress.is_waiting.load() && !progress.finished.load();
                });
                if (next != download_tasks.end()) {
                    auto &task = next->second;
                    auto &progress = *task->download_progress;
                    progress.is_waiting = false;
                    progress.finished = false;
                    {
                        std::lock_guard state_lock(progress.state.mutex);
                        progress.state.canceled = false;
                        progress.state.pause = false;
                    }
                    task->thread = std::thread(download_update, std::ref(emuenv), task->id, next->first);
                }
            }
        }

        for (auto &finished : finished_tasks) {
            if (finished.thread.joinable())
                finished.thread.join();
            if (finished.remove_data) {
                std::error_code error;
                fs::remove_all(finished.directory, error);
                if (error)
                    LOG_ERROR("Failed to remove canceled update download directory '{}': {}", finished.directory, error.message());
            }
        }
    }

    destroy_download_threads();
}

void destroy_download_manager() {
    download_manager_running = false;
    download_cv.notify_all();
    if (download_manager_thread.joinable())
        download_manager_thread.join();
}

void begin_update_download(GuiState &gui, EmuEnvState &emuenv, const std::string &id) {
    if (!gui.new_update_infos.contains(id) && !get_remote_update_info(gui, emuenv, id)) {
        LOG_ERROR("Could not load update metadata for app '{}'.", id);
        return;
    }

    update_notice_info(gui, emuenv, "downloading", id);
}

void draw_notifications(GuiState &gui, EmuEnvState &emuenv) {
    Notification notification;
    const double now = ImGui::GetTime();
    {
        std::lock_guard lock(gui.notifications_mutex);
        if (gui.notifications.empty())
            return;
        auto &current = gui.notifications.front();
        if (!current.active) {
            current.start_time = now;
            current.active = true;
        }
        notification = current;
    }

    const double elapsed = now - notification.start_time;
    constexpr double display_time = 4.0;
    if (elapsed >= display_time) {
        std::lock_guard lock(gui.notifications_mutex);
        if (!gui.notifications.empty() && gui.notifications.front().start_time == notification.start_time)
            gui.notifications.erase(gui.notifications.begin());
        return;
    }
    const float alpha = static_cast<float>(std::clamp(std::min(elapsed / 0.15, (display_time - elapsed) / 0.2), 0.0, 1.0));

    const auto &viewport = sdl::get_viewport();
    const ImVec2 viewport_pos(viewport.logical_x, viewport.logical_y);
    const ImVec2 viewport_size(viewport.logical_width, viewport.logical_height);
    const ImVec2 scale(viewport.scale_x * viewport.manual_dpi_scale, viewport.scale_y * viewport.manual_dpi_scale);
    const ImVec2 window_size(410.f * scale.x, 58.f * scale.y);
    const ImVec2 icon_size(36.f * scale.x, 36.f * scale.y);
    const ImVec2 window_pos(viewport_pos.x + viewport_size.x - window_size.x - (16.f * scale.x), viewport_pos.y + (gui.vita_area.home_screen ? 68.f : 35.f) * scale.y);

    ImGui::SetNextWindowPos(window_pos);
    ImGui::SetNextWindowSize(window_size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.f * scale.x);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.867f, 0.902f, 0.918f, 0.9f * alpha));
    ImGui::Begin("##download_toast", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);

    const auto notice_app = get_app_index(gui, emuenv, notification.app_id);
    const auto icon = notice_app ? get_app_icon(gui, notice_app->path) : nullptr;
    if (icon && *icon) {
        ImGui::SetCursorPos(ImVec2(10.f * scale.x, (window_size.y - icon_size.y) / 2.f));
        ImGui::Image(*icon, icon_size);
    }

    const ImVec2 text_pos(56.f * scale.x, 10.f * scale.y);
    ImGui::SetWindowFontScale(viewport.scale_y);
    if (!notification.title.empty()) {
        ImGui::SetCursorPos(text_pos);
        ImGui::TextColored(ImVec4(0.f, 0.f, 0.f, alpha), "%s", notification.title.c_str());
        ImGui::SetCursorPos(ImVec2(text_pos.x, text_pos.y + (24.f * scale.y)));
    } else {
        const auto message_size = ImGui::CalcTextSize(notification.message.c_str());
        ImGui::SetCursorPos(ImVec2(text_pos.x, (window_size.y - message_size.y) / 2.f));
    }
    ImGui::TextColored(ImVec4(0.f, 0.f, 0.f, alpha), "%s", notification.message.c_str());
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

static const char *get_trophy_grade_icon_name(np::trophy::SceNpTrophyGrade trophy_grade) {
    switch (trophy_grade) {
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM:
        return "trophy_platinum";
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD:
        return "trophy_gold";
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER:
        return "trophy_silver";
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE:
        return "trophy_bronze";
    default:
        return nullptr;
    }
}

static std::string get_trophy_grade_label(GuiState &gui, np::trophy::SceNpTrophyGrade trophy_grade) {
    auto &common = gui.lang.common.main;

    switch (trophy_grade) {
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM:
        return common["platinum"];
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD:
        return common["gold"];
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER:
        return common["silver"];
    case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE:
        return common["bronze"];
    default:
        return "?";
    }
}

static bool set_notice_info(GuiState &gui, EmuEnvState &emuenv, const NoticeList &info) {
    std::string msg, name;
    fs::path content_path;

    auto &lang = gui.lang.indicator;
    if (info.type == "activity") {
        if (info.group == "commented_on_friend_activity") {
            msg = fmt::format(fmt::runtime(lang.at("commented_on_activity")), info.activity.online_id);
        } else if (info.group == "commented_on_your_activity" || info.group == "commented_and_likes_your_activity" || info.group == "likes_your_activity") {
            msg = lang.at(info.group);
        } else {
            LOG_WARN("Unknown V3KN activity event: {}", info.group);
            msg = fmt::format("V3KN activity: {}", info.group);
        }
    } else if (info.type == "friend") {
        msg = info.group == "status_online"
            ? fmt::format(fmt::runtime(lang.at("player_is_online")), info.friend_info.online_id)
            : lang.at("friend_requests");
    } else if (info.type == "content") {
        if (info.group.find("gd") != std::string::npos) {
            content_path = fs::path("app") / info.id;
            msg = lang["app_added_home"];
        } else {
            if (info.group == "ac")
                content_path = fs::path("addcont") / info.id / info.content.id;
            else if (info.group.find("gp") != std::string::npos)
                content_path = fs::path("app") / info.id;
            else if (info.group == "theme")
                content_path = fs::path("theme") / info.content.id;
            msg = lang["install_complete"];
        }
        vfs::FileBuffer params;
        if (vfs::read_file(VitaIoDevice::ux0, params, emuenv.vita_fs_path, content_path / "sce_sys/param.sfo")) {
            SfoFile sfo_handle;
            sfo::load(sfo_handle, params);
            if (!sfo::get_data_by_key(name, sfo_handle, fmt::format("TITLE_{:0>2d}", emuenv.cfg.sys_lang)))
                sfo::get_data_by_key(name, sfo_handle, "TITLE");
        } else {
            LOG_WARN("Content not found for id: {}, in path: {}", info.content.id, content_path);
            return false;
        }
        init_notice_icon(gui, emuenv, content_path / "sce_sys/icon0.png", info);
    } else if (info.type.find("downloading") != std::string::npos) {
        if (!gui.new_update_infos.contains(info.id) && !get_remote_update_info(gui, emuenv, info.id))
            return false;

        const auto &update = gui.new_update_infos.at(info.id);
        name = update.title;
        auto &install = gui.updates_install[info.id];
        install.state = UpdateState::DOWNLOADING;

        auto task = std::make_shared<DownloadTask>();
        task->id = info.id;
        task->url = update.url;
        task->total_size = update.size;
        task->path = emuenv.vita_fs_path / "ux0/bgdl/t" / info.group / fs::path(update.url).filename();
        task->download_progress = std::make_unique<DownloadProgress>();
        auto &progress = *task->download_progress;
        std::error_code error;
        if (fs::exists(task->path, error) && !error) {
            const auto downloaded = fs::file_size(task->path, error);
            if (!error) {
                progress.downloaded = downloaded;
                if (update.size != 0)
                    progress.progress = std::min(1.f, static_cast<float>(downloaded) / static_cast<float>(update.size));
            }
        }
        const bool paused = info.type == "downloading_paused";
        progress.is_paused = paused;
        progress.is_waiting = !paused;
        progress.state.canceled = true;

        {
            std::lock_guard lock(download_mutex);
            if (!download_tasks.emplace(info.group, std::move(task)).second) {
                LOG_ERROR("Duplicate app update download group '{}'.", info.group);
                return false;
            }
        }
        download_cv.notify_all();
        content_path = fs::path("app") / info.id / "sce_sys/icon0.png";
        init_notice_icon(gui, emuenv, content_path, info);
    } else if ((info.type == "wait_install") || (info.type == "download_failed") || (info.type == "failed_install")) {
        if (!gui.new_update_infos.contains(info.id) && !get_remote_update_info(gui, emuenv, info.id))
            return false;

        const auto &update = gui.new_update_infos.at(info.id);
        name = update.title;
        auto &install = gui.updates_install[info.id];
        if (info.type == "wait_install") {
            msg = lang["waiting_install"];
            install.content_id = info.content.id;
            install.pkg_path = emuenv.vita_fs_path / "ux0/bgdl/t" / info.group / fs::path(update.url).filename();
            install.state = UpdateState::WAITING_INSTALL;
            if (!fs::exists(install.pkg_path)) {
                LOG_ERROR("Downloaded update package is missing: {}", install.pkg_path);
                return false;
            }
        } else if (info.type == "download_failed") {
            msg = gui.lang.common.main.at("download_failed");
            install.state = UpdateState::FAILED;
        } else {
            msg = lang["install_failed"];
            install.state = UpdateState::FAILED;
        }
        content_path = fs::path("app") / info.id / "sce_sys/icon0.png";
        init_notice_icon(gui, emuenv, content_path, info);
    } else {
        const auto trophy_conf_id_path{ emuenv.vita_fs_path / "ux0/user" / emuenv.io.user_id / "trophy/conf" / info.id };
        const std::string sfm_name = fs::exists(trophy_conf_id_path / fmt::format("TROP_{:0>2d}.SFM", emuenv.cfg.sys_lang)) ? fmt::format("TROP_{:0>2d}.SFM", emuenv.cfg.sys_lang) : "TROP.SFM";

        pugi::xml_document doc;
        if (doc.load_file((trophy_conf_id_path / sfm_name).c_str())) {
            for (const auto &conf : doc.child("trophyconf")) {
                if (conf.name() == std::string("trophy")) {
                    if (info.trophy.id == conf.attribute("id").as_string())
                        name += conf.child("name").text().as_string();
                }
            }
        } else {
            LOG_WARN("Trophy sfm in conf no found for NpComId: {}", info.id);
            return false;
        }
        msg = ::lang::get(::lang::str::trophy_earned);

        content_path = fs::path("user") / emuenv.io.user_id / "trophy/conf" / info.id / fmt::format("TROP{}.PNG", info.trophy.id);
        if (!init_notice_icon(gui, emuenv, content_path, info))
            return false;
    }

    notice_info.push_back({ info, name, msg });

    return true;
}

void init_notice_info(GuiState &gui, EmuEnvState &emuenv) {
    notice_info.clear();
    notice_info_count_new = 0;
    gui.notice_info_icon.clear();

    if (!notice_list.empty()) {
        for (auto &[user, lists] : notice_list) {
            if ((user == "global") || (user == emuenv.io.user_id)) {
                auto notice_it = lists.begin();
                while (notice_it != lists.end()) {
                    if (!set_notice_info(gui, emuenv, *notice_it)) {
                        notice_it = lists.erase(notice_it);
                        save_notice_list(emuenv);
                    } else {
                        ++notice_it;
                    }
                }
            }
        }

        notice_info_count_new = notice_list_count_new["global"] + notice_list_count_new[emuenv.io.user_id];

        // Sort in date order
        std::sort(notice_info.begin(), notice_info.end(), [&](const NoticeInfo &na, const NoticeInfo &nb) {
            return na.time > nb.time;
        });
    }

    if (!download_manager_thread.joinable()) {
        download_manager_running = true;
        download_manager_thread = std::thread(download_manager, std::ref(emuenv));
    }
}

void get_notice_list(EmuEnvState &emuenv) {
    notice_list.clear();
    notice_list_count_new.clear();
    const auto notice_path{ emuenv.vita_fs_path / "ux0/user/notice.xml" };

    if (fs::exists(notice_path)) {
        pugi::xml_document notice_xml;
        if (notice_xml.load_file(notice_path.c_str())) {
            const auto notice_child = notice_xml.child("notice");
            if (!notice_child.child("user").empty()) {
                // Load Notice List
                for (const auto &user : notice_child) {
                    auto user_id = user.attribute("id").as_string();
                    notice_list_count_new[user_id] = user.attribute("count_new").as_int();
                    for (const auto &notice : user) {
                        NoticeList noticeList;
                        noticeList.id = notice.attribute("id").as_string();
                        noticeList.group = notice.attribute("group").as_string();
                        noticeList.type = notice.attribute("type").as_string();
                        if (noticeList.type == "activity") {
                            noticeList.activity.online_id = notice.attribute("online_id").as_string();
                            noticeList.activity.created_at = notice.attribute("created_at").as_llong();
                        } else if (noticeList.type == "friend") {
                            noticeList.friend_info.online_id = notice.attribute("online_id").as_string();
                            if (noticeList.friend_info.online_id.empty())
                                noticeList.friend_info.online_id = notice.attribute("content_id").as_string();
                            if (noticeList.group == "friends_request_received")
                                noticeList.group = "request_received";
                        } else if (noticeList.type == "trophy") {
                            noticeList.trophy.id = notice.attribute("trophy_id").as_string();
                            if (noticeList.trophy.id.empty())
                                noticeList.trophy.id = notice.attribute("content_id").as_string();
                        } else {
                            noticeList.content.id = notice.attribute("content_id").as_string();
                        }
                        noticeList.time = !notice.attribute("time").empty() ? notice.attribute("time").as_llong() : (notice.attribute("date").as_llong() * 1000); // Backward Compat
                        noticeList.is_new = notice.attribute("new").as_bool();
                        notice_list[user_id].push_back(noticeList);
                    }
                }
            }
        } else {
            LOG_ERROR("Notice XML found is corrupted on path: {}", notice_path);
            fs::remove(notice_path);
        }
    }
}

void save_notice_list(EmuEnvState &emuenv) {
    pugi::xml_document notice_xml;
    auto declarationUser = notice_xml.append_child(pugi::node_declaration);
    declarationUser.append_attribute("version") = "1.0";
    declarationUser.append_attribute("encoding") = "utf-8";

    // Save notif
    auto notice_child = notice_xml.append_child("notice");

    for (const auto &user : notice_list) {
        auto user_child = notice_child.append_child("user");
        user_child.append_attribute("id") = user.first.c_str();
        user_child.append_attribute("count_new") = notice_list_count_new[user.first];

        std::sort(notice_list[user.first].begin(), notice_list[user.first].end(), [&](const NoticeList &na, const NoticeList &nb) {
            return na.time > nb.time;
        });

        for (const auto &notice : user.second) {
            auto info_child = user_child.append_child("info");
            info_child.append_attribute("id") = notice.id.c_str();
            info_child.append_attribute("group") = notice.group.c_str();
            info_child.append_attribute("type") = notice.type.c_str();
            if (notice.type == "activity") {
                info_child.append_attribute("created_at") = notice.activity.created_at;
                if (notice.group == "commented_on_friend_activity")
                    info_child.append_attribute("online_id") = notice.activity.online_id.c_str();
            } else if (notice.type == "friend") {
                info_child.append_attribute("online_id") = notice.friend_info.online_id.c_str();
            } else if (notice.type == "trophy") {
                info_child.append_attribute("trophy_id") = notice.trophy.id.c_str();
            } else {
                info_child.append_attribute("content_id") = notice.content.id.c_str();
            }
            info_child.append_attribute("time") = notice.time;
            info_child.append_attribute("new") = notice.is_new;
        }
    }

    const auto notice_path{ emuenv.vita_fs_path / "ux0/user/notice.xml" };
    const auto save_xml = notice_xml.save_file(notice_path.c_str());
    if (!save_xml)
        LOG_ERROR("Fail save xml");
}

void update_notice_info(GuiState &gui, EmuEnvState &emuenv, const std::string &type, const std::string &id) {
    NoticeList info{};
    const std::string user_id = (type == "trophy") ? emuenv.io.user_id : "global";
    bool replaced_update_notice = false;

    if (type == "content") {
        info.id = emuenv.app_info.app_title_id;
        info.content.id = emuenv.app_info.app_content_id;
        info.group = emuenv.app_info.app_category;

        auto &global_notices = notice_list["global"];
        for (auto notice_it = global_notices.begin(); notice_it != global_notices.end();) {
            if (notice_it->type != "wait_install" || notice_it->content.id != info.content.id) {
                ++notice_it;
                continue;
            }
            replaced_update_notice = true;
            if (notice_it->is_new) {
                notice_list_count_new["global"] = std::max(0, notice_list_count_new["global"] - 1);
                notice_info_count_new = std::max(0, notice_info_count_new - 1);
            }
            gui.notice_info_icon.erase(notice_it->time);
            std::erase_if(notice_info, [&](const NoticeInfo &notice) { return notice.time == notice_it->time; });
            notice_it = global_notices.erase(notice_it);
        }
    } else if (type == "downloading") {
        if (!gui.new_update_infos.contains(id) && !get_remote_update_info(gui, emuenv, id)) {
            LOG_ERROR("Could not load update metadata for app '{}'.", id);
            return;
        }
        const auto &update = gui.new_update_infos.at(id);
        info.id = update.titleid;
        info.content.id = update.content_id;
        const auto failed_notice = std::find_if(notice_list["global"].begin(), notice_list["global"].end(), [&](const NoticeList &notice) {
            return notice.id == update.titleid && notice.type == "download_failed";
        });
        if (failed_notice != notice_list["global"].end()) {
            info.group = failed_notice->group;
            if (failed_notice->is_new) {
                notice_list_count_new["global"] = std::max(0, notice_list_count_new["global"] - 1);
                notice_info_count_new = std::max(0, notice_info_count_new - 1);
            }
            gui.notice_info_icon.erase(failed_notice->time);
            std::erase_if(notice_info, [&](const NoticeInfo &notice) { return notice.time == failed_notice->time; });
            notice_list["global"].erase(failed_notice);
        } else {
            const auto bgdl_path = emuenv.vita_fs_path / "ux0/bgdl/t";
            for (int i = 0; i <= 99; ++i) {
                const auto group = fmt::format("{:02}", i);
                const auto path = bgdl_path / group;
                std::error_code error;
                const bool occupied_task = [&]() {
                    std::lock_guard lock(download_mutex);
                    return download_tasks.contains(group);
                }();
                if (!occupied_task && (!fs::exists(path, error) || (!error && fs::is_empty(path, error)))) {
                    info.group = group;
                    break;
                }
            }
        }
        if (info.group.empty()) {
            LOG_ERROR("No free update download slot is available for app '{}'.", id);
            return;
        }
    } else if (type == "wait_install" || type == "download_failed") {
        if (!gui.new_update_infos.contains(id) && !get_remote_update_info(gui, emuenv, id)) {
            LOG_ERROR("Could not load update metadata for app '{}'.", id);
            return;
        }
        const auto &update = gui.new_update_infos.at(id);
        const auto current = std::find_if(notice_list["global"].begin(), notice_list["global"].end(), [&](const NoticeList &notice) {
            return notice.id == update.titleid && notice.type.find("downloading") != std::string::npos;
        });
        if (current == notice_list["global"].end()) {
            LOG_ERROR("No active download notice found for app '{}'.", id);
            return;
        }
        info = *current;
        info.type = type;
        info.content.id = update.content_id;
        if (type == "wait_install") {
            const auto package_path = emuenv.vita_fs_path / "ux0/bgdl/t" / info.group / fs::path(update.url).filename();
            if (!fs::exists(package_path)) {
                LOG_ERROR("Completed update package is missing: {}", package_path);
                return;
            }
        }
        gui.notice_info_icon.erase(current->time);
        std::erase_if(notice_info, [&](const NoticeInfo &notice) { return notice.time == current->time; });
        notice_list["global"].erase(current);
    } else if (type == "failed_install") {
        if (!gui.new_update_infos.contains(id) && !get_remote_update_info(gui, emuenv, id)) {
            LOG_ERROR("Could not load update metadata for app '{}'.", id);
            return;
        }
        const auto &update = gui.new_update_infos.at(id);
        auto &global_notices = notice_list["global"];
        const auto current = std::find_if(global_notices.begin(), global_notices.end(), [&](const NoticeList &notice) {
            return notice.id == update.titleid && notice.type == "wait_install";
        });
        if (current != global_notices.end()) {
            info = *current;
            gui.notice_info_icon.erase(current->time);
            std::erase_if(notice_info, [&](const NoticeInfo &notice) { return notice.time == current->time; });
            global_notices.erase(current);
        }
        info.id = update.titleid;
        info.content.id = update.content_id;
        info.group = "gp";
    } else {
        if (gui.trophy_unlock_display_requests.empty()) {
            LOG_ERROR("Cannot create a trophy notice without a pending trophy unlock.");
            return;
        }
        const auto &trophy_data = gui.trophy_unlock_display_requests.back();
        info.id = trophy_data.np_com_id;
        info.trophy.id = trophy_data.trophy_id;
        info.group = std::to_string(static_cast<int>(trophy_data.trophy_kind));
    }

    info.type = type;
    info.time = next_notice_time();
    info.is_new = (type == "trophy") || (type == "wait_install") || (type == "download_failed") || (type == "content" && !replaced_update_notice);

    if (!set_notice_info(gui, emuenv, info)) {
        LOG_ERROR("Failed to create {} notice for app/content '{}'.", type, info.id);
        return;
    }
    notice_list[user_id].push_back(info);
    if (info.is_new) {
        ++notice_list_count_new[user_id];
        ++notice_info_count_new;
    }

    if (type == "wait_install" || type == "download_failed") {
        const auto app_index = get_app_index(gui, emuenv, info.id);
        const std::string title = app_index ? app_index->title : info.id;
        std::lock_guard lock(gui.notifications_mutex);
        gui.notifications.push_back({ info.id, title, type == "wait_install" ? gui.lang.indicator["downloading_complete"] : gui.lang.common.main.at("download_failed") });
    }

    std::sort(notice_info.begin(), notice_info.end(), [&](const NoticeInfo &a, const NoticeInfo &b) { return a.time > b.time; });
    save_notice_list(emuenv);
}

void add_social_notice(GuiState &gui, EmuEnvState &emuenv, const SocialEvent &event) {
    if (emuenv.io.user_id.empty()) {
        LOG_ERROR("Cannot save V3KN notice without an active user");
        return;
    }

    NoticeList info{};
    info.id = "NPXS10006";
    info.type = event.type == SocialEventType::Activity ? "activity" : "friend";
    info.group = event.group == "friends_request_received" ? "request_received" : event.group;
    if (info.type == "friend") {
        info.friend_info.online_id = event.online_id;
    } else {
        if (info.group == "commented_on_friend_activity")
            info.activity.online_id = event.online_id;
        info.activity.created_at = event.created_at;
    }
    info.time = next_notice_time();
    info.is_new = true;

    if (!set_notice_info(gui, emuenv, info)) {
        LOG_ERROR("Failed to create V3KN {} notice for '{}'", info.type, info.group);
        return;
    }

    notice_list[emuenv.io.user_id].push_back(info);
    ++notice_list_count_new[emuenv.io.user_id];
    ++notice_info_count_new;
    {
        std::lock_guard lock(gui.notifications_mutex);
        gui.notifications.push_back({ info.id, {}, notice_info.back().msg });
    }
    std::sort(notice_info.begin(), notice_info.end(), [](const NoticeInfo &a, const NoticeInfo &b) { return a.time > b.time; });
    save_notice_list(emuenv);
}

static void remove_notice(GuiState &gui, EmuEnvState &emuenv, time_t time);

static std::shared_ptr<DownloadTask> find_download_task(const std::string &group) {
    std::lock_guard lock(download_mutex);
    const auto it = download_tasks.find(group);
    return it == download_tasks.end() ? nullptr : it->second;
}

static void cancel_download(const std::string &group, const bool remove_data, const fs::path &directory) {
    std::shared_ptr<DownloadTask> task;
    {
        std::lock_guard lock(download_mutex);
        const auto it = download_tasks.find(group);
        if (it != download_tasks.end()) {
            task = it->second;
            task->remove_on_finish = remove_data;
            auto &progress = *task->download_progress;
            progress.is_paused = false;
            progress.is_waiting = false;
            std::lock_guard state_lock(progress.state.mutex);
            progress.state.pause = false;
            progress.state.canceled = true;
            progress.state.cv.notify_all();
        }
    }
    download_cv.notify_all();
    if (!task && remove_data) {
        std::error_code error;
        fs::remove_all(directory, error);
        if (error)
            LOG_ERROR("Failed to remove canceled update download directory '{}': {}", directory, error.message());
    }
}

static void remove_notice(GuiState &gui, EmuEnvState &emuenv, time_t time) {
    for (auto &[user_id, notices] : notice_list) {
        const auto it = std::find_if(notices.begin(), notices.end(), [&](const NoticeList &notice) { return notice.time == time; });
        if (it == notices.end())
            continue;

        const auto notice = *it;
        if (notice.is_new) {
            notice_list_count_new[user_id] = std::max(0, notice_list_count_new[user_id] - 1);
            notice_info_count_new = std::max(0, notice_info_count_new - 1);
        }
        if (notice.type.find("downloading") != std::string::npos || notice.type == "download_failed")
            cancel_download(notice.group, true, emuenv.vita_fs_path / "ux0/bgdl/t" / notice.group);
        else if (notice.type == "wait_install") {
            std::error_code error;
            fs::remove_all(emuenv.vita_fs_path / "ux0/bgdl/t" / notice.group, error);
            if (error)
                LOG_ERROR("Failed to remove update package directory: {}", error.message());
        }

        notices.erase(it);
        std::erase_if(notice_info, [&](const NoticeInfo &info) { return info.time == time; });
        gui.notice_info_icon.erase(time);
        if (notice.type.find("download") != std::string::npos || notice.type == "wait_install" || notice.type == "failed_install")
            gui.updates_install.erase(notice.id);
        save_notice_list(emuenv);
        return;
    }
}

void process_pending_notice_updates(GuiState &gui, EmuEnvState &emuenv) {
    std::vector<PendingNoticeUpdate> pending;
    {
        std::lock_guard lock(pending_notice_mutex);
        pending.swap(pending_notice_updates);
    }

    for (const auto &event : pending) {
        if (event.type == "remove_notice") {
            remove_notice(gui, emuenv, event.time);
        } else if (event.type == "install") {
            update_install(gui, emuenv, event.id);
        } else if (event.type == "retry") {
            begin_update_download(gui, emuenv, event.id);
        } else if (event.type == "update_success") {
            init_user_app(gui, emuenv, "ux0:app/" + event.id);
            {
                std::lock_guard lock(gui.update_state_mutex);
                gui.app_has_update[event.id] = false;
            }
            gui.new_update_infos.erase(event.id);
            gui.updates_install[event.id].state = UpdateState::SUCCESS;
            gui.vita_area.pkg_install = false;
            update_notice_info(gui, emuenv, "content");
        } else if (event.type == "update_failed") {
            gui.vita_area.pkg_install = false;
            if (const auto it = gui.updates_install.find(event.id); it != gui.updates_install.end())
                it->second.state = UpdateState::FAILED;
            update_notice_info(gui, emuenv, "failed_install", event.id);
        } else {
            update_notice_info(gui, emuenv, event.type, event.id);
        }
    }
}

static void clean_notice_info_new(const std::string &user_id) {
    notice_info_count_new = 0;
    notice_list_count_new["global"] = 0;
    notice_list_count_new[user_id] = 0;
    for (auto &notice : notice_info)
        notice.is_new = false;
    for (auto &[user, lists] : notice_list) {
        if ((user == "global") || (user == user_id)) {
            for (auto &list : lists)
                list.is_new = false;
        }
    }
}

static std::string get_notice_time(GuiState &gui, EmuEnvState &emuenv, const time_t &time) {
    std::string date;
    const auto time_in_second = time / 1000;
    const auto diff_time = difftime(std::time(nullptr), time_in_second);
    constexpr auto minute = 60;
    constexpr auto hour = minute * 60;
    constexpr auto day = hour * 24;
    if (diff_time >= day) {
        tm date_tm = {};
        SAFE_LOCALTIME(&time_in_second, &date_tm);
        auto DATE_TIME = get_date_time(gui, emuenv, date_tm);
        date = fmt::format("{} {}", DATE_TIME[DateTime::DATE_MINI], DATE_TIME[DateTime::CLOCK]);
        if (emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR)
            date += fmt::format(" {}", DATE_TIME[DateTime::DAY_MOMENT]);
    } else {
        auto &lang = gui.lang.common.main;
        if (diff_time >= (hour * 2))
            date = fmt::format(fmt::runtime(lang["hours_ago"]), static_cast<uint32_t>(diff_time / hour));
        else if (diff_time >= hour)
            date = lang["one_hour_ago"];
        else if (diff_time >= (minute * 2))
            date = fmt::format(fmt::runtime(lang["minutes_ago"]), static_cast<uint32_t>(diff_time / 60));
        else
            date = lang["one_minute_ago"];
    }

    return date;
}

void draw_indicator(GuiState &gui, EmuEnvState &emuenv) {
    static bool notice_info_state;
    const auto &viewport = sdl::get_viewport();
    const ImVec2 VIEWPORT_POS(viewport.logical_x, viewport.logical_y);
    const ImVec2 VIEWPORT_SIZE(viewport.logical_width, viewport.logical_height);
    const ImVec2 RES_SCALE(viewport.scale_x, viewport.scale_y);
    const ImVec2 SCALE(RES_SCALE.x * viewport.manual_dpi_scale, RES_SCALE.y * viewport.manual_dpi_scale);

    const auto VIEWPORT_WIDTH_POS_MAX(VIEWPORT_POS.x + VIEWPORT_SIZE.x);

    const ImVec2 NOTICE_SIZE = notice_info_count_new ? ImVec2(108.f * SCALE.x, 99.0f * SCALE.y) : ImVec2(88.f * SCALE.x, 81.f * SCALE.y);
    const ImVec2 NOTICE_ICON_POS(VIEWPORT_WIDTH_POS_MAX - NOTICE_SIZE.x, VIEWPORT_POS.y);
    const ImVec2 NOTICE_ICON_POS_MAX(NOTICE_ICON_POS.x + NOTICE_SIZE.x, NOTICE_ICON_POS.y + NOTICE_SIZE.y);

    const auto NOTICE_COLOR = gui.information_bar_color.notice_font;

    const ImVec2 WINDOW_SIZE = notice_info_state ? VIEWPORT_SIZE : (notice_info_count_new ? ImVec2(86.f * SCALE.x, 78.f * SCALE.y) : ImVec2(70.f * SCALE.x, 64.f * SCALE.y));
    const ImVec2 WINDOW_POS = ImVec2(VIEWPORT_POS.x + (notice_info_state ? 0.f : VIEWPORT_SIZE.x - WINDOW_SIZE.x), VIEWPORT_POS.y);

    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("##notice_info", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    const auto draw_list = ImGui::GetWindowDrawList();

    const float tex_w = NOTICE_SIZE.x;
    const float tex_h = NOTICE_SIZE.y;
    const ImVec2 uv0(0.5f / tex_w, 0.5f / tex_h);
    const ImVec2 uv1((tex_w - 0.5f) / tex_w, (tex_h - 0.5f) / tex_h);

    const auto FG_DRAW_LIST = ImGui::GetForegroundDrawList();
    FG_DRAW_LIST->PushClipRect(VIEWPORT_POS, ImVec2(VIEWPORT_WIDTH_POS_MAX, VIEWPORT_POS.y + VIEWPORT_SIZE.y), true);
    if (notice_info_count_new) {
        if (gui.theme_information_bar_notice.contains(NoticeIcon::NEW))
            FG_DRAW_LIST->AddImage(gui.theme_information_bar_notice[NoticeIcon::NEW], NOTICE_ICON_POS, NOTICE_ICON_POS_MAX, uv0, uv1);
        else if (gui.vita_icons.contains("indicator_new"))
            FG_DRAW_LIST->AddImage(gui.vita_icons.at("indicator_new"), NOTICE_ICON_POS, NOTICE_ICON_POS_MAX, uv0, uv1);
        else
            FG_DRAW_LIST->AddCircleFilled(ImVec2(VIEWPORT_WIDTH_POS_MAX - (23.f * SCALE.x), VIEWPORT_POS.y + (14.f * SCALE.y)), 61.f * SCALE.x, IM_COL32(11.f, 90.f, 252.f, 255.f));
        const auto FONT_SCALE = 40.f * SCALE.x;
        const auto NOTICE_COUNT_FONT_SCALE = FONT_SCALE / 40.f;
        const auto NOTICE_COUNT_SIZE = ImGui::CalcTextSize(std::to_string(notice_info_count_new).c_str()).x * NOTICE_COUNT_FONT_SCALE;
        FG_DRAW_LIST->AddText(gui.vita_font[gui.current_font_level], FONT_SCALE, ImVec2(VIEWPORT_WIDTH_POS_MAX - (NOTICE_SIZE.x / 2.f) - (NOTICE_COUNT_SIZE / 2.f) + (12.f * SCALE.x), VIEWPORT_POS.y + (15.f * SCALE.y)), NOTICE_COLOR, std::to_string(notice_info_count_new).c_str());
    } else {
        if (gui.theme_information_bar_notice.contains(NoticeIcon::NO))
            FG_DRAW_LIST->AddImage(gui.theme_information_bar_notice[NoticeIcon::NO], NOTICE_ICON_POS, NOTICE_ICON_POS_MAX, uv0, uv1);
        else if (gui.vita_icons.contains("indicator_no"))
            FG_DRAW_LIST->AddImage(gui.vita_icons.at("indicator_no"), NOTICE_ICON_POS, NOTICE_ICON_POS_MAX, uv0, uv1);
        else
            FG_DRAW_LIST->AddCircleFilled(ImVec2(VIEWPORT_WIDTH_POS_MAX - (22.5f * SCALE.x), VIEWPORT_POS.y + (15.f * SCALE.y)), 45.f * SCALE.x, IM_COL32(62.f, 98.f, 160.f, 255.f));
    }
    FG_DRAW_LIST->PopClipRect();

    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (notice_info_state && (notice_info_count_new > 0)) {
            clean_notice_info_new(emuenv.io.user_id);
            save_notice_list(emuenv);
        }
        notice_info_state = !notice_info_state;
    }

    if (notice_info_state) {
        const auto SELECT_SIZE = ImVec2(776.f * SCALE.x, 90.f * SCALE.y);
        const auto BORDER_SIZE = notice_info.empty() ? 0.f : 8.f * SCALE.x;
        const auto HALF_BORDER_SIZE = BORDER_SIZE / 2.f;
        const auto VISIBLE_COUNT = std::min(notice_info.size(), size_t(5));
        const auto INDICATOR_SIZE = notice_info.empty() ? ImVec2(412.f * SCALE.x, 86.f * SCALE.y) : ImVec2(SELECT_SIZE.x, (VISIBLE_COUNT * SELECT_SIZE.y) + BORDER_SIZE + ((VISIBLE_COUNT - 1) * 1.f));
        const auto INDICATOR_POS = ImVec2(VIEWPORT_POS.x + (notice_info.empty() ? VIEWPORT_SIZE.x - (502.f * SCALE.x) : (VIEWPORT_SIZE.x / 2.f) - (INDICATOR_SIZE.x / 2.f)), VIEWPORT_POS.y + (57.f * SCALE.y) + HALF_BORDER_SIZE);
        const ImVec4 INDICATOR_BORDER_COLOR(0.93f, 0.93f, 0.945f, 1.f);
        const auto INDICATOR_BG_COLOR = notice_info.empty() ? INDICATOR_BORDER_COLOR : GUI_SMOOTH_GRAY;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, INDICATOR_BG_COLOR);
        ImGui::PushStyleColor(ImGuiCol_Border, INDICATOR_BORDER_COLOR);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, BORDER_SIZE);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(HALF_BORDER_SIZE, HALF_BORDER_SIZE));
        ImGui::SetNextWindowPos(INDICATOR_POS, ImGuiCond_Always);
        ImGui::BeginChild("##notice_info_child", INDICATOR_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoSavedSettings);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);
        auto &lang = gui.lang.indicator;
        const auto set_download_state = [&](const std::string &group, bool pause) {
            {
                std::lock_guard lock(download_mutex);
                const auto it = download_tasks.find(group);
                if (it == download_tasks.end()) {
                    LOG_WARN("Download task not found for group '{}'.", group);
                    return false;
                }
                auto &task = *it->second;
                auto &progress = *task.download_progress;
                if (pause && progress.finished.load() && !progress.is_paused.load())
                    return false;
                if (!pause && task.thread.joinable())
                    return false;
                progress.is_paused = pause;
                progress.is_waiting = !pause;
                if (pause) {
                    std::lock_guard state_lock(progress.state.mutex);
                    progress.state.canceled = true;
                } else {
                    progress.finished = false;
                }
            }
            const auto update_notice_type = [&](auto &notices) {
                for (auto &entry : notices) {
                    if (entry.group == group && entry.type.find("downloading") != std::string::npos)
                        entry.type = pause ? "downloading_paused" : "downloading";
                }
            };
            update_notice_type(notice_list["global"]);
            update_notice_type(notice_info);
            save_notice_list(emuenv);
            download_cv.notify_all();
            return true;
        };

        const ImVec2 tip_top(INDICATOR_POS.x + INDICATOR_SIZE.x, INDICATOR_POS.y + (22.f * SCALE.y));
        const ImVec2 tip_right(tip_top.x + (30.f * SCALE.x), INDICATOR_POS.y + (3.f * SCALE.y));
        const ImVec2 tip_bottom(tip_top.x, INDICATOR_POS.y + (46.f * SCALE.y));
        draw_list->AddTriangleFilled(tip_top, tip_right, tip_bottom, ImGui::ColorConvertFloat4ToU32(INDICATOR_BORDER_COLOR));

        if (notice_info.empty()) {
            ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
            const auto no_notif = lang["no_notif"].c_str();
            const auto calc_text = ImGui::CalcTextSize(no_notif);
            ImGui::SetCursorPos(ImVec2((INDICATOR_SIZE.x / 2.f) - (calc_text.x / 2.f), (INDICATOR_SIZE.y / 2.f) - (calc_text.y / 2.f)));
            ImGui::TextColored(ImVec4(0.f, 0.f, 0.f, 1.f), "%s", no_notif);
        } else {
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 82.f * SCALE.x);
            const auto ICON_SIZE = ImVec2(64.f * SCALE.x, 64.f * SCALE.y);

            for (const auto &notice : notice_info) {
                if (notice.time != notice_info.front().time)
                    ImGui::Separator();
                const auto BEGIN_POS = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(BEGIN_POS.x + (ImGui::GetColumnWidth() / 2.f) - (ICON_SIZE.x / 2.f), BEGIN_POS.y + (SELECT_SIZE.y / 2.f) - (ICON_SIZE.y / 2.f)));
                const ImGui_Texture *icon = nullptr;
                if (notice.type == "activity" || notice.type == "friend") {
                    icon = get_app_icon(gui, "emu:app/NPXS10006");
                } else if (const auto it = gui.notice_info_icon.find(notice.time); it != gui.notice_info_icon.end()) {
                    icon = &it->second;
                }
                if (icon && *icon) {
                    ImGui::Image(*icon, ICON_SIZE);
                } else {
                    const auto pos = ImGui::GetCursorScreenPos();
                    ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + ICON_SIZE.x, pos.y + ICON_SIZE.y), IM_COL32_WHITE);
                }
                ImGui::SetCursorPosY(BEGIN_POS.y);
                ImGui::PushID(std::to_string(notice.time).c_str());
                const auto SELECT_COLOR = ImVec4(0.23f, 0.68f, 0.95f, 0.60f);
                const auto SELECT_COLOR_HOVERED = ImVec4(0.23f, 0.68f, 0.99f, 0.80f);
                const auto SELECT_COLOR_ACTIVE = ImVec4(0.23f, 0.68f, 1.f, 1.f);
                ImGui::PushStyleColor(ImGuiCol_Header, SELECT_COLOR);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, SELECT_COLOR_HOVERED);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, SELECT_COLOR_ACTIVE);
                if (ImGui::Selectable("##icon", notice.is_new, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, SELECT_SIZE)) {
                    clean_notice_info_new(emuenv.io.user_id);
                    save_notice_list(emuenv);
                    if (notice.type == "activity") {
                        pre_load_app(gui, emuenv, false, "emu:app/NPXS10006");
                        open_friend_activity(gui, emuenv, notice.activity.created_at, notice.activity.online_id);
                    } else if (notice.type == "friend") {
                        pre_load_app(gui, emuenv, false, "emu:app/NPXS10006");
                        if (notice.group == "request_received")
                            open_friend_requests(gui, emuenv);
                        else
                            open_friend_profile(gui, emuenv, notice.friend_info.online_id);
                    } else if (notice.type == "wait_install") {
                        if (gui.vita_area.home_screen) {
                            if (const auto app = get_app_index(gui, emuenv, notice.id)) {
                                const auto live_area_app_index = get_live_area_current_open_apps_list_index(gui, app->path);
                                if (live_area_app_index == gui.live_area_current_open_apps_list.end())
                                    open_live_area(gui, emuenv, app->path);
                                else
                                    gui.live_area_app_current_open = static_cast<int32_t>(std::distance(gui.live_area_current_open_apps_list.begin(), live_area_app_index));
                                gui.gate_animation.start(GateAnimationState::EnterApp);
                            } else {
                                LOG_ERROR("Cannot open LiveArea for update: app index not found for '{}'.", notice.id);
                            }
                        }
                    } else if (notice.type == "content") {
                        if (notice.group == "theme")
                            pre_load_app(gui, emuenv, false, "NPXS10015");
                        else
                            select_app(gui, emuenv, notice.id);
                    } else if (notice.type == "trophy") {
                        pre_load_app(gui, emuenv, false, "NPXS10008");
                        open_trophy_unlocked(gui, emuenv, notice.id, notice.trophy.id);
                    }
                    if (notice.type != "failed_install")
                        notice_info_state = false;
                }
                ImGui::PopStyleColor(3);
                ImGui::NextColumn();
                ImGui::SetWindowFontScale(1.34f * RES_SCALE.y);
                const bool downloading = notice.type.find("downloading") != std::string::npos;
                if (!notice.name.empty()) {
                    ImGui::SetCursorPosY(BEGIN_POS.y + (downloading ? 16.f : 20.f) * SCALE.y);
                    if (notice.type == "trophy") {
                        const auto trophy_grade = static_cast<np::trophy::SceNpTrophyGrade>(string_utils::stoi_def(notice.group, 0, "trophy group"));
                        const auto trophy_kind_icon_name = get_trophy_grade_icon_name(trophy_grade);
                        const auto trophy_kind_icon = trophy_kind_icon_name ? gui.vita_icons.find(trophy_kind_icon_name) : gui.vita_icons.end();
                        if (trophy_kind_icon != gui.vita_icons.end()) {
                            const auto TROPHY_TITLE_ICON_SIZE = 30.f * SCALE.x;
                            const auto TROPHY_TITLE_ICON_SPACING = 3.f * SCALE.x;
                            const auto TROPHY_TITLE_ICON_RIGHT_OFFSET = 4.f * SCALE.x;
                            const auto TROPHY_TITLE_ICON_Y_OFFSET = 2.f * SCALE.y;
                            const auto TROPHY_TITLE_ICON_RIGHT_ALPHA_COMP = 10.f * SCALE.x;
                            const auto title_cursor_pos = ImGui::GetCursorPos();
                            const auto title_text_x = title_cursor_pos.x + TROPHY_TITLE_ICON_SIZE - TROPHY_TITLE_ICON_RIGHT_ALPHA_COMP + TROPHY_TITLE_ICON_SPACING;
                            const auto title_icon_y = title_cursor_pos.y + (((ImGui::GetTextLineHeight() - TROPHY_TITLE_ICON_SIZE) > 0.f) ? ((ImGui::GetTextLineHeight() - TROPHY_TITLE_ICON_SIZE) * 0.5f) : 0.f) - TROPHY_TITLE_ICON_Y_OFFSET;
                            ImGui::SetCursorPos(ImVec2(title_cursor_pos.x - TROPHY_TITLE_ICON_RIGHT_OFFSET, title_icon_y));
                            ImGui::Image(trophy_kind_icon->second, ImVec2(TROPHY_TITLE_ICON_SIZE, TROPHY_TITLE_ICON_SIZE));
                            ImGui::SetCursorPos(ImVec2(title_text_x, title_cursor_pos.y));
                            ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice.name.c_str());
                        } else {
                            const auto trophy_kind_s = get_trophy_grade_label(gui, trophy_grade);
                            ImGui::TextColored(GUI_COLOR_TEXT, "(%s) %s", trophy_kind_s.c_str(), notice.name.c_str());
                        }
                    } else {
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice.name.c_str());
                    }
                }
                ImGui::SetWindowFontScale(0.94f * RES_SCALE.y);
                if (downloading) {
                    if (const auto task = find_download_task(notice.group)) {
                        auto &progress = *task->download_progress;
                        ImGui::SetCursorPosY(BEGIN_POS.y + (45.f * SCALE.y));
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
                        ImGui::ProgressBar(progress.progress.load(), ImVec2(602.f * SCALE.x, 12.f * SCALE.y), "");
                        ImGui::PopStyleColor();
                        ImGui::SetCursorPosY(BEGIN_POS.y + (60.f * SCALE.y));
                        const std::string status = progress.is_paused.load() ? lang["paused"] : progress.is_waiting.load() ? lang["waiting_download"]
                                                                                                                           : fmt::format("{} ({} / {})", fmt::format(fmt::runtime(progress.remaining.load() > 120 ? lang["minutes_left"] : lang["seconds_left"]), progress.remaining.load() > 120 ? progress.remaining.load() / 60 : progress.remaining.load()), get_unit_size(progress.downloaded.load()), get_unit_size(task->total_size));
                        ImGui::TextColored(GUI_COLOR_TEXT, "%s", status.c_str());
                    }
                } else {
                    if (!notice.name.empty())
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (14.f * SCALE.y));
                    else
                        ImGui::SetCursorPosY(BEGIN_POS.y + (SELECT_SIZE.y / 2.f) - (ImGui::CalcTextSize(notice.msg.c_str()).y / 2.f));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice.msg.c_str());
                }
                if (downloading || notice.type == "download_failed" || notice.type == "wait_install") {
                    ImGui::SetWindowFontScale(0.94f * RES_SCALE.y);
                    const ImVec2 menu_button_size(40.f * SCALE.x, 40.f * SCALE.y);
                    ImGui::SetCursorPos(ImVec2(BEGIN_POS.x + SELECT_SIZE.x - menu_button_size.x - 35.f * SCALE.x, BEGIN_POS.y + 14.f * SCALE.y));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, menu_button_size.x);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.902f, 0.902f, 0.902f, 1.f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.502f, 0.502f, 0.502f, 1.f));
                    const auto button_pos = ImGui::GetCursorScreenPos();
                    if (ImGui::Button("...##menu_btn", menu_button_size))
                        ImGui::OpenPopup(fmt::format("download_menu_{}", notice.group).c_str());
                    ImGui::PopStyleColor(2);
                    ImGui::PopStyleVar();

                    const ImVec2 action_size(200.f * SCALE.x, 60.f * SCALE.y);
                    const bool has_resume = downloading || notice.type == "download_failed";
                    ImGui::SetNextWindowSize(ImVec2(action_size.x, action_size.y * (has_resume ? 2.f : 1.f) + (has_resume ? 1.f : 0.f)), ImGuiCond_Always);
                    ImGui::SetNextWindowPos(ImVec2(button_pos.x - action_size.x - 45.f * SCALE.x, button_pos.y + menu_button_size.y / 2.f + 6.f * SCALE.y), ImGuiCond_Always);
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.f * SCALE.x);
                    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.f, 0.f, 0.f, 0.f));
                    if (ImGui::BeginPopupModal(fmt::format("download_menu_{}", notice.group).c_str(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
                        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.23f, 0.68f, 1.f, 0.6f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.23f, 0.68f, 1.f, 0.8f));
                        if (notice.type == "download_failed") {
                            if (ImGui::Button(lang["resume"].c_str(), action_size)) {
                                queue_notice_info("retry", notice.id);
                                ImGui::CloseCurrentPopup();
                            }
                        } else if (downloading) {
                            if (const auto task = find_download_task(notice.group)) {
                                auto &progress = *task->download_progress;
                                const bool paused = progress.is_paused.load();
                                bool stopping = false;
                                {
                                    std::lock_guard lock(download_mutex);
                                    stopping = paused && task->thread.joinable();
                                }
                                ImGui::BeginDisabled(stopping);
                                if (ImGui::Button(paused ? lang["resume"].c_str() : lang["pause"].c_str(), action_size)) {
                                    if (set_download_state(notice.group, !paused))
                                        ImGui::CloseCurrentPopup();
                                }
                                ImGui::EndDisabled();
                            }
                        }
                        if (has_resume)
                            ImGui::Separator();
                        if (ImGui::Button(::lang::get(::lang::str::cancel).c_str(), action_size)) {
                            if (const auto task = find_download_task(notice.group)) {
                                auto &state = task->download_progress->state;
                                std::lock_guard lock(state.mutex);
                                if (!state.canceled)
                                    state.pause = true;
                            }
                            ImGui::OpenPopup("cancel_download_popup");
                        }
                        ImGui::PopStyleColor(3);
                        ImGui::PopStyleVar(3);

                        const auto POPUP_SIZE = ImVec2(760.0f * SCALE.x, 436.0f * SCALE.y);
                        bool cancel_confirmed = false;
                        ImGui::SetNextWindowSize(POPUP_SIZE, ImGuiCond_Always);
                        ImGui::SetNextWindowPos(ImVec2(WINDOW_POS.x + (VIEWPORT_SIZE.x / 2.f) - (POPUP_SIZE.x / 2.f), WINDOW_POS.y + (VIEWPORT_SIZE.y / 2.f) - (POPUP_SIZE.y / 2.f)), ImGuiCond_Always);
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f * SCALE.x);
                        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 16.f * SCALE.x);
                        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 8.f * SCALE.x);
                        if (ImGui::BeginPopupModal("cancel_download_popup", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration)) {
                            ImGui::SetWindowFontScale(1.34f * RES_SCALE.y);
                            const auto LARGE_BUTTON_SIZE = ImVec2(310.f * SCALE.x, 46.f * SCALE.y);
                            const std::string &msg = notice.type.find("downloading") != std::string::npos ? lang["cancel_download"] : lang["cancel_install"];
                            const auto str_size = ImGui::CalcTextSize(msg.c_str(), 0, false, POPUP_SIZE.x - (120.f * SCALE.y));
                            ImGui::PushTextWrapPos(POPUP_SIZE.x - (120.f * SCALE.x));
                            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - (str_size.x / 2.f), (POPUP_SIZE.y / 2.f) - (str_size.y / 2.f) - (LARGE_BUTTON_SIZE.y / 2.f) - (22.0f * SCALE.y)));
                            ImGui::Text("%s", msg.c_str());
                            ImGui::PopTextWrapPos();
                            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - LARGE_BUTTON_SIZE.x - (20.f * SCALE.x), POPUP_SIZE.y - LARGE_BUTTON_SIZE.y - (22.0f * SCALE.y)));
                            ImGui::SetWindowFontScale(1.54f * RES_SCALE.y);
                            if (ImGui::Button(::lang::get(::lang::str::no).c_str(), LARGE_BUTTON_SIZE)) {
                                if (const auto task = find_download_task(notice.group)) {
                                    auto &state = task->download_progress->state;
                                    std::lock_guard lock(state.mutex);
                                    if (state.pause) {
                                        state.pause = false;
                                        state.cv.notify_one();
                                    }
                                }
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::SameLine(0, 40.f * SCALE.x);
                            if (ImGui::Button(::lang::get(::lang::str::yes).c_str(), LARGE_BUTTON_SIZE)) {
                                queue_remove_notice(notice.time);
                                cancel_confirmed = true;
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
                        }
                        const auto BORDER_TRANSLUCIDE = IM_COL32(255, 255, 255, 80);
                        const ImVec2 p0(ImGui::GetWindowPos().x - HALF_BORDER_SIZE, ImGui::GetWindowPos().y - HALF_BORDER_SIZE);
                        const ImVec2 p1(p0.x + ImGui::GetWindowSize().x + BORDER_SIZE, p0.y + ImGui::GetWindowSize().y + BORDER_SIZE);
                        const auto popup_draw_list = ImGui::GetWindowDrawList();
                        popup_draw_list->PushClipRect(VIEWPORT_POS, ImVec2(VIEWPORT_WIDTH_POS_MAX, VIEWPORT_POS.y + VIEWPORT_SIZE.y), false);
                        popup_draw_list->AddRect(p0, p1, BORDER_TRANSLUCIDE, 16.f * SCALE.x, 0, BORDER_SIZE);
                        const ImVec2 tip_top(p1.x + HALF_BORDER_SIZE, p0.y + (28.f * SCALE.y) - HALF_BORDER_SIZE);
                        const ImVec2 tip_right(p1.x + HALF_BORDER_SIZE + (20.f * SCALE.x), p0.y + (20.f * SCALE.y) - HALF_BORDER_SIZE);
                        const ImVec2 tip_bottom(p1.x + HALF_BORDER_SIZE, p0.y + (44.f * SCALE.y) - HALF_BORDER_SIZE);
                        popup_draw_list->AddTriangleFilled(tip_top, tip_right, tip_bottom, BORDER_TRANSLUCIDE);
                        popup_draw_list->PopClipRect();
                        ImGui::PopStyleVar(3);
                        if (cancel_confirmed)
                            ImGui::CloseCurrentPopup();
                        if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                            ImGui::CloseCurrentPopup();
                        ImGui::EndPopup();
                    }
                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar();
                }
                ImGui::SetWindowFontScale(0.94f * RES_SCALE.x);
                const auto notice_time = get_notice_time(gui, emuenv, notice.time);
                const auto notice_time_size = ImGui::CalcTextSize(notice_time.c_str());
                ImGui::SetCursorPos(ImVec2(INDICATOR_SIZE.x - (34.f * SCALE.x) - notice_time_size.x, BEGIN_POS.y + (66.f * SCALE.y)));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice_time.c_str());
                ImGui::ScrollWhenDragging();
                ImGui::PopID();
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
        }
        ImGui::EndChild();
        ImGui::PopStyleVar(3);

        if (!notice_info.empty()) {
            const auto DELETE_POPUP_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);
            const auto BUTTON_SIZE = ImVec2(320.f * SCALE.x, 46.f * SCALE.y);

            const auto MENU_BUTTON_SIZE = ImVec2(246.f * SCALE.x, 60.f * SCALE.y);
            const auto has_download_state = [&](const std::string &type) {
                return std::any_of(notice_info.begin(), notice_info.end(), [&](const auto &notice) {
                    return notice.type == type;
                });
            };
            const auto has_downloading = has_download_state("downloading");
            const auto has_download_paused = has_download_state("downloading_paused");
            const auto BUTTON_COUNT = static_cast<int>(has_downloading) + static_cast<int>(has_download_paused) + 1;
            const auto POPUP_HEIGHT = MENU_BUTTON_SIZE.y * BUTTON_COUNT + ((BUTTON_COUNT - 1) * 1.f);
            const auto button_size = ImVec2(64.f * SCALE.x, 40.f * SCALE.y);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, button_size.x);
            ImGui::SetCursorPos(ImVec2(VIEWPORT_SIZE.x - (70.f * SCALE.x), VIEWPORT_SIZE.y - (52.f * SCALE.y)));
            const auto button_pos = ImGui::GetCursorScreenPos();
            ImGui::SetWindowFontScale(1.4f * RES_SCALE.y);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.502f, 0.502f, 0.502f, 1.f));
            draw_list->AddCircleFilled(ImVec2(button_pos.x + (button_size.x / 2.f), button_pos.y + (button_size.y / 2.f)), button_size.x / 2.f, IM_COL32(234, 234, 234, 255));
            if (ImGui::Button("...##ellipsis_menu", button_size) || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_triangle))
                ImGui::OpenPopup("ellipsis_menu");
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
            ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.f, 0.f, 0.f, 0.f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.f * SCALE.x);
            ImGui::SetNextWindowSize(ImVec2(MENU_BUTTON_SIZE.x, POPUP_HEIGHT), ImGuiCond_Always);
            ImGui::SetNextWindowPos(ImVec2(button_pos.x - MENU_BUTTON_SIZE.x - (45.f * SCALE.x), button_pos.y - POPUP_HEIGHT - (6.f * SCALE.y)), ImGuiCond_Always);
            if (ImGui::BeginPopupModal("ellipsis_menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.23f, 0.68f, 1.f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.23f, 0.68f, 1.f, 0.8f));
                if (has_download_paused) {
                    bool stopping = false;
                    {
                        std::lock_guard lock(download_mutex);
                        stopping = std::any_of(download_tasks.begin(), download_tasks.end(), [](const auto &entry) {
                            return entry.second->download_progress->is_paused.load() && entry.second->thread.joinable();
                        });
                    }
                    ImGui::BeginDisabled(stopping);
                    if (ImGui::Button(lang["resume_all"].c_str(), MENU_BUTTON_SIZE)) {
                        for (const auto &notice : notice_info) {
                            if (notice.type == "downloading_paused")
                                set_download_state(notice.group, false);
                        }
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndDisabled();
                    ImGui::Separator();
                }
                if (has_downloading) {
                    if (ImGui::Button(lang["pause_all"].c_str(), MENU_BUTTON_SIZE)) {
                        for (const auto &notice : notice_info) {
                            if (notice.type == "downloading")
                                set_download_state(notice.group, true);
                        }
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::Separator();
                }
                if (ImGui::Button(lang["delete_all"].c_str(), MENU_BUTTON_SIZE))
                    ImGui::OpenPopup("Delete All");
                ImGui::SetNextWindowSize(DELETE_POPUP_SIZE, ImGuiCond_Always);
                ImGui::SetNextWindowPos(ImVec2(VIEWPORT_POS.x + (VIEWPORT_SIZE.x / 2.f) - (DELETE_POPUP_SIZE.x / 2.f), VIEWPORT_POS.y + (VIEWPORT_SIZE.y / 2.f) - (DELETE_POPUP_SIZE.y / 2.f)));
                if (ImGui::BeginPopupModal("Delete All", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
                    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
                    ImGui::SetCursorPosY((DELETE_POPUP_SIZE.y / 2.f) - (46.f * SCALE.y));
                    TextColoredCentered(GUI_COLOR_TEXT, lang["notif_deleted"].c_str());
                    ImGui::SetCursorPos(ImVec2((DELETE_POPUP_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * SCALE.x)), DELETE_POPUP_SIZE.y - BUTTON_SIZE.y - (24.0f * SCALE.y)));
                    if (ImGui::Button(lang::get(lang::str::cancel).c_str(), BUTTON_SIZE) || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_circle)) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine(0, 20.f * SCALE.x);
                    if (ImGui::Button(::lang::get(::lang::str::ok).c_str(), BUTTON_SIZE) || ImGui_ImplSdl_IsPhysicalKeyPressed(emuenv.cfg.keyboard_button_cross)) {
                        std::vector<time_t> notice_times;
                        for (const auto &notice : notice_list["global"])
                            if (notice.type.find("downloading") == std::string::npos && notice.type != "wait_install")
                                notice_times.push_back(notice.time);
                        for (const auto &notice : notice_list[emuenv.io.user_id])
                            if (notice.type.find("downloading") == std::string::npos && notice.type != "wait_install")
                                notice_times.push_back(notice.time);
                        for (const auto time : notice_times)
                            remove_notice(gui, emuenv, time);
                        save_notice_list(emuenv);
                        if (notice_info.empty())
                            notice_info_state = false;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                } else {
                    const auto border_color = IM_COL32(255, 255, 255, 80);
                    const ImVec2 p0(ImGui::GetWindowPos().x - HALF_BORDER_SIZE, ImGui::GetWindowPos().y - HALF_BORDER_SIZE);
                    const ImVec2 p1(p0.x + ImGui::GetWindowSize().x + BORDER_SIZE, p0.y + ImGui::GetWindowSize().y + BORDER_SIZE);
                    const auto popup_draw_list = ImGui::GetWindowDrawList();
                    popup_draw_list->PushClipRect(VIEWPORT_POS, ImVec2(VIEWPORT_WIDTH_POS_MAX, VIEWPORT_POS.y + VIEWPORT_SIZE.y), false);
                    popup_draw_list->AddRect(p0, p1, border_color, 16.f * SCALE.x, 0, BORDER_SIZE);

                    const ImVec2 tip_top(p1.x + HALF_BORDER_SIZE, p1.y - (28.f * SCALE.y) - HALF_BORDER_SIZE);
                    const ImVec2 tip_right(tip_top.x + (20.f * SCALE.x), p1.y - (20.f * SCALE.y) - HALF_BORDER_SIZE);
                    const ImVec2 tip_bottom(tip_top.x, p1.y - (44.f * SCALE.y) - HALF_BORDER_SIZE);
                    popup_draw_list->AddTriangleFilled(tip_top, tip_right, tip_bottom, border_color);
                    popup_draw_list->PopClipRect();
                }
                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar(3);

                if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    ImGui::CloseCurrentPopup();

                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.f);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);

    const auto display_size = ImGui::GetIO().DisplaySize;
    draw_list->AddRectFilled(ImVec2(0.f, 0.f), ImVec2(display_size.x, VIEWPORT_POS.y), IM_COL32(0.f, 0.f, 0.f, 255.f));
    draw_list->AddRectFilled(ImVec2(VIEWPORT_WIDTH_POS_MAX, VIEWPORT_POS.y), ImVec2(display_size.x, VIEWPORT_POS.y + display_size.y), IM_COL32(0.f, 0.f, 0.f, 255.f));
}

} // namespace gui
