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
#include <v3kn/storage.h>

#include <emuenv/state.h>
#include <io/device.h>
#include <io/functions.h>
#include <io/state.h>

#include <packages/sfo.h>

#include <boost/algorithm/string/trim.hpp>

#include <util/fs.h>
#include <util/log.h>
#include <util/net_utils.h>

#include <miniz.h>
#include <pugixml.hpp>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <optional>
#include <regex>
#include <sstream>
#include <thread>
#include <unordered_set>
#include <utility>

static bool valid_trophy_context(const np::trophy::Context &context) {
    if (context.group_count >= np::trophy::MAX_GROUPS || context.trophy_count > np::trophy::MAX_TROPHIES)
        return false;

    uint32_t trophy_total = 0;
    for (uint32_t group_index = 0; group_index < context.group_count + 1; ++group_index) {
        const auto group_count = context.trophy_count_by_group[group_index];
        if (group_count > np::trophy::MAX_TROPHIES - trophy_total)
            return false;
        trophy_total += group_count;
    }
    return trophy_total == context.trophy_count;
}

bool load_and_compute_trophy_stats(EmuEnvState &emuenv, np::trophy::Context &context, const std::string &np_com_id, TrophyProgressInfo &trophy_progress_info) {
    const auto trophy_path = emuenv.vita_fs_path / "ux0/user" / emuenv.io.user_id / "trophy";
    const auto progress_path = trophy_path / "data" / np_com_id / "TROPUSR.DAT";
    if (!fs::exists(progress_path)) {
        LOG_WARN("Trophy TROPUSR.DAT not found for NP communication ID {}", np_com_id);
        return false;
    }

    trophy_progress_info = {};
    trophy_progress_info.last_updated = fs_utils::to_time_t(fs::last_write_time(progress_path));
    const auto vita_progress_path = device::construct_normalized_path(VitaIoDevice::ux0,
        "user/" + emuenv.io.user_id + "/trophy/data/" + np_com_id + "/TROPUSR.DAT");
    const auto progress_file = open_file(emuenv.io, vita_progress_path.c_str(), SCE_O_RDONLY, emuenv.vita_fs_path, "load_trophy_progress_file");
    if (progress_file < 0) {
        LOG_ERROR("Failed to open trophy progress for NP communication ID {}", np_com_id);
        return false;
    }

    context = np::trophy::Context{};
    context.io = &emuenv.io;
    context.vita_fs_path = emuenv.vita_fs_path;
    const bool loaded = context.load_trophy_progress_file(progress_file);
    close_file(emuenv.io, progress_file, "load_trophy_progress_file");
    if (!loaded) {
        LOG_ERROR("Failed to load trophy progress for NP communication ID {}", np_com_id);
        return false;
    }
    if (context.group_count >= np::trophy::MAX_GROUPS || context.trophy_count > np::trophy::MAX_TROPHIES) {
        LOG_ERROR("Invalid trophy counts in progress file for NP communication ID {}", np_com_id);
        return false;
    }

    uint32_t trophy_index = 0;
    for (uint32_t group_index = 0; group_index < context.group_count + 1; ++group_index) {
        const auto group_count = context.trophy_count_by_group[group_index];
        if (group_count > np::trophy::MAX_TROPHIES - trophy_index) {
            LOG_ERROR("Invalid trophy group count in progress file for NP communication ID {}", np_com_id);
            return false;
        }
        if (group_count == 0)
            continue;

        const auto group_id = fmt::format("{:03}", group_index);
        for (uint32_t i = 0; i < group_count; ++i, ++trophy_index) {
            if (!context.is_trophy_unlocked(trophy_index))
                continue;

            const auto trophy_id = fmt::format("{:03}", trophy_index);
            const auto grade = context.trophy_kinds[trophy_index];
            ++trophy_progress_info.unlocked_type_count[group_id][grade];
            ++trophy_progress_info.unlocked_type_count["global"][grade];
            trophy_progress_info.unlocked_ids[group_id].push_back(trophy_id);
            trophy_progress_info.unlocked_ids["global"].push_back(trophy_id);
        }
        trophy_progress_info.progress[group_id] = static_cast<uint32_t>(trophy_progress_info.unlocked_ids[group_id].size() * 100 / group_count);
    }

    if (trophy_index != context.trophy_count) {
        LOG_ERROR("Trophy group totals do not match the total trophy count for NP communication ID {}", np_com_id);
        return false;
    }

    if (context.trophy_count != 0)
        trophy_progress_info.progress["global"] = static_cast<uint32_t>(trophy_progress_info.unlocked_ids["global"].size() * 100 / context.trophy_count);

    return true;
}

namespace v3kn {

constexpr uint32_t TROPHY_SYNC_VERSION = 2;
std::atomic_bool trophy_sync_running{ false };

struct CurlSessionGuard {
    explicit CurlSessionGuard(net_utils::CurlSession value)
        : session(std::move(value)) {}

    ~CurlSessionGuard() {
        net_utils::cleanup_curl_session(session);
    }

    net_utils::CurlSession session;
};

fs::path trophy_sync_state_path(const EmuEnvState &emuenv) {
    return emuenv.vita_fs_path / "ux0/user" / emuenv.io.user_id / "trophy_sync.xml";
}

bool valid_trophy_id(const std::string &id) {
    return !id.empty() && std::all_of(id.begin(), id.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '_' || character == '-';
    });
}

