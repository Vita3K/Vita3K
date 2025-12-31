// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <gui/functions.h>

#include <config/state.h>
#include <dialog/state.h>
#include <display/state.h>
#include <io/state.h>

#include <packages/license.h>
#include <packages/pkg.h>
#include <packages/sfo.h>

#include <util/safe_time.h>

#include <util/lock_and_find.h>
#include <util/net_utils.h>
#include <util/string_utils.h>

#include <io/VitaIoDevice.h>
#include <util/log.h>

#include <pugixml.hpp>

#include <SDL3/SDL_power.h>
#include <stb_image.h>

#include <rif2zrif.h>

namespace gui {

struct NoticeList {
    std::string id;
    std::string content_id;
    std::string group;
    std::string type;
    time_t time = 0;
    bool is_new = false;
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

static std::mutex notice_mutex;
static std::vector<NoticeTask> notice_tasks;

static std::map<std::string, std::vector<NoticeList>> notice_list;
static std::map<std::string, int> notice_list_count_new;
static int notice_info_count_new = 0;
static std::vector<NoticeInfo> notice_info;

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
        const auto notice_info_it = std::find_if(notice_info.begin(), notice_info.end(), [&](const NoticeInfo &n) {
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
    int32_t width = 0;
    int32_t height = 0;
    vfs::FileBuffer buffer;

    if (!vfs::read_file(VitaIoDevice::ux0, buffer, emuenv.pref_path, content_path)) {
        if (info.type == "trophy") {
            LOG_WARN("Icon no found for trophy id: {} on NpComId: {}", info.content_id, info.id);
            return false;
        } else {
            if (!vfs::read_app_file(buffer, emuenv.pref_path, info.id, "sce_sys/icon0.png")) {
                buffer = init_default_icon(gui, emuenv);
                if (buffer.empty()) {
                    LOG_WARN("Not found default icon for this notice content: {}", info.content_id);
                    return false;
                }
            }
        }
    }
    stbi_uc *data = stbi_load_from_memory(&buffer[0], static_cast<int>(buffer.size()), &width, &height, nullptr, STBI_rgb_alpha);
    if (!data) {
        LOG_ERROR("Invalid icon for notice id: {} in path {}.", info.id, content_path);
        return false;
    }
    gui.notice_info_icon[info.time] = ImGui_Texture(gui.imgui_state.get(), data, width, height);
    stbi_image_free(data);

    return gui.notice_info_icon.contains(info.time);
}

struct DownloadProgress {
    std::atomic<float> progress{ 0 };
    std::atomic<uint64_t> remaining{ 0 };
    std::atomic<double> downloaded{ 0 };
    std::atomic<bool> is_paused{ false };
    std::atomic<bool> is_waiting{ false };
    net_utils::ProgressState state{};
};

struct DownloadTask {
    std::thread thread;
    uint64_t total_size = 0;
    std::string url;
    std::unique_ptr<DownloadProgress> download_progress;
};

static std::mutex download_mutex;
static std::condition_variable download_cv;
static std::map<std::string, std::shared_ptr<DownloadTask>> download_threads;

static void download_update(GuiState &gui, EmuEnvState &emuenv, const std::string &id, const std::string &group) {
    DownloadTask *task = nullptr;
    {
        std::lock_guard<std::mutex> lock(download_mutex);
        auto it = download_threads.find(group);
        if (it == download_threads.end()) {
            LOG_ERROR("DownloadTask not found for group '{}'", group);
            return;
        }
        task = it->second.get();
    }

    if (task->url.empty()) {
        LOG_ERROR("No URL provided for update download.");
        return;
    }
    LOG_INFO("Downloading update from: {}", task->url);

    net_utils::ProgressCallback progress_callback = [group](float p, uint64_t r, double d) -> net_utils::ProgressState * {
        std::lock_guard<std::mutex> lock(download_mutex);
        auto it = download_threads.find(group);
        if (it == download_threads.end())
            return nullptr;

        it->second->download_progress->progress = p;
        it->second->download_progress->remaining = r;
        it->second->download_progress->downloaded = d;
        return &it->second->download_progress->state;
    };

    const auto update_path = (emuenv.pref_path / "ux0/bgdl/t" / group / fs::path(task->url).filename()).string();
    fs::create_directories(fs::path(update_path).parent_path());
    bool success = net_utils::download_file(task->url, update_path, progress_callback);

    LOG_DEBUG("Download finished for group: {}, success: {}", group, success);

    if (task->download_progress->state.download && !success) {
        LOG_ERROR("Failed to download update from: {}, to: {}, downloaded: {}/{}", task->url, update_path, task->download_progress->downloaded.load(), task->total_size);
    } else {
        LOG_INFO("Update downloaded successfully: {}, {}/{}", task->url, task->download_progress->downloaded.load(), task->total_size);
    }

    if (task->download_progress->state.download) {
        std::lock_guard<std::mutex> lock(task->download_progress->state.mutex);
        task->download_progress->state.download = false;
    } else if (!success)
        return; // Download was cancelled, do not proceed

    {
        std::lock_guard<std::mutex> lock(notice_mutex);
        const auto result = success ? "wait_install" : "download_failed";
        update_notice_info(gui, emuenv, result, id);
    }
}

static void destroy_download_threads() {
    for (auto &[group, task] : download_threads) {
        auto &progess_state = task->download_progress->state;
        {
            std::unique_lock<std::mutex> lock(progess_state.mutex);
            progess_state.download = false;
            progess_state.pause = false;
            progess_state.cv.notify_all();
        }
        if (task->thread.joinable()) {
            task->thread.join();
        }
    }
    download_threads.clear();
}

std::atomic<bool> download_manager_running{ false };
std::thread download_manager_thread;
std::mutex download_manager_mutex;

static void download_manager() {
    while (download_manager_running.load()) {
        // Wait for a notification to start processing downloads
        if (download_threads.empty()) {
            LOG_DEBUG("No active downloads, waiting for new tasks.");
        }
        std::unique_lock<std::mutex> lock(download_manager_mutex);
        download_cv.wait(lock, [] {
            return !download_manager_running.load() || !download_threads.empty();
        });
        lock.unlock();

        if (!download_manager_running.load()) {
            LOG_DEBUG("Download manager is stopping.");
            break; // Exit if the manager is no longer running
        }

        {
            std::lock_guard<std::mutex> lock(download_manager_mutex);
            for (auto it = download_threads.begin(); it != download_threads.end();) {
                if (!it->second->thread.joinable()) {
                    it = download_threads.erase(it);
                    LOG_DEBUG("Removed completed download thread for group: {}", it->first);
                } else if (!it->second->download_progress->state.download && it->second->thread.joinable()) {
                    LOG_DEBUG("Joining download thread for group: {}", it->first);
                    it->second->thread.join();
                    it = download_threads.erase(it);
                } else {
                    ++it;
                }
            }
        }

        const auto has_active_download = std::any_of(download_threads.begin(), download_threads.end(), [](const auto &pair) {
            auto &dp = pair.second->download_progress;
            return !dp->is_waiting.load() && !dp->is_paused.load();
        });

        if (has_active_download)
            continue; // Skip the rest of the loop if there are active downloads
        else {
            std::lock_guard<std::mutex> lock(download_manager_mutex);
            auto it = std::find_if(download_threads.begin(), download_threads.end(),
                [](const auto &pair) {
                    auto &dp = pair.second->download_progress;
                    return dp->is_waiting.load();
                });

            if (it != download_threads.end()) {
                auto &dp = it->second->download_progress;
                dp->is_waiting = false;

                {
                    std::unique_lock<std::mutex> lock(dp->state.mutex);
                    dp->state.pause = false;
                    dp->state.cv.notify_all();
                }

                LOG_DEBUG("Resuming download thread for group: {}", it->first);
            }
        }
    }

    destroy_download_threads();
}

void destroy_download_manager() {
    {
        std::lock_guard<std::mutex> lock(download_manager_mutex);
        if (download_manager_running) {
            download_manager_running = false;
            download_cv.notify_all();
        }
    }

    if (download_manager_thread.joinable())
        download_manager_thread.join();
}

static bool set_notice_icon(GuiState &gui, EmuEnvState &emuenv, const NoticeList &info) {
    fs::path content_path;

    if (info.type == "trophy") {
        content_path = fs::path("user") / emuenv.io.user_id / "trophy/conf" / info.id / fmt::format("TROP{}.PNG", info.content_id);
    } else {
        if (info.type == "content") {
            if (info.group.find("gd") != std::string::npos) {
                content_path = fs::path("app") / info.id;
            } else {
                if (info.group == "ac")
                    content_path = fs::path("addcont") / info.id / info.content_id;
                else if (info.group.find("gp") != std::string::npos)
                    content_path = fs::path("app") / info.id;
                else if (info.group == "theme")
                    content_path = fs::path("theme") / info.content_id;
            }
        } else
            content_path = fs::path("app") / info.id;

        content_path /= "sce_sys/icon0.png";
    }

    return init_notice_icon(gui, emuenv, content_path, info);
}

static bool set_notice_info(GuiState &gui, EmuEnvState &emuenv, const NoticeList &info) {
    std::string msg, name;

    const std::string user_id = info.type == "trophy" ? emuenv.io.user_id : "global";
    auto &lang = gui.lang.indicator;
    if (info.type == "content") {
        fs::path content_path;
        if (info.type == "content") {
            if (info.group.find("gd") != std::string::npos) {
                content_path = fs::path("app") / info.id;
                msg = lang["app_added_home"];
            } else {
                if (info.group == "ac")
                    content_path = fs::path("addcont") / info.id / info.content_id;
                else if (info.group.find("gp") != std::string::npos)
                    content_path = fs::path("app") / info.id;
                else if (info.group == "theme")
                    content_path = fs::path("theme") / info.content_id;
                msg = lang["install_complete"];
            }
            vfs::FileBuffer params;
            if (vfs::read_file(VitaIoDevice::ux0, params, emuenv.pref_path, content_path / "sce_sys/param.sfo")) {
                SfoFile sfo_handle;
                sfo::load(sfo_handle, params);
                if (!sfo::get_data_by_key(name, sfo_handle, fmt::format("TITLE_{:0>2d}", emuenv.cfg.sys_lang)))
                    sfo::get_data_by_key(name, sfo_handle, "TITLE");
            } else {
                LOG_WARN("Content not found for id: {}, in path: {}", info.content_id, content_path);
                return false;
            }
        }
    } else if (info.type == "download_failed") {
        if (!gui.new_update_infos.contains(info.id))
            get_remote_update_info(gui, emuenv, info.id);

        const auto &update_infos = gui.new_update_infos[info.id];
        name = update_infos.title;
        msg = emuenv.common_dialog.lang.common["download_failed"];
    } else if (info.type.find("downloading") != std::string::npos) {
        if (!gui.new_update_infos.contains(info.id))
            get_remote_update_info(gui, emuenv, info.id);
        const auto update_infos = gui.new_update_infos[info.id];
        name = update_infos.title;
        auto &update_install = gui.updates_install[info.id];
        update_install.state = UpdateState::DOWNLOADING;

        auto task = std::make_shared<DownloadTask>();
        task->url = update_infos.url;
        task->total_size = static_cast<uint64_t>(update_infos.size);
        task->download_progress = std::make_unique<DownloadProgress>();
        task->download_progress->state.pause = true;

        if (info.type == "downloading_paused")
            task->download_progress->is_paused = true;
        else
            task->download_progress->is_waiting = true;

        task->thread = std::thread(download_update, std::ref(gui), std::ref(emuenv), info.id, info.group);

        download_threads.emplace(info.group, std::move(task));
        {
            std::lock_guard<std::mutex> lock(download_manager_mutex);
            download_cv.notify_all();
        }
    } else if (info.type == "wait_install") {
        if (!gui.new_update_infos.contains(info.id))
            get_remote_update_info(gui, emuenv, info.id);
        const auto &update_infos = gui.new_update_infos[info.id];
        name = update_infos.title;
        msg = lang["waiting_install"];

        auto &update_install = gui.updates_install[info.id];
        update_install.content_id = info.content_id;
        update_install.pkg_path = emuenv.pref_path / "ux0/bgdl/t" / info.group / fs::path(update_infos.url).filename();
        update_install.state = UpdateState::WAITING_INSTALL;
    } else if (info.type == "failed_install") {
        if (!gui.new_update_infos.contains(info.id))
            get_remote_update_info(gui, emuenv, info.id);
        const auto &update_infos = gui.new_update_infos[info.id];
        name = update_infos.title;
        msg = lang["install_failed"];
    } else {
        auto &common = gui.lang.common.main;
        switch (static_cast<np::trophy::SceNpTrophyGrade>(string_utils::stoi_def(info.group, 0, "trophy group"))) {
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM:
            name = fmt::format("({}) ", common["platinum"]);
            break;
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD:
            name = fmt::format("({}) ", common["gold"]);
            break;
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER:
            name = fmt::format("({}) ", common["silver"]);
            break;
        case np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE:
            name = fmt::format("({}) ", common["bronze"]);
            break;
        default: break;
        }

        const auto trophy_conf_id_path{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "trophy/conf" / info.id };
        const std::string sfm_name = fs::exists(trophy_conf_id_path / fmt::format("TROP_{:0>2d}.SFM", emuenv.cfg.sys_lang)) ? fmt::format("TROP_{:0>2d}.SFM", emuenv.cfg.sys_lang) : "TROP.SFM";

        pugi::xml_document doc;
        if (doc.load_file((trophy_conf_id_path / sfm_name).c_str())) {
            for (const auto &conf : doc.child("trophyconf")) {
                if (conf.name() == std::string("trophy")) {
                    if (info.content_id == conf.attribute("id").as_string())
                        name += conf.child("name").text().as_string();
                }
            }
        } else {
            LOG_WARN("Trophy sfm in conf no found for NpComId: {}", info.id);
            return false;
        }
        msg = lang["trophy_earned"];
    }

    // remove newline characters from name with regex
    name = std::regex_replace(name, std::regex(R"(\s+)"), " ");
    notice_info.push_back({ info, name, msg });

    std::sort(notice_info.begin(), notice_info.end(), [&](const NoticeInfo &na, const NoticeInfo &nb) {
        return na.time > nb.time;
    });

    notice_tasks.push_back({ NoticeAction::ADD, info.time, user_id });

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

    download_manager_thread = std::thread(download_manager);
    download_manager_running = true;
}

void get_notice_list(EmuEnvState &emuenv) {
    notice_list.clear();
    notice_list_count_new.clear();
    const auto notice_path{ emuenv.pref_path / "ux0/user/notice.xml" };

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
                        noticeList.content_id = notice.attribute("content_id").as_string();
                        noticeList.group = notice.attribute("group").as_string();
                        noticeList.type = notice.attribute("type").as_string();
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
            info_child.append_attribute("content_id") = notice.content_id.c_str();
            info_child.append_attribute("group") = notice.group.c_str();
            info_child.append_attribute("type") = notice.type.c_str();
            info_child.append_attribute("time") = notice.time;
            info_child.append_attribute("new") = notice.is_new;
        }
    }