bool load_trophy_sync_state(const EmuEnvState &emuenv, time_t &last_sync, std::unordered_set<std::string> &pending_ids) {
    last_sync = 0;
    pending_ids.clear();
    const auto path = trophy_sync_state_path(emuenv);
    if (!fs::exists(path))
        return false;

    pugi::xml_document document;
    if (!document.load_file(path.string().c_str())) {
        LOG_WARN("Failed to load trophy sync state: {}", path.string());
        return false;
    }
    const auto root = document.child("trophy_sync");
    if (!root || root.attribute("version").as_uint() != TROPHY_SYNC_VERSION) {
        LOG_WARN("Trophy sync state has an unsupported format: {}", path.string());
        return false;
    }

    last_sync = static_cast<time_t>(root.attribute("last_sync").as_ullong());
    for (const auto &node : root.child("pending").children("npcom")) {
        const std::string id = node.attribute("id").as_string();
        if (valid_trophy_id(id))
            pending_ids.insert(id);
    }
    return true;
}

bool save_trophy_sync_state(const EmuEnvState &emuenv, time_t last_sync, const std::unordered_set<std::string> &pending_ids) {
    const auto path = trophy_sync_state_path(emuenv);
    fs::create_directories(path.parent_path());
    pugi::xml_document document;
    auto root = document.append_child("trophy_sync");
    root.append_attribute("version").set_value(TROPHY_SYNC_VERSION);
    root.append_attribute("last_sync").set_value(static_cast<unsigned long long>(last_sync));
    auto pending = root.append_child("pending");
    for (const auto &id : pending_ids)
        pending.append_child("npcom").append_attribute("id").set_value(id.c_str());
    if (!document.save_file(path.string().c_str())) {
        LOG_ERROR("Failed to save trophy sync state: {}", path.string());
        return false;
    }
    return true;
}

bool load_local_trophy_sync(EmuEnvState &emuenv, const std::string &np_com_id, TrophySync &sync_info) {
    if (!valid_trophy_id(np_com_id))
        return false;

    np::trophy::Context context{};
    TrophyProgressInfo progress_info{};
    if (!load_and_compute_trophy_stats(emuenv, context, np_com_id, progress_info))
        return false;

    sync_info.last_updated = progress_info.last_updated;
    sync_info.progress = progress_info.progress["global"];
    sync_info.unlocked_ids = std::move(progress_info.unlocked_ids["global"]);
    const auto &counts = progress_info.unlocked_type_count["global"];
    sync_info.platinum = counts.contains(np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM) ? counts.at(np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM) : 0;
    sync_info.gold = counts.contains(np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD) ? counts.at(np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD) : 0;
    sync_info.silver = counts.contains(np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER) ? counts.at(np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER) : 0;
    sync_info.bronze = counts.contains(np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE) ? counts.at(np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE) : 0;
    return true;
}

void collect_local_trophy_progress(EmuEnvState &emuenv, std::map<std::string, TrophySync> &progress) {
    const auto data_path = emuenv.vita_fs_path / "ux0/user" / emuenv.io.user_id / "trophy" / "data";
    if (!fs::exists(data_path))
        return;
    for (const auto &entry : fs::directory_iterator(data_path)) {
        if (!entry.is_directory())
            continue;

        const auto id = entry.path().filename().string();
        if (progress.contains(id))
            continue;

        TrophySync sync_info{};
        if (load_local_trophy_sync(emuenv, id, sync_info) && !sync_info.unlocked_ids.empty())
            progress.emplace(id, std::move(sync_info));
    }
}

bool update_trophy_sync_state(EmuEnvState &emuenv, const std::unordered_set<std::string> &synced_ids) {
    time_t last_sync = 0;
    std::unordered_set<std::string> pending_ids;
    load_trophy_sync_state(emuenv, last_sync, pending_ids);
    if (synced_ids.empty())
        pending_ids.clear();
    else {
        for (const auto &id : synced_ids)
            pending_ids.erase(id);
    }

    return save_trophy_sync_state(emuenv, std::time(nullptr), pending_ids);
}

void report_trophy_sync_error(const TrophySyncErrorCallback &on_error, const Error &error) {
    LOG_ERROR("V3KN trophy sync: {}", error.message);
    if (on_error)
        on_error(error);
}

static void start_trophy_sync_internal(EmuEnvState &emuenv, std::map<std::string, TrophySync> trophy_progress,
    std::atomic<float> &sync_progress, std::atomic<bool> &sync_cancel, std::vector<std::string> &sync_downloaded,
    std::unordered_set<std::string> pending_ids, time_t local_last_sync, TrophySyncErrorCallback on_error) {
    std::erase_if(trophy_progress, [](const auto &entry) {
        return !valid_trophy_id(entry.first);
    });
    bool expected = false;
    if (!trophy_sync_running.compare_exchange_strong(expected, true)) {
        report_trophy_sync_error(on_error, { .message = "A trophy synchronization is already running." });
        sync_progress = 1.f;
        return;
    }

    sync_progress = 0.f;
    sync_cancel = false;
    sync_downloaded.clear();
    std::thread([&emuenv, trophy_progress = std::move(trophy_progress), &sync_progress, &sync_cancel, &sync_downloaded,
                    pending_ids = std::move(pending_ids), local_last_sync, on_error = std::move(on_error)]() mutable {
        struct RunningGuard {
            ~RunningGuard() { trophy_sync_running.store(false); }
        } running_guard;
        auto fail = [&](const Error &error) {
            report_trophy_sync_error(on_error, error);
            sync_cancel = true;
            sync_progress = 1.f;
        };

        try {
            const auto user_info = get_v3kn_user_info(emuenv);
            const auto user_id = emuenv.io.user_id;
            if (user_info.token.empty() || user_info.host.empty()) {
                fail({ .message = "No authenticated V3KN account is available." });
                return;
            }
            const auto account_matches = [&] {
                const auto current = get_v3kn_user_info(emuenv);
                return emuenv.io.user_id == user_id && current.host == user_info.host && current.token == user_info.token;
            };
            const auto trophies_path = emuenv.vita_fs_path / "ux0/user" / user_id / "trophy";

            const auto conf_check_url = get_v3kn_server_url(user_info.host, "v3kn/check_trophy_conf_data");
            const auto conf_response = net_utils::get_web_response_ex(conf_check_url, user_info.token);
            handle_v3kn_status(emuenv, conf_response, &user_info);
            if (!conf_response.is_success() || conf_response.body.starts_with("ERR:")) {
                fail(get_v3kn_error(conf_response));
                return;
            }
            if (conf_response.body != "OK" && !conf_response.body.starts_with("WARN:")) {
                pugi::xml_document conf_document;
                if (!conf_document.load_string(conf_response.body.c_str())) {
                    fail({ .message = "The V3KN trophy configuration response is invalid." });
                    return;
                }

                CurlSessionGuard session(net_utils::init_curl_upload_session(user_info.token));
                const auto missing_confs = conf_document.child("missing_confs");
                const auto count = std::distance(missing_confs.children("npcommid").begin(), missing_confs.children("npcommid").end());
                size_t processed = 0;
                for (const auto &node : missing_confs.children("npcommid")) {
                    if (sync_cancel || !account_matches())
                        break;
                    const std::string id = node.text().as_string();
                    const auto conf_path = trophies_path / "conf" / id;
                    if (!valid_trophy_id(id) || !fs::exists(conf_path) || !fs::is_directory(conf_path)) {
                        LOG_WARN("Missing trophy configuration data for {}", id);
                        ++processed;
                        continue;
                    }
                    const auto url = get_v3kn_server_url(user_info.host, fmt::format("v3kn/upload_trophy_conf_data?id={}", id));
                    for (const auto &file : fs::directory_iterator(conf_path)) {
                        if (!file.is_regular_file())
                            continue;
                        const auto response = net_utils::upload_file(url, fs_utils::path_to_utf8(file.path()), user_info.token, "", {}, &session.session);
                        handle_v3kn_status(emuenv, response, &user_info);
                        if (!response.is_success() || response.body.starts_with("ERR:")) {
                            fail(get_v3kn_error(response));
                            return;
                        }
                    }
                    ++processed;
                    if (count > 0)
                        sync_progress = static_cast<float>(processed) / static_cast<float>(count) * 0.1f;
                }
            }

            if (sync_cancel || !account_matches()) {
                sync_progress = 1.f;
                return;
            }

            const auto info_url = get_v3kn_server_url(user_info.host, "v3kn/trophies_info");
            const auto info_response = net_utils::get_web_response_ex(info_url, user_info.token);
            handle_v3kn_status(emuenv, info_response, &user_info);
            if (!info_response.is_success() || info_response.body.starts_with("ERR:")) {
                fail(get_v3kn_error(info_response));
                return;
            }

            std::map<std::string, uint32_t> server_progress;
            std::map<std::string, time_t> server_updated;
            pugi::xml_document document;
            pugi::xml_parse_result parse_result{};
            bool incomplete = info_response.body.starts_with("WARN:");
            time_t server_last_sync = 0;
            if (!incomplete) {
                parse_result = document.load_string(info_response.body.c_str());
                if (!parse_result) {
                    incomplete = true;
                    LOG_WARN("Failed to parse V3KN trophy information: {}", parse_result.description());
                } else {
                    auto trophies = document.child("trophies");
                    if (!trophies)
                        incomplete = true;
                    else {
                        server_last_sync = static_cast<time_t>(trophies.attribute("last_sync").as_ullong());
                        if (!trophies.attribute("last_sync"))
                            incomplete = true;
                        for (auto node = trophies.child("trophy"); node;) {
                            const auto next = node.next_sibling("trophy");
                            const std::string id = node.attribute("id").as_string();
                            const auto percent = node.attribute("progress").as_uint();
                            if (valid_trophy_id(id) && percent > 0) {
                                server_progress[id] = percent;
                                server_updated[id] = static_cast<time_t>(node.attribute("last_updated").as_ullong());
                            }
                            trophies.remove_child(node);
                            node = next;
                            incomplete = true;
                        }
                        for (const auto &node : trophies.children("np")) {
                            const std::string id = node.attribute("commid").as_string();
                            const auto percent = node.child("progress").attribute("percent").as_uint();
                            if (!valid_trophy_id(id))
                                continue;
                            if (percent > 0 || node.child("progress").attribute("unlocked_count").as_uint() > 0) {
                                server_progress[id] = percent;
                                server_updated[id] = static_cast<time_t>(node.attribute("last_updated").as_ullong());
                            } else {
                                incomplete = true;
                            }
                        }
                    }
                }
            }

            if (incomplete && trophy_progress.empty())
                collect_local_trophy_progress(emuenv, trophy_progress);

            std::unordered_set<std::string> effective_filter = pending_ids;
            const bool filter_active = !pending_ids.empty() || local_last_sync > 0;
            if (filter_active && server_last_sync > local_last_sync) {
                for (const auto &[id, updated] : server_updated) {
                    if (updated > local_last_sync)
                        effective_filter.insert(id);
                }
            }

            std::map<std::string, time_t> to_download;
            std::vector<std::string> to_upload;
            const auto queue_upload = [&](const std::string &id) {
                if (std::find(to_upload.begin(), to_upload.end(), id) == to_upload.end())
                    to_upload.push_back(id);
            };
            for (const auto &[id, remote_progress] : server_progress) {
                if (filter_active && !effective_filter.contains(id))
                    continue;
                const auto local = trophy_progress.find(id);
                if (local == trophy_progress.end())
                    to_download[id] = server_updated[id];
                else if (remote_progress > local->second.progress)
                    to_download[id] = server_updated[id];
                else if (local->second.progress > remote_progress)
                    queue_upload(id);
            }

            const bool force_upload = incomplete && to_download.empty();
            const bool resync_after_download = incomplete && !to_download.empty();
            for (const auto &[id, local] : trophy_progress) {
                if (filter_active && !force_upload && !effective_filter.contains(id))
                    continue;
                if (force_upload || !server_progress.contains(id))
                    queue_upload(id);
            }

            size_t total_operations = to_download.size() + to_upload.size();
            size_t completed_operations = 0;
            const auto update_progress = [&] {
                sync_progress = total_operations == 0 ? 1.f : 0.1f + 0.9f * static_cast<float>(completed_operations) / static_cast<float>(total_operations);
            };

            for (const auto &[id, updated] : to_download) {
                if (sync_cancel || !account_matches())
                    break;
                const auto trophy_file = trophies_path / "data" / id / "TROPUSR.DAT";
                const auto temp_file = trophy_file.parent_path() / "TROPUSR_v3kn_tmp.DAT";
                fs::create_directories(trophy_file.parent_path());
                const auto url = get_v3kn_server_url(user_info.host, fmt::format("v3kn/download_file?type=trophy&id={}", id));
                const auto response = net_utils::download_file_ex(url, fs_utils::path_to_utf8(temp_file), {}, user_info.token);
                handle_v3kn_status(emuenv, response, &user_info);
                if (!response.is_success() || response.body.starts_with("ERR:")) {
                    if (fs::exists(temp_file))
                        fs::remove(temp_file);
                    fail(get_v3kn_error(response));
                    return;
                }
                if (!fs::exists(temp_file) || fs::file_size(temp_file) == 0) {
                    fail({ .message = fmt::format("Downloaded trophy progress for {} is empty.", id) });
                    return;
                }
                const auto temp_vita_path = device::construct_normalized_path(VitaIoDevice::ux0,
                    "user/" + emuenv.io.user_id + "/trophy/data/" + id + "/TROPUSR_v3kn_tmp.DAT");
                const auto validation_file = open_file(emuenv.io, temp_vita_path.c_str(), SCE_O_RDONLY, emuenv.vita_fs_path, "validate_downloaded_trophy_progress");
                np::trophy::Context downloaded_context{};
                downloaded_context.io = &emuenv.io;
                downloaded_context.vita_fs_path = emuenv.vita_fs_path;
                const bool valid_file = validation_file >= 0 && downloaded_context.load_trophy_progress_file(validation_file)
                    && valid_trophy_context(downloaded_context);
                if (validation_file >= 0)
                    close_file(emuenv.io, validation_file, "validate_downloaded_trophy_progress");
                if (!valid_file) {
                    fs::remove(temp_file);
                    fail({ .message = fmt::format("Downloaded trophy progress for {} is invalid.", id) });
                    return;
                }
                fs::last_write_time(temp_file, fs_utils::to_file_time(updated));

                const auto backup_file = trophy_file.parent_path() / "TROPUSR_v3kn_backup.DAT";
                std::error_code rename_error;
                const bool had_previous = fs::exists(trophy_file);
                if (had_previous) {
                    if (fs::exists(backup_file)) {
                        fs::remove(temp_file);
                        fail({ .message = fmt::format("A trophy recovery backup already exists at {}. Restore or move it before retrying.", backup_file.string()) });
                        return;
                    }
                    fs::rename(trophy_file, backup_file, rename_error);
                    if (rename_error) {
                        fs::remove(temp_file);
                        fail({ .message = fmt::format("Could not preserve existing trophy data: {}", rename_error.message()) });
                        return;
                    }
                }
                fs::rename(temp_file, trophy_file, rename_error);
                if (rename_error) {
                    if (had_previous) {
                        std::error_code restore_error;
                        fs::rename(backup_file, trophy_file, restore_error);
                        if (restore_error)
                            fail({ .message = fmt::format("Could not install downloaded trophy data ({}); original data is preserved at {}", rename_error.message(), backup_file.string()) });
                        else
                            fail({ .message = fmt::format("Could not install downloaded trophy data: {}", rename_error.message()) });
                    } else {
                        fail({ .message = fmt::format("Could not install downloaded trophy data: {}", rename_error.message()) });
                    }
                    return;
                }
                if (had_previous)
                    fs::remove(backup_file);
                sync_downloaded.push_back(id);
                ++completed_operations;
                update_progress();
            }

            if (sync_cancel) {
                sync_progress = 1.f;
                return;
            }

            if (resync_after_download) {
                for (const auto &id : sync_downloaded) {
                    TrophySync updated{};
                    if (load_local_trophy_sync(emuenv, id, updated)) {
                        trophy_progress[id] = std::move(updated);
                        queue_upload(id);
                    }
                }
                total_operations = completed_operations + to_upload.size();
            }

            if (!document.child("trophies"))
                document.append_child("trophies");
            CurlSessionGuard upload_session(net_utils::init_curl_upload_session(user_info.token));
            for (const auto &id : to_upload) {
                if (sync_cancel || !account_matches())
                    break;
                const auto local = trophy_progress.find(id);
                if (local == trophy_progress.end())
                    continue;
                const auto trophy_file = trophies_path / "data" / id / "TROPUSR.DAT";
                if (!fs::exists(trophy_file)) {
                    LOG_WARN("Local trophy progress file does not exist for {}", id);
                    continue;
                }

                auto trophies = document.child("trophies");
                trophies.attribute("last_sync") ? trophies.attribute("last_sync").set_value(static_cast<unsigned long long>(std::time(nullptr))) : trophies.append_attribute("last_sync").set_value(static_cast<unsigned long long>(std::time(nullptr)));
                auto np_node = trophies.find_child_by_attribute("np", "commid", id.c_str());
                if (!np_node) {
                    np_node = trophies.append_child("np");
                    np_node.append_attribute("commid").set_value(id.c_str());
                }
                auto updated_attr = np_node.attribute("last_updated");
                if (!updated_attr)
                    updated_attr = np_node.append_attribute("last_updated");
                updated_attr.set_value(static_cast<unsigned long long>(local->second.last_updated));
                auto progress_node = np_node.child("progress");
                if (!progress_node)
                    progress_node = np_node.append_child("progress");
                auto set_count = [&](const char *name, uint64_t value) {
                    auto attribute = progress_node.attribute(name);
                    if (!attribute)
                        attribute = progress_node.append_attribute(name);
                    attribute.set_value(value);
                };
                set_count("unlocked_count", local->second.unlocked_ids.size());
                set_count("platinum", local->second.platinum);
                set_count("gold", local->second.gold);
                set_count("silver", local->second.silver);
                set_count("bronze", local->second.bronze);
                set_count("percent", local->second.progress);
                auto unlocked = np_node.child("unlocked");
                if (!unlocked)
                    unlocked = np_node.append_child("unlocked");
                for (const auto &trophy_id : local->second.unlocked_ids) {
                    if (unlocked.find_child_by_attribute("trophy", "id", trophy_id.c_str()))
                        continue;
                    unlocked.append_child("trophy").append_attribute("id").set_value(trophy_id.c_str());
                }

                std::stringstream metadata;
                document.save(metadata);
                const auto url = get_v3kn_server_url(user_info.host, fmt::format("v3kn/upload_file?type=trophy&id={}", id));
                const auto response = net_utils::upload_file(url, fs_utils::path_to_utf8(trophy_file), user_info.token, metadata.str(), {}, &upload_session.session);
                handle_v3kn_status(emuenv, response, &user_info);
                if (!response.is_success() || !response.body.starts_with("OK:")) {
                    fail(get_v3kn_error(response));
                    return;
                }

                std::smatch quota_match;
                if (std::regex_search(response.body, quota_match, std::regex(R"(OK:(\d+):(\d+))")) && quota_match.size() == 3) {
                    std::lock_guard lock(emuenv.v3kn.account_state.user_info_mutex);
                    auto &current_user = emuenv.v3kn.account_state.user_info;
                    if (current_user.host == user_info.host && current_user.token == user_info.token) {
                        current_user.quota_used = std::strtoull(quota_match[1].str().c_str(), nullptr, 10);
                        current_user.quota_total = std::strtoull(quota_match[2].str().c_str(), nullptr, 10);
                    }
                }
                ++completed_operations;
                update_progress();
            }
            if (!sync_cancel) {
                if (total_operations == 0)
                    LOG_INFO("V3KN trophy sync: trophy information is already up to date");
                if (!update_trophy_sync_state(emuenv, pending_ids)) {
                    fail({ .message = "Failed to save local trophy synchronization state." });
                    return;
                }
            }
            sync_progress = 1.f;
        } catch (const fs::filesystem_error &error) {
            fail({ .message = fmt::format("Trophy synchronization filesystem error: {}", error.what()) });
        }
    }).detach();
}