    const auto notice_path{ emuenv.pref_path / "ux0/user/notice.xml" };
    const auto save_xml = notice_xml.save_file(notice_path.c_str());
    if (!save_xml)
        LOG_ERROR("Fail save xml");
}

void update_notice_info(GuiState &gui, EmuEnvState &emuenv, const std::string &type, const std::string &id) {
    NoticeList info;
    const auto user_id = type == "trophy" ? emuenv.io.user_id : "global";
    if (type == "content") {
        info.id = emuenv.app_info.app_title_id;
        info.content_id = emuenv.app_info.app_content_id;
        info.group = emuenv.app_info.app_category;
        if (emuenv.app_info.app_category == "gp") {
            const auto NOTICE_INDEX = std::find_if(notice_list["global"].begin(), notice_list["global"].end(), [&](const auto &n) {
                return n.type == "wait_install" && n.content_id == info.content_id;
            });
            if (NOTICE_INDEX != notice_list["global"].end()) {
                if (NOTICE_INDEX->is_new) {
                    --notice_list_count_new["global"];
                    --notice_info_count_new;
                }
                notice_tasks.push_back({ NoticeAction::REMOVE_ICON, NOTICE_INDEX->time, "global" });
                notice_list["global"].erase(NOTICE_INDEX);
                std::erase_if(notice_info, [&](const auto &n) {
                    return n.type == "wait_install" && n.content_id == info.content_id;
                });
            }
        }
    } else if (type == "downloading") {
        LOG_DEBUG("Adding new download notice info");
        if (!gui.new_update_infos.contains(id))
            get_remote_update_info(gui, emuenv, id);

        const auto &update_infos = gui.new_update_infos[id];
        info.id = update_infos.titleid;
        info.content_id = update_infos.content_id;
        const auto bgdl_path = emuenv.pref_path / "ux0/bgdl/t";
        for (int i = 0; i <= 99; ++i) {
            const auto &group = fmt::format("{:02}", i);
            const fs::path t = bgdl_path / group;
            if (!fs::exists(t) || fs::is_empty(t)) {
                info.group = group;
                break;
            }
        }
        LOG_DEBUG("New download notice info group: {}", info.group);
    } else if ((type == "wait_install") || (type == "download_failed")) {
        if (!gui.new_update_infos.contains(id))
            get_remote_update_info(gui, emuenv, id);

        const auto &update_infos = gui.new_update_infos[id];

        for (const auto &notice : notice_info) {
            if (notice.type.find("downloading") == std::string::npos)
                continue;

            const auto &pkg_path = emuenv.pref_path / "ux0/bgdl/t" / notice.group / fs::path(update_infos.url).filename();
            if (fs::exists(pkg_path)) {
                info.group = notice.group;
                info.time = notice.time;
                break;
            }
        }

        info.id = update_infos.titleid;
        info.content_id = update_infos.content_id;

        auto is_notice_deleted = [info](const auto &notice) {
            return (notice.type == "downloading") && (notice.group == info.group);
        };

        std::erase_if(notice_info, is_notice_deleted);
        std::erase_if(notice_list["global"], is_notice_deleted);
        notice_tasks.push_back({ NoticeAction::REMOVE_ICON, info.time, "global" });
    } else if (type == "failed_install") {
        if (!gui.new_update_infos.contains(id))
            get_remote_update_info(gui, emuenv, id);

        info.id = id;
        info.content_id = gui.new_update_infos[id].content_id;
        info.group = "gp";

        auto is_notice_deleted = [info](const auto &notice) {
            return (notice.type == "wait_install") && (notice.id == info.id);
        };

        auto it = std::find_if(notice_list["global"].begin(), notice_list["global"].end(), is_notice_deleted);
        if (it != notice_list["global"].end())
            notice_tasks.push_back({ NoticeAction::REMOVE_ICON, it->time, "global" });

        std::erase_if(notice_info, is_notice_deleted);
        std::erase_if(notice_list["global"], is_notice_deleted);
    } else {
        const auto &trophy_data = gui.trophy_unlock_display_requests.back();
        info.id = trophy_data.np_com_id;
        info.content_id = trophy_data.trophy_id;
        info.group = std::to_string(static_cast<int>(trophy_data.trophy_kind));
    }
    info.type = type;
    info.time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    const auto is_new = (type != "downloading") && (type != "content") && (type != "failed_install");
    if (is_new)
        info.is_new = true;
    notice_list[user_id].push_back(info);
    LOG_DEBUG("New notice info added: id: {}, content_id: {}, group: {}, type: {}, time: {}, is new: {}", info.id, info.content_id, info.group, info.type, info.time, info.is_new);

    if (set_notice_info(gui, emuenv, info) && is_new) {
        ++notice_list_count_new[user_id];
        ++notice_info_count_new;
    }

    save_notice_list(emuenv);
}

static void process_notice_tasks(EmuEnvState &emuenv, GuiState &gui) {
    for (const auto &task : notice_tasks) {
        switch (task.action) {
        case NoticeAction::REMOVE_FULL: {
            auto is_notice_deleted = [task](const auto &notice) {
                return notice.time == task.time;
            };

            const auto notice_it = std::find_if(notice_list[task.user_id].begin(), notice_list[task.user_id].end(), is_notice_deleted);
            if (notice_it != notice_list[task.user_id].end()) {
                if (notice_it->is_new) {
                    --notice_list_count_new[task.user_id];
                    --notice_info_count_new;
                }

                if (notice_it->type != "download_failed") {
                    const auto bgdl_path = emuenv.pref_path / "ux0/bgdl/t" / notice_it->group;
                    if (fs::exists(bgdl_path))
                        fs::remove_all(bgdl_path);
                }

                LOG_INFO("Notice content: id: {}, content_id: {}, group: {}, type: {}, time: {} has been removed.", notice_it->id, notice_it->content_id, notice_it->group, notice_it->type, notice_it->time);
                notice_list[task.user_id].erase(notice_it);
            }

            std::erase_if(notice_info, is_notice_deleted);
            gui.notice_info_icon.erase(task.time);
            save_notice_list(emuenv);
            break;
        }
        case NoticeAction::REMOVE_ICON: {
            gui.notice_info_icon.erase(task.time);
            break;
        }
        case NoticeAction::ADD: {
            auto it = std::find_if(notice_list[task.user_id].begin(), notice_list[task.user_id].end(), [&](const auto &n) {
                return n.time == task.time;
            });
            if (it == notice_list[task.user_id].end()) {
                LOG_ERROR("Notice with time {} not found.", task.time);
                continue;
            }
            if (!set_notice_icon(gui, emuenv, *it)) {
                LOG_ERROR("Failed to set notice icon for time: {}, user id: {}", task.time, task.user_id);
                // If setting notice info fails, remove it from the list
                notice_list[task.user_id].erase(it);
                save_notice_list(emuenv);
                continue;
            }

            break;
        }
        }
    }
    notice_tasks.clear();
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

static std::string get_remaining_str(LangState &lang, const uint64_t remaining) {
    if (remaining > 120)
        return fmt::format(fmt::runtime(lang.indicator["minutes_left"]), remaining / 60);
    else
        return fmt::format(fmt::runtime(lang.indicator["seconds_left"]), remaining);
}

static void draw_notice_info(GuiState &gui, EmuEnvState &emuenv) {
    static bool notice_info_state;
    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

    const auto VIEWPORT_WIDTH_POS_MAX(VIEWPORT_POS.x + VIEWPORT_SIZE.x);

    const ImVec2 NOTICE_SIZE = notice_info_count_new ? ImVec2(104.0f * SCALE.x, 95.0f * SCALE.y) : ImVec2(90.0f * SCALE.x, 82.0f * SCALE.y);
    const ImVec2 NOTICE_ICON_POS(VIEWPORT_WIDTH_POS_MAX - NOTICE_SIZE.x, VIEWPORT_POS.y);
    const ImVec2 NOTICE_ICON_POS_MAX(NOTICE_ICON_POS.x + NOTICE_SIZE.x, NOTICE_ICON_POS.y + NOTICE_SIZE.y);

    const auto NOTICE_COLOR = gui.information_bar_color.notice_font;

    const ImVec2 WINDOW_SIZE = notice_info_state ? VIEWPORT_SIZE : (notice_info_count_new ? ImVec2(84.f * SCALE.x, 76.f * SCALE.y) : ImVec2(72.f * SCALE.x, 72.f * SCALE.y));
    const ImVec2 WINDOW_POS = ImVec2(VIEWPORT_POS.x + (notice_info_state ? 0.f : VIEWPORT_SIZE.x - WINDOW_SIZE.x), VIEWPORT_POS.y);

    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 50.f * SCALE.x);
    ImGui::Begin("##notice_info", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    if (!notice_tasks.empty())
        process_notice_tasks(emuenv, gui);

    const auto draw_list = ImGui::GetWindowDrawList();
    if (notice_info_count_new) {
        if (gui.theme_information_bar_notice.contains(NoticeIcon::NEW))
            draw_list->AddImage(gui.theme_information_bar_notice[NoticeIcon::NEW], NOTICE_ICON_POS, NOTICE_ICON_POS_MAX);
        else
            draw_list->AddCircleFilled(ImVec2(VIEWPORT_WIDTH_POS_MAX - (24.f * SCALE.x), VIEWPORT_POS.y + (16.f * SCALE.y)), 60.f * SCALE.x, IM_COL32(11.f, 90.f, 252.f, 255.f));
        const auto FONT_SCALE = 40.f * SCALE.x;
        const auto NOTICE_COUNT_FONT_SCALE = FONT_SCALE / 40.f;
        const auto NOTICE_COUNT_SIZE = ImGui::CalcTextSize(std::to_string(notice_info_count_new).c_str()).x * NOTICE_COUNT_FONT_SCALE;
        draw_list->AddText(gui.vita_font[emuenv.current_font_level], FONT_SCALE, ImVec2(VIEWPORT_WIDTH_POS_MAX - (NOTICE_SIZE.x / 2.f) - (NOTICE_COUNT_SIZE / 2.f) + (12.f * SCALE.x), VIEWPORT_POS.y + (15.f * SCALE.y)), NOTICE_COLOR, std::to_string(notice_info_count_new).c_str());
    } else {
        if (gui.theme_information_bar_notice.contains(NoticeIcon::NO))
            draw_list->AddImage(gui.theme_information_bar_notice[NoticeIcon::NO], NOTICE_ICON_POS, NOTICE_ICON_POS_MAX);
        else
            draw_list->AddCircleFilled(ImVec2(VIEWPORT_WIDTH_POS_MAX - (28.f * SCALE.x), VIEWPORT_POS.y + (18.f * SCALE.y)), 44.f * SCALE.x, IM_COL32(62.f, 98.f, 160.f, 255.f));
    }

    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootWindow) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (notice_info_state && (notice_info_count_new > 0)) {
            clean_notice_info_new(emuenv.io.user_id);
            save_notice_list(emuenv);
        }
        notice_info_state = !notice_info_state;
    }