void start_trophy_sync(EmuEnvState &emuenv, std::map<std::string, TrophySync> trophy_progress, std::atomic<float> &sync_progress,
    std::atomic<bool> &sync_cancel, std::vector<std::string> &sync_downloaded, TrophySyncErrorCallback on_error) {
    start_trophy_sync_internal(emuenv, std::move(trophy_progress), sync_progress, sync_cancel, sync_downloaded, {}, 0, std::move(on_error));
}

void start_trophy_auto_sync(EmuEnvState &emuenv, TrophySyncErrorCallback on_error) {
    if (!is_v3kn_logged_in())
        return;

    time_t last_sync = 0;
    std::unordered_set<std::string> pending_ids;
    std::map<std::string, TrophySync> trophy_progress;
    if (!load_trophy_sync_state(emuenv, last_sync, pending_ids)) {
        collect_local_trophy_progress(emuenv, trophy_progress);
        for (const auto &[id, _] : trophy_progress)
            pending_ids.insert(id);
    } else {
        for (const auto &id : pending_ids) {
            TrophySync sync_info{};
            if (load_local_trophy_sync(emuenv, id, sync_info))
                trophy_progress.emplace(id, std::move(sync_info));
        }
    }
    if (trophy_progress.empty())
        return;

    static std::atomic<float> progress{ 0.f };
    static std::atomic<bool> cancel{ false };
    static std::vector<std::string> downloaded;
    start_trophy_sync_internal(emuenv, std::move(trophy_progress), progress, cancel, downloaded, std::move(pending_ids), last_sync, std::move(on_error));
}

void add_pending_trophy_sync(EmuEnvState &emuenv, const std::string &np_com_id) {
    if (!valid_trophy_id(np_com_id))
        return;
    time_t last_sync = 0;
    std::unordered_set<std::string> pending_ids;
    load_trophy_sync_state(emuenv, last_sync, pending_ids);
    if (pending_ids.insert(np_com_id).second)
        save_trophy_sync_state(emuenv, last_sync, pending_ids);
}

// Online Storage
bool get_savedata_info(EmuEnvState &emuenv, const std::string &titleid, Error &error) {
    error = {};
    std::map<SaveDataType, SaveDataInfo> savedata_info;

    // Get local savedata info
    const fs::path save_data_path = { emuenv.vita_fs_path / "ux0/user" / emuenv.io.user_id / "savedata" / titleid };
    if (fs::exists(save_data_path) && !fs::is_empty(save_data_path)) {
        auto &save_data_info_local = savedata_info[SAVE_DATA_TYPE_LOCAL];
        uint64_t total_size_local = 0;
        time_t global_last_updated = 0;
        for (auto &file : fs::recursive_directory_iterator(save_data_path)) {
            if (!file.is_regular_file())
                continue;

            const auto file_size = fs::file_size(file.path());
            const auto last_updated = fs_utils::to_time_t(fs::last_write_time(file.path()));
            SaveEntry entry{
                .rel_path = fs::relative(file.path(), save_data_path).generic_string(),
                .last_updated = last_updated
            };
            save_data_info_local.files.push_back(entry);
            global_last_updated = std::max(global_last_updated, last_updated);
            total_size_local += file_size;
        }
        save_data_info_local.last_updated = global_last_updated;
        save_data_info_local.total_size = total_size_local;
    }
    {
        std::lock_guard lock(emuenv.v3kn.storage_state.savedata_info_mutex);
        emuenv.v3kn.storage_state.savedata_info = savedata_info;
    }

    const auto user_info = get_v3kn_user_info(emuenv);
    const std::string url = get_v3kn_server_url(user_info.host, fmt::format("v3kn/save_info?titleid={}", titleid));
    const auto response = net_utils::get_web_response_ex(url, user_info.token);

    // Handle response and update connection status
    handle_v3kn_status(emuenv, response, &user_info);

    if (!response.is_success()) {
        error = get_v3kn_error(response);
        return false;
    }

    if (response.body.starts_with("WARN:")) {
        LOG_WARN("V3KN get savedata info: server warning: {}", response.body);
        return true;
    } else if (response.body.starts_with("ERR:")) {
        error = get_v3kn_error(response);
        return false;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(response.body.c_str());
    if (!result) {
        LOG_WARN("V3KN get savedata info: failed to parse XML: {}", result.description());
        return false;
    }

    pugi::xml_node savedata_node = doc.child("savedata");
    if (!savedata_node) {
        LOG_WARN("V3KN get savedata info: missing <savedata> node");
        return false;
    }

    auto &save_data_info_cloud = savedata_info[SAVE_DATA_TYPE_CLOUD];
    save_data_info_cloud.last_updated = static_cast<time_t>(savedata_node.attribute("last_updated").as_ullong());
    save_data_info_cloud.last_uploaded = static_cast<time_t>(savedata_node.attribute("last_uploaded").as_ullong());
    save_data_info_cloud.total_size = savedata_node.attribute("total_size").as_ullong();
    for (pugi::xml_node file_node : savedata_node.children("file")) {
        SaveEntry entry{
            .rel_path = file_node.attribute("path").as_string(),
            .last_updated = static_cast<time_t>(file_node.attribute("last_updated").as_ullong())
        };
        save_data_info_cloud.files.push_back(entry);
    }
    {
        std::lock_guard lock(emuenv.v3kn.storage_state.savedata_info_mutex);
        emuenv.v3kn.storage_state.savedata_info = std::move(savedata_info);
    }

    return true;
}

static bool create_zip_from_directory(const fs::path &directory, const fs::path &zip_path) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_writer_init_file(&zip, zip_path.string().c_str(), 0)) {
        LOG_ERROR("Failed to init zip writer");
        return false;
    }

    for (auto &entry : fs::recursive_directory_iterator(directory)) {
        if (!entry.is_regular_file())
            continue;

        fs::path file_path = entry.path();
        fs::path rel_path = fs::relative(file_path, directory);

        if (!mz_zip_writer_add_file(
                &zip,
                rel_path.generic_string().c_str(), // path in the zip
                file_path.string().c_str(), // source file
                nullptr,
                0,
                MZ_BEST_SPEED)) { // fast compression
            LOG_ERROR("Failed to add file {}", file_path.string());
            mz_zip_writer_end(&zip);
            return false;
        }
    }

    if (!mz_zip_writer_finalize_archive(&zip)) {
        LOG_ERROR("Failed to finalize zip");
        mz_zip_writer_end(&zip);
        return false;
    }

    mz_zip_writer_end(&zip);
    return true;
}