    if (notice_info_state) {
        const auto SELECT_SIZE = ImVec2(782.f * SCALE.x, 88.f * SCALE.y);
        const auto ITEM_SPACING = ImGui::GetStyle().ItemSpacing;
        const auto POPUP_SIZE = notice_info.empty() ? ImVec2(412.f * SCALE.x, 86.f * SCALE.y) : ImVec2(SELECT_SIZE.x, notice_info.size() < 5 ? (notice_info.size() * (SELECT_SIZE.y + ImGui::GetStyle().ItemSpacing.y) + ((notice_info.size() - 1) * ImGui::GetStyle().ItemSpacing.y)) : 464.f * SCALE.y);
        const auto POPUP_POS = ImVec2(VIEWPORT_POS.x + (notice_info.empty() ? VIEWPORT_SIZE.x - (502.f * SCALE.y) : (VIEWPORT_SIZE.x / 2.f) - (POPUP_SIZE.x / 2.f)), VIEWPORT_POS.y + (56.f * SCALE.y));
        const auto POPUP_BG_COLOR = notice_info.empty() ? GUI_COLOR_TEXT : GUI_SMOOTH_GRAY;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, POPUP_BG_COLOR);
        ImGui::PushStyleColor(ImGuiCol_Border, GUI_COLOR_TEXT);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, notice_info.empty() ? 0.f : 8.0f * SCALE.x);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        ImGui::SetNextWindowPos(POPUP_POS, ImGuiCond_Always);
        ImGui::BeginChild("##notice_info_child", POPUP_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoSavedSettings);

        auto &lang = gui.lang.indicator;
        auto &common = emuenv.common_dialog.lang.common;

        if (notice_info.empty()) {
            ImGui::SetWindowFontScale(1.2f * RES_SCALE.x);
            const auto no_notif = lang["no_notif"].c_str();
            const auto calc_text = ImGui::CalcTextSize(no_notif);
            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - (calc_text.x / 2.f), (POPUP_SIZE.y / 2.f) - (calc_text.y / 2.f)));
            ImGui::TextColored(ImVec4(0.f, 0.f, 0.f, 1.f), "%s", no_notif);
        } else {
            ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnWidth(0, 85.f * SCALE.x);
            const auto ICON_SIZE = ImVec2(64.f * SCALE.x, 64.f * SCALE.y);

            for (const auto &notice : notice_info) {
                if (notice.time != notice_info.front().time)
                    ImGui::Separator();
                const auto ICON_POS = ImGui::GetCursorPos();
                if (gui.notice_info_icon.contains(notice.time)) {
                    ImGui::SetCursorPos(ImVec2(ICON_POS.x + (ImGui::GetColumnWidth() / 2.f) - (ICON_SIZE.x / 2.f), ICON_POS.y + (SELECT_SIZE.y / 2.f) - (ICON_SIZE.y / 2.f)));
                    ImGui::Image(gui.notice_info_icon[notice.time], ICON_SIZE);
                }
                ImGui::SetCursorPosY(ICON_POS.y);
                ImGui::PushID(std::to_string(notice.time).c_str());
                const auto SELECT_COLOR = ImVec4(0.23f, 0.68f, 0.95f, 0.60f);
                const auto SELECT_COLOR_HOVERED = ImVec4(0.23f, 0.68f, 0.99f, 0.80f);
                const auto SELECT_COLOR_ACTIVE = ImVec4(0.23f, 0.68f, 1.f, 1.f);
                ImGui::PushStyleColor(ImGuiCol_Header, SELECT_COLOR);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, SELECT_COLOR_HOVERED);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, SELECT_COLOR_ACTIVE);
                if (ImGui::Selectable("##icon", notice.is_new, (notice.type.find("downloading") != std::string::npos) || notice.type == "wait_install" ? ImGuiSelectableFlags_Disabled : ImGuiSelectableFlags_SpanAllColumns, SELECT_SIZE)) {
                    clean_notice_info_new(emuenv.io.user_id);
                    save_notice_list(emuenv);
                    if (notice.type == "content") {
                        if (notice.group == "theme")
                            pre_run_app(gui, emuenv, "emu:app/NPXS10015");
                        else
                            select_app(gui, notice.id);
                    } else if (notice.type == "wait_install") {
                        const auto current_home_screen_state = gui.vita_area.home_screen;
                        if (current_home_screen_state) {
                            const auto app_path = fmt::format("ux0:app/{}", notice.id);
                            const auto live_area_app_index = gui::get_live_area_current_open_apps_list_index(gui, emuenv.io.app_path);
                            if (live_area_app_index == gui.live_area_current_open_apps_list.end())
                                gui::open_live_area(gui, emuenv, app_path);
                            else
                                gui.live_area_app_current_open = std::distance(gui.live_area_current_open_apps_list.begin(), live_area_app_index);
                            gui.gate_animation.start(GateAnimationState::EnterApp);
                        }
                    } else if (notice.type == "trophy") {
                        pre_load_app(gui, emuenv, false, "emu:app/NPXS10008");
                        open_trophy_unlocked(gui, emuenv, notice.id, notice.content_id);
                    }
                    if (notice.type != "failed_install")
                        notice_info_state = false;
                }
                ImGui::PopStyleColor(3);
                ImGui::NextColumn();
                ImGui::SetWindowFontScale(1.34f * RES_SCALE.y);
                const auto begin_space = ((notice.type.find("downloading") != std::string::npos) ? 18.f : 20.f) * SCALE.y;
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + begin_space);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice.name.c_str());
                ImGui::SetWindowFontScale(0.94f * RES_SCALE.y);
                if ((notice.type != "download_failed") && (notice.type.find("downloading") != std::string::npos)) {
                    auto dt = lock_and_find(notice.group, download_threads, download_mutex);
                    if (dt) {
                        const auto &dp = dt->download_progress;
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ITEM_SPACING.y + (7.f * SCALE.y));
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
                        ImGui::ProgressBar(dp->progress.load() / 100.f, ImVec2(602.f * SCALE.x, 12.f * SCALE.y), "");
                        ImGui::PopStyleColor();
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ITEM_SPACING.y + (4.f * SCALE.y));
                        if (dp->is_paused.load()) {
                            ImGui::Text("%s", lang["paused"].c_str());
                        } else if (dp->is_waiting.load()) {
                            ImGui::Text("%s", lang["waiting_download"].c_str());
                        } else {
                            const auto remaining_str = get_remaining_str(gui.lang, dp->remaining.load());
                            ImGui::Text("%s %s", remaining_str.c_str(), fmt::format("{} / {}", get_unit_size(dp->downloaded.load()), get_unit_size(dt->total_size)).c_str());
                        }
                    }
                } else {
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ITEM_SPACING.y + (14.f * SCALE.y));
                    ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice.msg.c_str());
                }
                if ((notice.type.find("downloading") != std::string::npos) || (notice.type == "wait_install")) {
                    ImGui::SetWindowFontScale(0.94f * RES_SCALE.y);
                    const auto button_size = ImVec2(40.f * SCALE.x, 40.f * SCALE.y);
                    ImGui::SetCursorPos(ImVec2(ICON_POS.x + SELECT_SIZE.x - button_size.x - (35.f * SCALE.x), ICON_POS.y + (14.f * SCALE.y)));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, button_size.x);
                    ImGui::PushStyleColor(ImGuiCol_Button, GUI_COLOR_TEXT);
                    ImGui::PushStyleColor(ImGuiCol_Text, GUI_COLOR_TEXT_BLACK);
                    if (ImGui::Button("...##menu_btn", button_size))
                        ImGui::OpenPopup(fmt::format("download_menu_{}", notice.group).c_str());
                    ImGui::PopStyleColor(2);
                    ImGui::PopStyleVar();
                    if (ImGui::BeginPopup(fmt::format("download_menu_{}", notice.group).c_str())) {
                        if (notice.type == "download_failed") {
                            if (ImGui::MenuItem(lang["resume"].c_str())) {
                                notice_tasks.push_back({ NoticeAction::REMOVE_FULL, notice.time });
                                NoticeList new_notice = notice;
                                new_notice.type = "downloading";
                                new_notice.is_new = false;
                                new_notice.time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                                LOG_DEBUG("Resuming failed download notice: id: {}, content_id: {}, group: {}, type: {}, time: {}, is new: {}", new_notice.id, new_notice.content_id, new_notice.group, new_notice.type, new_notice.time, new_notice.is_new);
                                notice_list["global"].push_back(new_notice);
                                set_notice_info(gui, emuenv, new_notice);
                                save_notice_list(emuenv);
                            }
                        } else if (notice.type.find("downloading") != std::string::npos) {
                            auto dt = lock_and_find(notice.group, download_threads, download_mutex);
                            if (dt) {
                                const auto &dp = dt->download_progress;
                                const auto is_paused = dp->is_paused.load();
                                const auto &button_str = is_paused ? lang["resume"] : lang["pause"];
                                if (ImGui::MenuItem(button_str.c_str())) {
                                    auto update_notice_type = [&emuenv](const time_t &time, const std::string &new_type) {
                                        auto it = std::find_if(notice_list["global"].begin(), notice_list["global"].end(),
                                            [&time](const auto &notice) { return notice.time == time; });

                                        if (it != notice_list["global"].end()) {
                                            it->type = new_type;

                                            auto info_it = std::find_if(notice_info.begin(), notice_info.end(),
                                                [&time](const auto &info) { return info.time == time; });
                                            if (info_it != notice_info.end())
                                                info_it->type = new_type;

                                            save_notice_list(emuenv);
                                        }
                                    };

                                    {
                                        std::unique_lock<std::mutex> lock(download_mutex);
                                        dp->is_paused = !is_paused;
                                        dp->is_waiting = is_paused;
                                    }

                                    if (is_paused)
                                        update_notice_type(notice.time, "downloading"); // Resume
                                    else {
                                        update_notice_type(notice.time, "downloading_paused"); // Pause
                                        std::unique_lock<std::mutex> lock(dp->state.mutex);
                                        dp->state.pause = true; // Set pause state
                                        dp->state.cv.notify_one(); // Notify the download thread
                                    }
                                }
                            }
                        }

                        if (ImGui::Button(common["cancel"].c_str())) {
                            auto dt = lock_and_find(notice.group, download_threads, download_mutex);
                            if (dt) {
                                std::unique_lock<std::mutex> lock(dt->download_progress->state.mutex);
                                dt->download_progress->state.pause = true;
                            }
                            ImGui::OpenPopup("cancel_download_popup");
                        }

                        // Draw Cancel popup
                        const auto POPUP_SIZE = ImVec2(760.0f * SCALE.x, 436.0f * SCALE.y);
                        ImGui::SetNextWindowSize(POPUP_SIZE, ImGuiCond_Always);
                        ImGui::SetNextWindowPos(ImVec2(WINDOW_POS.x + (VIEWPORT_SIZE.x / 2.f) - (POPUP_SIZE.x / 2.f), WINDOW_POS.y + (VIEWPORT_SIZE.y / 2.f) - (POPUP_SIZE.y / 2.f)), ImGuiCond_Always);
                        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f * SCALE.x);
                        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 15.f * SCALE.x);
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
                            if (ImGui::Button(common["no"].c_str(), LARGE_BUTTON_SIZE)) {
                                auto dt = lock_and_find(notice.group, download_threads, download_mutex);
                                if (dt) {
                                    if (!dt->download_progress->is_paused.load()) {
                                        std::unique_lock<std::mutex> lock(download_mutex);
                                        dt->download_progress->is_waiting = true;
                                    }
                                }

                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::SameLine(0, 40.f * SCALE.x);
                            if (ImGui::Button(common["yes"].c_str(), LARGE_BUTTON_SIZE)) {
                                auto dt = lock_and_find(notice.group, download_threads, download_mutex);
                                if (dt) {
                                    std::unique_lock<std::mutex> lock(dt->download_progress->state.mutex);
                                    dt->download_progress->state.pause = false;
                                    dt->download_progress->state.download = false;
                                    dt->download_progress->state.cv.notify_one();
                                }

                                notice_tasks.push_back({ NoticeAction::REMOVE_FULL, notice.time });

                                gui.updates_install.erase(notice.id);

                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
                        }
                        ImGui::PopStyleVar(3);

                        ImGui::EndPopup();
                    }
                }
                ImGui::SetWindowFontScale(0.94f * RES_SCALE.x);
                const auto notice_time = get_notice_time(gui, emuenv, notice.time);
                const auto notice_time_size = ImGui::CalcTextSize(notice_time.c_str());
                ImGui::SetCursorPos(ImVec2(POPUP_SIZE.x - (34.f * SCALE.x) - notice_time_size.x, ICON_POS.y + SELECT_SIZE.y + ITEM_SPACING.y - ImGui::GetFontSize() - (8.f * SCALE.y)));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", notice_time.c_str());
                ImGui::PopID();
                ImGui::ScrollWhenDragging();
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);

        if (!notice_info.empty()) {
            const auto DELETE_POPUP_SIZE = ImVec2(756.0f * SCALE.x, 436.0f * SCALE.y);
            const auto BUTTON_SIZE = ImVec2(320.f * SCALE.x, 46.f * SCALE.y);

            ImGui::SetWindowFontScale(1.f * RES_SCALE.x);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f * SCALE.x);
            ImGui::SetCursorPos(ImVec2(VIEWPORT_SIZE.x - (70.f * SCALE.x), VIEWPORT_SIZE.y - (52.f * SCALE.y)));
            if (ImGui::Button("...", ImVec2(64.f * SCALE.x, 40.f * SCALE.y)) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_triangle)))
                ImGui::OpenPopup("...");
            if (ImGui::BeginPopup("...", ImGuiWindowFlags_NoMove)) {
                const auto has_download_state = [&](const std::string &type) {
                    return std::any_of(notice_info.begin(), notice_info.end(), [&](const auto &n) {
                        return n.type == type;
                    });
                };
                const auto has_downloading = has_download_state("downloading");
                const auto has_download_paused = has_download_state("downloading_paused");

                auto set_download_state = [&](bool pause) {
                    {
                        std::unique_lock<std::mutex> manager_lock(download_manager_mutex);
                        for (auto &[group, dt] : download_threads) {
                            auto &dp = dt->download_progress;
                            if (pause && dp->is_paused.load())
                                continue;

                            {
                                std::unique_lock<std::mutex> lock(download_mutex);
                                dp->is_paused.store(pause);
                                dp->is_waiting.store(!pause);
                            }

                            if (pause) {
                                std::unique_lock<std::mutex> state_lock(dp->state.mutex);
                                dp->state.pause = pause;
                                dp->state.cv.notify_all();
                            }
                        }
                    }

                    const auto update_notice_type = [pause](auto &n) {
                        for (auto &info : n) {
                            if (info.type.find("downloading") == std::string::npos)
                                continue;
                            info.type = pause ? "downloading_paused" : "downloading";
                        }
                    };

                    update_notice_type(notice_info);
                    update_notice_type(notice_list["global"]);

                    save_notice_list(emuenv);
                    ImGui::CloseCurrentPopup();
                };

                if (has_download_paused && ImGui::MenuItem(lang["resume_all"].c_str()))
                    set_download_state(false);

                if (has_downloading && ImGui::MenuItem(lang["pause_all"].c_str()))
                    set_download_state(true);

                if (ImGui::Button(lang["delete_all"].c_str()))
                    ImGui::OpenPopup("Delete All");
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f);
                ImGui::SetNextWindowSize(DELETE_POPUP_SIZE, ImGuiCond_Always);
                ImGui::SetNextWindowPos(ImVec2(VIEWPORT_POS.x + (VIEWPORT_SIZE.x / 2.f) - (DELETE_POPUP_SIZE.x / 2.f), VIEWPORT_POS.y + (VIEWPORT_SIZE.y / 2.f) - (DELETE_POPUP_SIZE.y / 2.f)));
                if (ImGui::BeginPopupModal("Delete All", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings)) {
                    ImGui::SetWindowFontScale(1.4f * RES_SCALE.x);
                    auto &common = emuenv.common_dialog.lang.common;
                    ImGui::SetCursorPosY((DELETE_POPUP_SIZE.y / 2.f) - (46.f * SCALE.y));
                    TextColoredCentered(GUI_COLOR_TEXT, lang["notif_deleted"].c_str());
                    ImGui::SetCursorPos(ImVec2((DELETE_POPUP_SIZE.x / 2) - (BUTTON_SIZE.x + (20.f * SCALE.x)), DELETE_POPUP_SIZE.y - BUTTON_SIZE.y - (24.0f * SCALE.y)));
                    if (ImGui::Button(common["cancel"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_circle))) {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine(0.f, 20.f);
                    if (ImGui::Button(common["ok"].c_str(), BUTTON_SIZE) || ImGui::IsKeyPressed(static_cast<ImGuiKey>(emuenv.cfg.keyboard_button_cross))) {
                        for (const auto &info : notice_info) {
                            if ((info.type.find("downloading") == std::string::npos) && (info.type != "wait_install")) {
                                notice_tasks.push_back({ NoticeAction::REMOVE_FULL, info.time });
                                LOG_DEBUG("Removing notice icon for title: {}", info.name);
                            }
                        }
                        if (notice_info.empty())
                            notice_info_state = false;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopStyleVar();
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();
            ImGui::SetWindowFontScale(1.f);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();

    const auto display_size = ImGui::GetIO().DisplaySize;
    draw_list->AddRectFilled(ImVec2(0.f, 0.f), ImVec2(display_size.x, VIEWPORT_POS.y), IM_COL32(0.f, 0.f, 0.f, 255.f));
    draw_list->AddRectFilled(ImVec2(VIEWPORT_WIDTH_POS_MAX, VIEWPORT_POS.y), ImVec2(display_size.x, VIEWPORT_POS.y + display_size.y), IM_COL32(0.f, 0.f, 0.f, 255.f));
}

void draw_information_bar(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 RES_SCALE(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const ImVec2 SCALE(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);

    const ImVec2 INFO_BAR_SIZE(VIEWPORT_SIZE.x, 32.f * SCALE.y);

    constexpr ImU32 DEFAULT_BAR_COLOR = 0xFF000000; // Black
    constexpr ImU32 DEFAULT_INDICATOR_COLOR = 0xFFFFFFFF; // White

    const auto is_12_hour_format = emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR;
    const auto is_notif_pos = !gui.vita_area.start_screen && gui.vita_area.home_screen ? 78.f * SCALE.x : 0.f;
    const auto is_theme_color = gui.vita_area.home_screen || gui.vita_area.start_screen;
    const auto indicator_color = gui.information_bar_color.indicator;
    const auto bar_color = gui.information_bar_color.bar;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, VIEWPORT_POS.y + INFO_BAR_SIZE.y), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::Begin("##information_bar", &gui.vita_area.information_bar, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(3);

    const ImVec2 INFO_BAR_POS_MAX(VIEWPORT_POS.x + INFO_BAR_SIZE.x, VIEWPORT_POS.y + INFO_BAR_SIZE.y);
    const auto draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(VIEWPORT_POS, INFO_BAR_POS_MAX, is_theme_color ? bar_color : DEFAULT_BAR_COLOR, 0.f, ImDrawFlags_RoundCornersAll);

    if (gui.vita_area.home_screen) {
        const auto HOME_ICON_POS_CENTER = VIEWPORT_POS.x + (INFO_BAR_SIZE.x / 2.f) - (32.f * (static_cast<float>(gui.live_area_current_open_apps_list.size()) / 2.f)) * SCALE.x;
        const auto APP_IS_OPEN = gui.live_area_app_current_open >= 0;

        // Draw Home Icon
        const std::vector<ImVec2> HOME_UP_POS = {
            ImVec2(HOME_ICON_POS_CENTER - (13.f * SCALE.x), VIEWPORT_POS.y + (16.f * SCALE.y)),
            ImVec2(HOME_ICON_POS_CENTER, VIEWPORT_POS.y + (6.f * SCALE.y)),
            ImVec2(HOME_ICON_POS_CENTER + (13.f * SCALE.x), VIEWPORT_POS.y + (16.f * SCALE.y))
        };
        const auto HOME_DOWN_POS_MINI = ImVec2(HOME_ICON_POS_CENTER - (8.f * SCALE.x), VIEWPORT_POS.y + (16.f * SCALE.y));
        const auto HOME_DOWN_POS_MAX = ImVec2(HOME_ICON_POS_CENTER + (8.f * SCALE.x), HOME_DOWN_POS_MINI.y + (10.f * SCALE.y));

        draw_list->AddTriangleFilled(HOME_UP_POS[0], HOME_UP_POS[1], HOME_UP_POS[2], indicator_color);
        draw_list->AddRectFilled(HOME_DOWN_POS_MINI, HOME_DOWN_POS_MAX, indicator_color);
        if (APP_IS_OPEN) {
            draw_list->AddTriangleFilled(HOME_UP_POS[0], HOME_UP_POS[1], HOME_UP_POS[2], IM_COL32(0.f, 0.f, 0.f, 100.f));
            draw_list->AddRectFilled(HOME_DOWN_POS_MINI, HOME_DOWN_POS_MAX, IM_COL32(0.f, 0.f, 0.f, 100.f));
        }
        draw_list->AddRectFilled(ImVec2(HOME_ICON_POS_CENTER - (3.f * SCALE.x), VIEWPORT_POS.y + (18.5f * SCALE.y)), ImVec2(HOME_ICON_POS_CENTER + (3.f * SCALE.x), VIEWPORT_POS.y + (26.f * SCALE.y)), bar_color);

        // Draw App Icon
        const float decal_app_icon_pos = 34.f * ((static_cast<float>(gui.live_area_current_open_apps_list.size()) - 2) / 2.f);
        const auto ICON_SIZE_SCALE = 28.f * SCALE.x;

        for (auto a = 0; a < gui.live_area_current_open_apps_list.size(); a++) {
            const ImVec2 ICON_POS_MIN(VIEWPORT_POS.x + (INFO_BAR_SIZE.x / 2.f) - (14.f * SCALE.x) - (decal_app_icon_pos * SCALE.x) + (a * (34 * SCALE.x)), VIEWPORT_POS.y + (2.f * SCALE.y));
            const ImVec2 ICON_POS_MAX(ICON_POS_MIN.x + ICON_SIZE_SCALE, ICON_POS_MIN.y + ICON_SIZE_SCALE);
            const ImVec2 ICON_CENTER_POS(ICON_POS_MIN.x + (ICON_SIZE_SCALE / 2.f), ICON_POS_MIN.y + (ICON_SIZE_SCALE / 2.f));
            const auto &APPS_OPENED = gui.live_area_current_open_apps_list[a];
            auto &APP_ICON_TYPE = APPS_OPENED.starts_with("emu") ? gui.app_selector.emu_apps_icon : gui.app_selector.vita_apps_icon;

            // Check if icon exist
            if (APP_ICON_TYPE.contains(APPS_OPENED))
                draw_list->AddImageRounded(APP_ICON_TYPE[APPS_OPENED], ICON_POS_MIN, ICON_POS_MAX, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, ICON_SIZE_SCALE, ImDrawFlags_RoundCornersAll);
            else
                draw_list->AddCircleFilled(ICON_CENTER_POS, ICON_SIZE_SCALE / 2.f, IM_COL32_WHITE);

            // hide Icon no opened
            if (!APP_IS_OPEN || (gui.live_area_current_open_apps_list[gui.live_area_app_current_open] != APPS_OPENED))
                draw_list->AddCircleFilled(ICON_CENTER_POS, ICON_SIZE_SCALE / 2.f, IM_COL32(0.f, 0.f, 0.f, 140.f));
        }
    }

    constexpr auto PIX_FONT_SCALE = 19.2f / 24.f;
    const auto DEFAULT_FONT_SCALE = ImGui::GetFontSize() / (19.2f * emuenv.manual_dpi_scale);
    const auto CLOCK_DEFAULT_FONT_SCALE = (24.f * emuenv.manual_dpi_scale) * DEFAULT_FONT_SCALE;
    const auto DAY_MOMENT_DEFAULT_FONT_SCALE = (18.f * emuenv.manual_dpi_scale) * DEFAULT_FONT_SCALE;
    const auto CLOCK_FONT_SIZE_SCALE = CLOCK_DEFAULT_FONT_SCALE / ImGui::GetFontSize();
    const auto DAY_MOMENT_FONT_SIZE_SCALE = DAY_MOMENT_DEFAULT_FONT_SCALE / ImGui::GetFontSize();

    const auto now = std::chrono::system_clock::now();
    const auto tt = std::chrono::system_clock::to_time_t(now);

    tm local = {};
    SAFE_LOCALTIME(&tt, &local);

    auto DATE_TIME = get_date_time(gui, emuenv, local);
    const auto CALC_CLOCK_SIZE = ImGui::CalcTextSize(DATE_TIME[DateTime::CLOCK].c_str());
    const auto CLOCK_SIZE_SCALE = ImVec2((CALC_CLOCK_SIZE.x * CLOCK_FONT_SIZE_SCALE) * RES_SCALE.x, (CALC_CLOCK_SIZE.y * CLOCK_FONT_SIZE_SCALE * PIX_FONT_SCALE) * RES_SCALE.y);
    const auto CALC_DAY_MOMENT_SIZE = ImGui::CalcTextSize(DATE_TIME[DateTime::DAY_MOMENT].c_str());
    const auto DAY_MOMENT_SIZE_SCALE = emuenv.io.user_id.empty() || is_12_hour_format ? ImVec2((CALC_DAY_MOMENT_SIZE.x * DAY_MOMENT_FONT_SIZE_SCALE) * RES_SCALE.y, (CALC_DAY_MOMENT_SIZE.y * DAY_MOMENT_FONT_SIZE_SCALE * PIX_FONT_SCALE) * RES_SCALE.y) : ImVec2(0.f, 0.f);

    const ImVec2 CLOCK_POS(VIEWPORT_POS.x + INFO_BAR_SIZE.x - (64.f * SCALE.x) - CLOCK_SIZE_SCALE.x - DAY_MOMENT_SIZE_SCALE.x - is_notif_pos, VIEWPORT_POS.y + (INFO_BAR_SIZE.y / 2.f) - (CLOCK_SIZE_SCALE.y / 2.f));
    const auto DAY_MOMENT_POS = ImVec2(CLOCK_POS.x + CLOCK_SIZE_SCALE.x + (6.f * SCALE.x), CLOCK_POS.y + (CLOCK_SIZE_SCALE.y - DAY_MOMENT_SIZE_SCALE.y));

    // Draw clock
    draw_list->AddText(gui.vita_font[emuenv.current_font_level], CLOCK_DEFAULT_FONT_SCALE * RES_SCALE.x, CLOCK_POS, is_theme_color ? indicator_color : DEFAULT_INDICATOR_COLOR, DATE_TIME[DateTime::CLOCK].c_str());
    if (emuenv.io.user_id.empty() || is_12_hour_format)
        draw_list->AddText(gui.vita_font[emuenv.current_font_level], DAY_MOMENT_DEFAULT_FONT_SCALE * RES_SCALE.x, DAY_MOMENT_POS, is_theme_color ? indicator_color : DEFAULT_INDICATOR_COLOR, DATE_TIME[DateTime::DAY_MOMENT].c_str());

    // Set full size and position of battery
    const auto FULL_BATTERY_SIZE = 38.f * SCALE.x;
    const auto BATTERY_HEIGHT_MIN_POS = VIEWPORT_POS.y + (5.f * SCALE.y);
    const ImVec2 BATTERY_MAX_POS(VIEWPORT_POS.x + INFO_BAR_SIZE.x - (12.f * SCALE.x) - is_notif_pos, BATTERY_HEIGHT_MIN_POS + (22 * SCALE.y));
    const ImVec2 BATTERY_BASE_MIN_POS(BATTERY_MAX_POS.x - FULL_BATTERY_SIZE - (4.f * SCALE.x), VIEWPORT_POS.y + (12.f * SCALE.y));
    const ImVec2 BATTERY_BASE_MAX_POS(BATTERY_BASE_MIN_POS.x + (4.f * SCALE.x), BATTERY_BASE_MIN_POS.y + (8 * SCALE.y));

    // Draw battery background
    constexpr ImU32 BATTERY_BG_COLOR = IM_COL32(128.f, 124.f, 125.f, 255.f);
    draw_list->AddRectFilled(BATTERY_BASE_MIN_POS, BATTERY_BASE_MAX_POS, BATTERY_BG_COLOR);
    draw_list->AddRectFilled(ImVec2(BATTERY_MAX_POS.x - FULL_BATTERY_SIZE, BATTERY_HEIGHT_MIN_POS), BATTERY_MAX_POS, BATTERY_BG_COLOR, 2.f * SCALE.x, ImDrawFlags_RoundCornersAll);

    // Set default battery size
    auto battery_size = FULL_BATTERY_SIZE;

    // Get battery level and adjust size accordingly to the battery level
    int32_t res;
    SDL_GetPowerInfo(NULL, &res);
    if ((res >= 0) && (res <= 75)) {
        if (res <= 25)
            battery_size *= 25.f;
        else if (res <= 50)
            battery_size *= 50.f;
        else
            battery_size *= 75.f;

        battery_size /= 100.f;
    }

    // Set battery color depending on battery level: red for levels less than or equal to 25% and green for levels above this threshold.
    const ImU32 BATTERY_COLOR = (res >= 0) && (res <= 25) ? IM_COL32(225.f, 50.f, 50.f, 255.f) : IM_COL32(90.f, 200.f, 30.f, 255.f);

    // Draw battery level
    if (battery_size == FULL_BATTERY_SIZE)
        draw_list->AddRectFilled(BATTERY_BASE_MIN_POS, BATTERY_BASE_MAX_POS, BATTERY_COLOR);
    draw_list->AddRectFilled(ImVec2(BATTERY_MAX_POS.x - battery_size, BATTERY_HEIGHT_MIN_POS), BATTERY_MAX_POS, BATTERY_COLOR, 2.f * SCALE.x, ImDrawFlags_RoundCornersAll);

    if (emuenv.display.imgui_render && !gui.vita_area.start_screen && !gui.vita_area.user_management && !gui.help_menu.vita3k_update && get_sys_apps_state(gui) && (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) || ImGui::IsItemClicked(0)))
        gui.vita_area.information_bar = false;

    if (is_notif_pos)
        draw_notice_info(gui, emuenv);

    ImGui::End();
}

} // namespace gui