static bool extract_zip_to_directory(const fs::path &zip_path, const fs::path &output_dir) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_file(&zip, zip_path.string().c_str(), 0)) {
        LOG_ERROR("Failed to init zip reader");
        return false;
    }

    int file_count = (int)mz_zip_reader_get_num_files(&zip);
    const auto normalized_output = output_dir.lexically_normal();

    for (int i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
            LOG_ERROR("Failed to stat file {}", i);
            mz_zip_reader_end(&zip);
            return false;
        }

        std::string filename = stat.m_filename;
        std::replace(filename.begin(), filename.end(), '\\', '/');
        const auto entry_path = fs_utils::utf8_to_path(filename);
        const auto out_path = (normalized_output / entry_path).lexically_normal();
        const auto relative_path = out_path.lexically_relative(normalized_output);
        // Reject Windows drive paths and alternate data streams on every host platform.
        if (filename.find(':') != std::string::npos || entry_path.has_root_path() || relative_path.empty()
            || relative_path.has_root_path() || *relative_path.begin() == ".."
            || (!stat.m_is_directory && relative_path == ".")) {
            LOG_ERROR("Rejecting savedata ZIP entry escaping the output directory: {}", stat.m_filename);
            mz_zip_reader_end(&zip);
            return false;
        }

        if (stat.m_is_directory) {
            fs::create_directories(out_path);
            continue;
        }

        fs::create_directories(out_path.parent_path());

        if (!mz_zip_reader_extract_to_file(&zip, i, out_path.string().c_str(), 0)) {
            LOG_ERROR("Failed to extract {}", stat.m_filename);
            mz_zip_reader_end(&zip);
            return false;
        }
    }

    mz_zip_reader_end(&zip);
    return true;
}

static void reset_progress(StorageState &storage_state) {
    storage_state.progress = 0.f;
    storage_state.bytes_done = 0;
    storage_state.remaining = 0;
    storage_state.progress_done_timestamp = 0;
    storage_state.please_wait_timestamp = 0;
    storage_state.please_wait_done.store(false);
    storage_state.operation_finished.store(false);
    storage_state.progress_state.canceled = false;
}

void download_savedata(EmuEnvState &emuenv, const std::string &titleid, std::function<void(const Error &)> on_error) {
    const fs::path savedata_path = emuenv.vita_fs_path / "ux0/user" / emuenv.io.user_id / "savedata" / titleid;
    const fs::path savedata_cache_path = emuenv.cache_path / fmt::format("{}.psvimg", titleid);

    const auto savedata_info_cloud = [&] {
        std::lock_guard lock(emuenv.v3kn.storage_state.savedata_info_mutex);
        return emuenv.v3kn.storage_state.savedata_info[SAVE_DATA_TYPE_CLOUD];
    }();

    reset_progress(emuenv.v3kn.storage_state);

    const auto user_info = get_v3kn_user_info(emuenv);
    std::thread([&emuenv, titleid, savedata_path, savedata_cache_path, savedata_info_cloud, user_info, on_error = std::move(on_error)]() {
        auto &storage_state = emuenv.v3kn.storage_state;
        auto finish = [&storage_state] {
            if (storage_state.progress.load() >= 100.f) {
                // Wait for progress to reach 100% and display please wait dialog a minimum time before proceeding for a better UX
                {
                    std::unique_lock<std::mutex> lck(storage_state.mutex_progress);
                    storage_state.cv_progress.wait(lck, [&] {
                        return storage_state.please_wait_done.load();
                    });
                }
            }
            storage_state.operation_finished.store(true);
        };
        auto fail = [&finish, &on_error](const Error &error) {
            LOG_ERROR("V3KN storage: {}", error.message);
            if (on_error)
                on_error(error);
            finish();
        };

        auto progress_callback = [&storage_state](float updated_progress, uint64_t updated_remaining, uint64_t updated_bytes_done) {
            storage_state.progress = updated_progress;
            storage_state.bytes_done = updated_bytes_done;
            storage_state.remaining = updated_remaining;
            return &storage_state.progress_state;
        };

        const std::string url = get_v3kn_server_url(user_info.host, fmt::format("v3kn/download_file?type=savedata&id={}", titleid));

        try {
            if (fs::exists(savedata_cache_path))
                fs::remove(savedata_cache_path);

            const auto savedata_cache_str = fs_utils::path_to_utf8(savedata_cache_path);
            const auto res = net_utils::download_file_ex(url, savedata_cache_str, progress_callback, user_info.token);

            // Handle response and update connection status
            handle_v3kn_status(emuenv, res, &user_info);

            if (!res.is_success() || res.body.starts_with("ERR:")) {
                fail(get_v3kn_error(res));
                LOG_ERROR("Failed to download {}", savedata_cache_str);
                return;
            }

            // Verify downloaded file size
            const auto file_size = fs::file_size(savedata_cache_path);
            if ((file_size == 0) || (file_size != savedata_info_cloud.total_size)) {
                fs::remove(savedata_cache_path);
                fail({ .message = "Downloaded savedata size mismatch, please try again." });
                LOG_ERROR("Downloaded savedata size mismatch: expected {}, got {}", savedata_info_cloud.total_size, file_size);
                return;
            }

            // Extract savedata zip in a temp folder first to avoid partial overwrite if extraction fails
            const auto save_data_temp_path = fs::path{ savedata_path.parent_path() / (titleid + "_v3kn") };
            if (fs::exists(save_data_temp_path))
                fs::remove_all(save_data_temp_path);
            const auto ret = extract_zip_to_directory(savedata_cache_path, save_data_temp_path);
            fs::remove(savedata_cache_path);
            if (!ret) {
                fs::remove_all(save_data_temp_path);
                fail({ .message = "Failed to extract savedata zip, please try again." });
                return;
            }

            // Validate all files and timestamps before replacing the local save.
            uint64_t total_size_local = 0;
            for (const auto &entry : savedata_info_cloud.files) {
                std::string filename = entry.rel_path;
                std::replace(filename.begin(), filename.end(), '\\', '/');
                const auto entry_path = fs_utils::utf8_to_path(filename);
                const auto local_file_path = (save_data_temp_path / entry_path).lexically_normal();
                const auto relative_path = local_file_path.lexically_relative(save_data_temp_path.lexically_normal());
                if (filename.find(':') != std::string::npos || entry_path.has_root_path() || relative_path.empty()
                    || relative_path.has_root_path() || *relative_path.begin() == ".." || relative_path == ".") {
                    fail({ .message = "Invalid savedata file path in cloud metadata." });
                    return;
                }
                total_size_local += fs::file_size(local_file_path);
                fs::last_write_time(local_file_path, fs_utils::to_file_time(entry.last_updated));
            }
            fs::last_write_time(save_data_temp_path, fs_utils::to_file_time(savedata_info_cloud.last_updated));

            // Keep the original save until the replacement has been installed successfully.
            const auto backup_path = savedata_path.parent_path() / (titleid + "_v3kn_backup");
            if (fs::exists(backup_path)) {
                fail({ .message = fmt::format("A savedata recovery backup already exists at {}. Restore or move it before retrying.", fs_utils::path_to_utf8(backup_path)) });
                return;
            }
            const bool had_local_save = fs::exists(savedata_path);
            if (had_local_save)
                fs::rename(savedata_path, backup_path);

            // Rename temp folder to actual savedata folder
            std::error_code replace_error;
            fs::rename(save_data_temp_path, savedata_path, replace_error);
            if (replace_error) {
                if (had_local_save) {
                    std::error_code restore_error;
                    fs::rename(backup_path, savedata_path, restore_error);
                    if (restore_error) {
                        fail({ .message = fmt::format("Failed to replace savedata: {}. Restoring the original also failed: {}. The original save is preserved at {}.", replace_error.message(), restore_error.message(), fs_utils::path_to_utf8(backup_path)) });
                        return;
                    }
                }
                fail({ .message = fmt::format("Failed to replace savedata: {}. The original save has not been changed.", replace_error.message()) });
                return;
            }

            auto savedata_info_local = savedata_info_cloud;
            savedata_info_local.total_size = total_size_local;
            {
                std::lock_guard lock(storage_state.savedata_info_mutex);
                storage_state.savedata_info[SAVE_DATA_TYPE_LOCAL] = std::move(savedata_info_local);
            }

            // Remove existing savedata if extracted successfully
            if (had_local_save)
                fs::remove_all(backup_path);
        } catch (const fs::filesystem_error &error) {
            fail({ .message = fmt::format("Savedata download filesystem error: {}", error.what()) });
            return;
        }
        finish();
        LOG_INFO("Download successful for titleid {}", titleid);
    }).detach();
}

void upload_savedata(EmuEnvState &emuenv, const std::string &titleid, std::function<void(const Error &)> on_error) {
    const fs::path savedata_path = { emuenv.vita_fs_path / "ux0/user" / emuenv.io.user_id / "savedata" / titleid };
    if (!fs::exists(savedata_path) || fs::is_empty(savedata_path)) {
        LOG_WARN("No savedata to upload for titleid {}", titleid);
        return;
    }

    const fs::path savedata_cache_path{ emuenv.cache_path / fmt::format("{}.psvimg", titleid) };

    reset_progress(emuenv.v3kn.storage_state);

    const auto user_info = get_v3kn_user_info(emuenv);
    std::thread([&emuenv, titleid, savedata_path, savedata_cache_path, user_info, on_error = std::move(on_error)]() {
        auto &storage_state = emuenv.v3kn.storage_state;
        auto finish = [&storage_state] {
            if (storage_state.progress.load() >= 100.f) {
                // Wait for progress to reach 100% and display please wait dialog a minimum time before proceeding for a better UX
                {
                    std::unique_lock<std::mutex> lck(storage_state.mutex_progress);
                    storage_state.cv_progress.wait(lck, [&] {
                        return storage_state.please_wait_done.load();
                    });
                }
            }
            storage_state.operation_finished.store(true);
        };
        auto fail = [&finish, &on_error](const Error &error) {
            LOG_ERROR("V3KN storage: {}", error.message);
            if (on_error)
                on_error(error);
            finish();
        };

        auto progress_callback = [&storage_state](float updated_progress, uint64_t updated_remaining, uint64_t updated_bytes_done) {
            storage_state.progress = updated_progress;
            storage_state.bytes_done = updated_bytes_done;
            storage_state.remaining = updated_remaining;
            return &storage_state.progress_state;
        };

        auto savedata_info_local = [&] {
            std::lock_guard lock(storage_state.savedata_info_mutex);
            return storage_state.savedata_info[SAVE_DATA_TYPE_LOCAL];
        }();

        if (!create_zip_from_directory(savedata_path, savedata_cache_path)) {
            LOG_ERROR("Failed to create savedata zip");
            fail({ .message = "Failed to create savedata zip." });
            return;
        }

        const auto total_size = fs::file_size(savedata_cache_path);
        const auto last_uploaded = std::time(0);

        std::stringstream savedata_info_xml;
        pugi::xml_document doc;
        pugi::xml_node savedata_node = doc.append_child("savedata");
        for (const auto &entry : savedata_info_local.files) {
            auto file_node = savedata_node.append_child("file");
            file_node.append_attribute("path").set_value(entry.rel_path.c_str());
            file_node.append_attribute("last_updated").set_value(static_cast<unsigned long long>(entry.last_updated));
        }
        savedata_node.append_attribute("last_uploaded").set_value(static_cast<unsigned long long>(last_uploaded));
        savedata_node.append_attribute("last_updated").set_value(static_cast<unsigned long long>(savedata_info_local.last_updated));
        savedata_node.append_attribute("total_size").set_value(total_size);
        doc.save(savedata_info_xml);

        const std::string url = get_v3kn_server_url(user_info.host, fmt::format("v3kn/upload_file?type=savedata&id={}", titleid));

        const std::string filepath_str = fs_utils::path_to_utf8(savedata_cache_path);
        const auto res = net_utils::upload_file(url, filepath_str, user_info.token, savedata_info_xml.str(), progress_callback);
        // Handle response and update connection status
        handle_v3kn_status(emuenv, res, &user_info);

        fs::remove(savedata_cache_path);

        if (!res.is_success() || !res.body.starts_with("OK:")) {
            fail(get_v3kn_error(res));
            LOG_ERROR("Upload failed for {}", filepath_str);
            return;
        }

        std::smatch match;
        // Expected format: "OK:quota_used:quota_total"
        std::regex regex_pattern(R"(OK:(\d+):(\d+))");
        if (std::regex_search(res.body, match, regex_pattern) && match.size() == 3) {
            // Update user info of quota
            {
                auto &account_state = emuenv.v3kn.account_state;
                std::lock_guard lock(account_state.user_info_mutex);
                if (account_state.user_info.host == user_info.host && account_state.user_info.token == user_info.token) {
                    account_state.user_info.quota_used = std::strtoull(match[1].str().c_str(), nullptr, 10);
                    account_state.user_info.quota_total = std::strtoull(match[2].str().c_str(), nullptr, 10);
                }
            }

            // Update cloud savedata info
            savedata_info_local.last_uploaded = last_uploaded;
            auto savedata_info_cloud = savedata_info_local;
            savedata_info_cloud.total_size = total_size;
            {
                std::lock_guard lock(storage_state.savedata_info_mutex);
                storage_state.savedata_info[SAVE_DATA_TYPE_LOCAL] = std::move(savedata_info_local);
                storage_state.savedata_info[SAVE_DATA_TYPE_CLOUD] = std::move(savedata_info_cloud);
            }
            LOG_INFO("Upload successful for titleid {}", titleid);
        } else
            LOG_WARN("Unexpected upload response format: {}", res.body);

        finish();
    }).detach();
}

} // namespace v3kn
