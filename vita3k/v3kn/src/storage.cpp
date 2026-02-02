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

#include <config/state.h>
#include <dialog/state.h>
#include <emuenv/state.h>
#include <gui/state.h>
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
#include <ctime>
#include <optional>
#include <sstream>
#include <thread>
#include <unordered_set>

bool load_and_compute_trophy_stats(EmuEnvState &emuenv, np::trophy::Context &context, const std::string &np_com_id, TrophyProgressInfo &trophy_progress_info) {
    const auto TROPHY_PATH{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "trophy" };
    const auto trophy_data_np_com_id_path{ TROPHY_PATH / "data" / np_com_id };

    if (!fs::exists(trophy_data_np_com_id_path / "TROPUSR.DAT")) {
        LOG_WARN("Trophy TROPUSR.DAT not found for Np Com ID: {}.", np_com_id);
        return false;
    }

    const auto updated = fs::last_write_time(trophy_data_np_com_id_path / "TROPUSR.DAT");
    trophy_progress_info.last_updated = updated;

    const auto trophy_progress_save_file = device::construct_normalized_path(VitaIoDevice::ux0, "user/" + emuenv.io.user_id + "/trophy/data/" + np_com_id + "/TROPUSR.DAT");
    const auto progress_input_file = open_file(emuenv.io, trophy_progress_save_file.c_str(), SCE_O_RDONLY, emuenv.pref_path, "load_trophy_progress_file");

    context.io = &emuenv.io;
    if (!context.load_trophy_progress_file(progress_input_file)) {
        LOG_ERROR("Failed to load trophy progress for Np Com ID: {}", np_com_id);
        close_file(emuenv.io, progress_input_file, "load_trophy_progress_file");
        return false;
    }

    close_file(emuenv.io, progress_input_file, "load_trophy_progress_file");

    const auto unlocked_count = context.total_trophy_unlocked();
    if (unlocked_count == 0) {
        LOG_INFO("No trophies unlocked for Np Com ID: {}.", np_com_id);
        return true;
    }

    auto trophy_index = 0;
    for (uint32_t gid = 0; gid < context.group_count + 1; gid++) {
        if (context.trophy_count_by_group[gid]) {
            const std::string group_id = fmt::format("{:0>3d}", gid);
            for (uint32_t i = 0; i < context.trophy_count_by_group[gid]; i++, trophy_index++) {
                if (context.is_trophy_unlocked(trophy_index)) {
                    ++trophy_progress_info.unlocked_type_count[group_id][context.trophy_kinds[trophy_index]];
                    ++trophy_progress_info.unlocked_type_count["global"][context.trophy_kinds[trophy_index]];
                    const std::string trophy_id = fmt::format("{:03}", trophy_index);
                    trophy_progress_info.unlocked_ids[group_id].push_back(trophy_id);
                    trophy_progress_info.unlocked_ids["global"].push_back(trophy_id);
                }
            }
            const auto plat_count = trophy_progress_info.unlocked_type_count[group_id][np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM];
            const auto gold_count = trophy_progress_info.unlocked_type_count[group_id][np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD];
            const auto silver_count = trophy_progress_info.unlocked_type_count[group_id][np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER];
            const auto bronze_count = trophy_progress_info.unlocked_type_count[group_id][np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE];
            trophy_progress_info.progress[group_id] = (trophy_progress_info.unlocked_ids[group_id].size() * 100) / context.trophy_count_by_group[gid];
        }
    }

    const auto plat_count = trophy_progress_info.unlocked_type_count["global"][np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM];
    const auto gold_count = trophy_progress_info.unlocked_type_count["global"][np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD];
    const auto silver_count = trophy_progress_info.unlocked_type_count["global"][np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER];
    const auto bronze_count = trophy_progress_info.unlocked_type_count["global"][np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE];

    const auto progress = trophy_progress_info.unlocked_ids["global"].size() * 100 / context.trophy_count;
    trophy_progress_info.progress["global"] = progress;

    return true;
}

namespace v3kn {

// Trophy
static fs::path get_trophy_sync_state_path(const EmuEnvState &emuenv) {
    return emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "trophy_sync.xml";
}

constexpr uint32_t TROPHY_SYNC_VERSION = 2;
static bool load_trophy_sync_state(EmuEnvState &emuenv, time_t &last_sync, std::unordered_set<std::string> &pending_ids) {
    last_sync = 0;
    pending_ids.clear();

    const auto sync_path = get_trophy_sync_state_path(emuenv);
    if (!fs::exists(sync_path)) {
        LOG_WARN("Trophy sync state file does not exist: {}", sync_path.string());
        return false;
    }

    pugi::xml_document doc;
    if (!doc.load_file(sync_path.string().c_str())) {
        LOG_WARN("Failed to load trophy sync state file: {}", sync_path.string());
        return false;
    }

    const pugi::xml_node root = doc.child("trophy_sync");
    if (!root) {
        LOG_WARN("Invalid trophy sync state file: {}", sync_path.string());
        return false;
    }

    const auto version = root.attribute("version").as_uint();
    if (version != TROPHY_SYNC_VERSION) {
        LOG_WARN("Outdated trophy sync state, sync state will be reset: {}", sync_path.string());
        return false;
    }

    last_sync = static_cast<time_t>(root.attribute("last_sync").as_ullong(0));

    const pugi::xml_node pending = root.child("pending");
    for (const pugi::xml_node node : pending.children("npcom")) {
        const std::string id = node.attribute("id").as_string();
        if (!id.empty())
            pending_ids.insert(id);
    }

    return true;
}

static void save_trophy_sync_state(EmuEnvState &emuenv, time_t last_sync, const std::unordered_set<std::string> &pending_ids) {
    const auto sync_path = get_trophy_sync_state_path(emuenv);
    pugi::xml_document doc;
    pugi::xml_node root = doc.append_child("trophy_sync");
    root.append_attribute("version").set_value(TROPHY_SYNC_VERSION);
    root.append_attribute("last_sync").set_value(static_cast<unsigned long long>(last_sync));

    pugi::xml_node pending = root.append_child("pending");
    for (const auto &id : pending_ids) {
        pending.append_child("npcom").append_attribute("id").set_value(id.c_str());
    }

    doc.save_file(sync_path.string().c_str());
}

static bool load_local_trophy_sync(EmuEnvState &emuenv, const std::string &np_com_id, TrophySync &sync_info) {
    np::trophy::Context context{};
    TrophyProgressInfo trophy_progress_info{};
    if (!load_and_compute_trophy_stats(emuenv, context, np_com_id, trophy_progress_info))
        return false;

    sync_info.last_updated = trophy_progress_info.last_updated;
    sync_info.progress = trophy_progress_info.progress["global"];
    sync_info.unlocked_ids = trophy_progress_info.unlocked_ids["global"];
    sync_info.platinum = trophy_progress_info.unlocked_type_count["global"][np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM];
    sync_info.gold = trophy_progress_info.unlocked_type_count["global"][np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD];
    sync_info.silver = trophy_progress_info.unlocked_type_count["global"][np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER];
    sync_info.bronze = trophy_progress_info.unlocked_type_count["global"][np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE];

    return true;
}

static void collect_local_trophy_progress(EmuEnvState &emuenv, std::map<std::string, TrophySync> &trophy_progress) {
    const auto trophies_data_path = emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "trophy" / "data";
    if (!fs::exists(trophies_data_path) || fs::is_empty(trophies_data_path))
        return;

    for (const auto &entry : fs::directory_iterator(trophies_data_path)) {
        if (!entry.is_directory())
            continue;

        const auto np_com_id = entry.path().stem().generic_string();
        if (trophy_progress.contains(np_com_id))
            continue;

        TrophySync sync_info{};
        if (load_local_trophy_sync(emuenv, np_com_id, sync_info)) {
            if (sync_info.progress > 0)
                trophy_progress.emplace(np_com_id, sync_info);
        }
    }
}

static void update_trophy_sync_state_after_sync(EmuEnvState &emuenv, const std::unordered_set<std::string> &synced_ids) {
    time_t last_sync = 0;
    std::unordered_set<std::string> pending_ids;
    load_trophy_sync_state(emuenv, last_sync, pending_ids);

    if (synced_ids.empty()) {
        pending_ids.clear();
    } else {
        for (const auto &id : synced_ids)
            pending_ids.erase(id);
    }

    save_trophy_sync_state(emuenv, static_cast<time_t>(std::time(0)), pending_ids);
}

static void start_trophy_sync_internal(GuiState &gui, EmuEnvState &emuenv, std::map<std::string, TrophySync> &trophy_progress, std::atomic<float> &sync_progress,
    std::atomic<bool> &sync_cancel, std::vector<std::string> &sync_downloaded, const std::unordered_set<std::string> pending_ids = {}, time_t local_last_sync = 0) {
    std::thread([&, trophy_progress, pending_ids, local_last_sync]() mutable {
        const fs::path trophies_path{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "trophy" };

        const auto check_url = get_v3kn_server_url(emuenv.v3kn.account_state.user_info.host, "v3kn/check_trophy_conf_data");
        const auto check_res = net_utils::get_web_response_ex(check_url, emuenv.v3kn.account_state.user_info.token);
        if (!check_res.body.starts_with("ERR:") || check_res.body == "OK") {
            pugi::xml_document doc;
            const auto parse_result = doc.load_string(check_res.body.c_str());
            if (!parse_result) {
                LOG_WARN("Failed to parse trophy conf check response, response: {}, error: {}", check_res.body, parse_result.description());
            } else {
                // Create a reusable CURL session with keep-alive for multiple downloads
                auto curl_upload_session = net_utils::init_curl_upload_session(emuenv.v3kn.account_state.user_info.token);
                const auto missing_confs = doc.child("missing_confs");
                const auto missing_confs_count = std::distance(missing_confs.children("npcommid").begin(), missing_confs.children("npcommid").end());
                auto processed_conf = 0;
                for (const auto &trophy_node : missing_confs.children("npcommid")) {
                    // get trophy id from child
                    const std::string npcomm_id = trophy_node.text().as_string();
                    const auto upload_url = get_v3kn_server_url(emuenv.v3kn.account_state.user_info.host, fmt::format("v3kn/upload_trophy_conf_data?id={}", npcomm_id));
                    const auto trophy_conf_path = trophies_path / "conf" / npcomm_id;
                    if (!fs::exists(trophy_conf_path) || fs::is_empty(trophy_conf_path)) {
                        LOG_WARN("Trophy conf data missing for trophy ID: {}, expected at: {}", npcomm_id, trophy_conf_path.string());
                        processed_conf++;
                        continue;
                    }

                    for (const auto &entry : fs::directory_iterator(trophy_conf_path)) {
                        if (!entry.is_regular_file())
                            continue;

                        const auto trophy_file_path_str = fs_utils::path_to_utf8(entry.path());
                        const auto upload_res = net_utils::upload_file(upload_url, trophy_file_path_str, emuenv.v3kn.account_state.user_info.token, "", nullptr, &curl_upload_session);
                        if (upload_res.body.starts_with("ERR:")) {
                            LOG_WARN("Failed to upload trophy conf data for trophy ID: {}, response: {}", npcomm_id, upload_res.body);
                            break;
                        }
                    }
                    processed_conf++;
                    sync_progress = static_cast<float>(processed_conf) / static_cast<float>(missing_confs_count);
                }
                net_utils::cleanup_curl_session(curl_upload_session);
                sync_progress = 0.f;
            }
        } else {
            LOG_WARN("Failed to check trophy conf data, response: {}", check_res.body);
        }

        const auto url = get_v3kn_server_url(emuenv.v3kn.account_state.user_info.host, "v3kn/trophies_info");
        const auto res = net_utils::get_web_response_ex(url, emuenv.v3kn.account_state.user_info.token);

        // Handle response and update connection status
        handle_v3kn_status(emuenv, res);

        if (res.body.starts_with("ERR:")) {
            gui.info_message.title = emuenv.common_dialog.lang.common["error"];
            gui.info_message.level = spdlog::level::err;
            gui.info_message.msg = get_v3kn_error_message(emuenv, res);
            sync_cancel = true;
            return;
        }
        std::map<std::string, uint32_t> server_trophy_progress;
        std::map<std::string, time_t> server_trophy_last_updated;
        bool server_xml_incomplete = false;
        pugi::xml_document doc;
        pugi::xml_parse_result result{};
        time_t server_last_sync = 0;
        std::unordered_set<std::string> effective_filter;
        const bool filter_active = !pending_ids.empty() || (local_last_sync > 0);
        bool has_zero_progress = false;
        if (!pending_ids.empty())
            effective_filter = pending_ids;
        if (!res.body.starts_with("WARN:")) {
            result = doc.load_string(res.body.c_str());
            if (result) {
                pugi::xml_node trophies_node = doc.child("trophies");
                pugi::xml_attribute last_sync_attr = trophies_node.attribute("last_sync");
                if (!last_sync_attr) {
                    LOG_WARN("V3KN trophy sync: server XML missing last_sync attribute, treating as never synced");
                    server_xml_incomplete = true;
                } else
                    server_last_sync = static_cast<time_t>(trophies_node.attribute("last_sync").as_ullong());

                if (!trophies_node.child("trophy").empty()) {
                    for (auto trophy_node = trophies_node.child("trophy"); trophy_node;) {
                        auto next_node = trophy_node.next_sibling("trophy");
                        const std::string trophy_id = trophy_node.attribute("id").as_string();
                        const uint32_t progress = trophy_node.attribute("progress").as_uint();
                        if (progress > 0) {
                            const time_t last_updated = static_cast<time_t>(trophy_node.attribute("last_updated").as_ullong());
                            server_trophy_progress[trophy_id] = progress;
                            server_trophy_last_updated[trophy_id] = last_updated;
                        }
                        trophies_node.remove_child(trophy_node);
                        trophy_node = next_node;
                    }
                    server_xml_incomplete = true;
                } else if (!trophies_node.child("np").empty()) {
                    for (auto np_node = trophies_node.child("np"); np_node;) {
                        auto next_node = np_node.next_sibling("np");
                        const std::string np_com_id = np_node.attribute("commid").as_string();
                        const time_t last_updated = static_cast<time_t>(np_node.attribute("last_updated").as_ullong());
                        const auto progress_node = np_node.child("progress");
                        const uint32_t progress = progress_node.attribute("percent").as_uint();
                        if (progress == 0) {
                            server_xml_incomplete = true;
                            trophies_node.remove_child(np_node);
                            np_node = next_node;
                            continue;
                        }
                        server_trophy_progress[np_com_id] = progress;
                        server_trophy_last_updated[np_com_id] = last_updated;
                        np_node = next_node;
                    }
                } else {
                    LOG_WARN("V3KN trophy sync: server XML is outdated format, resetting and treating as never synced");
                    doc.reset();
                    result = {};
                    server_xml_incomplete = true;
                }
            } else {
                LOG_WARN("Failed to parse trophy info response: {}, recreating", result.description());
                doc.reset();
                result = {};
                server_xml_incomplete = true;
            }
        }

        if (server_xml_incomplete && trophy_progress.empty())
            collect_local_trophy_progress(emuenv, trophy_progress);

        if (filter_active && (server_last_sync > local_last_sync) && !server_trophy_last_updated.empty()) {
            for (const auto &[trophy_id, last_updated] : server_trophy_last_updated) {
                if (last_updated > local_last_sync)
                    effective_filter.insert(trophy_id);
            }
        }

        // After getting server trophy progress, compare and download/upload as needed
        const uint32_t total_trophies = static_cast<uint32_t>(trophy_progress.size() + server_trophy_progress.size());
        uint32_t processed_trophies = 0;
        std::map<std::string, time_t> download_trophy_progress{};
        std::vector<std::string> upload_trophy_progress{};
        const auto add_upload_id = [&](const std::string &npcomm_id) {
            if (std::find(upload_trophy_progress.begin(), upload_trophy_progress.end(), npcomm_id) == upload_trophy_progress.end())
                upload_trophy_progress.push_back(npcomm_id);
        };

        // Before download and upload, first determine what needs to be done with create 2 lists
        if (!server_trophy_progress.empty()) {
            for (const auto &[trophy_id, server_sync] : server_trophy_progress) {
                if (filter_active && !effective_filter.contains(trophy_id))
                    continue;
                const auto it = trophy_progress.find(trophy_id);
                if (it != trophy_progress.end()) {
                    const uint32_t local_progress = it->second.progress;
                    if (server_sync > local_progress) {
                        // Server has more progress, need to download
                        download_trophy_progress[trophy_id] = server_trophy_last_updated[trophy_id];
                    } else if (local_progress > server_sync) {
                        // Local has more progress, need to upload
                        add_upload_id(trophy_id);
                    }
                } else {
                    // Trophy not found locally, need to download
                    download_trophy_progress[trophy_id] = server_trophy_last_updated[trophy_id];
                }
            }
        }

        const bool resync_after_download = server_xml_incomplete && !download_trophy_progress.empty();
        const bool force_full_upload = server_xml_incomplete && download_trophy_progress.empty();
        LOG_INFO_IF(force_full_upload, "V3KN trophy sync: server XML is outdated or corrupted, forcing full upload");
        for (const auto &[trophy_id, local_sync] : trophy_progress) {
            if (!force_full_upload && filter_active && !effective_filter.contains(trophy_id))
                continue;
            if (force_full_upload || !server_trophy_progress.contains(trophy_id)) {
                // Trophy not found on server, or server XML missing trophy fields, need to upload
                add_upload_id(trophy_id);
            }
        }

        // Now combine the two lists to get total operations
        const uint32_t total_operations = static_cast<uint32_t>(download_trophy_progress.size() + upload_trophy_progress.size());

        // Create a reusable CURL session with keep-alive for multiple downloads
        auto curl_download_session = net_utils::init_curl_download_session(emuenv.v3kn.account_state.user_info.token);

        // Now perform downloads if needed
        for (const auto &[trophy_id, server_sync] : download_trophy_progress) {
            if (sync_cancel) {
                break;
            }
            const auto download_url = get_v3kn_server_url(emuenv.v3kn.account_state.user_info.host, fmt::format("v3kn/download_file?type=trophy&id={}", trophy_id));
            const auto trophy_file = trophies_path / "data" / trophy_id / "TROPUSR.DAT";
            const auto trophy_temp_file = trophy_file.parent_path() / "TROPUSR_v3kn_tmp.DAT";
            fs::create_directories(trophy_file.parent_path());
            const auto trophy_file_str = fs_utils::path_to_utf8(trophy_temp_file);
            const auto download_res = net_utils::download_file_ex(download_url, trophy_file_str, nullptr, emuenv.v3kn.account_state.user_info.token, &curl_download_session);
            if (download_res.body.starts_with("ERR:")) {
                net_utils::cleanup_curl_session(curl_download_session);
                gui.info_message.title = emuenv.common_dialog.lang.common["error"];
                gui.info_message.level = spdlog::level::err;
                gui.info_message.msg = get_v3kn_error_message(emuenv, download_res);
                sync_cancel = true;
                LOG_ERROR("Download failed for trophy {}, res: {}", trophy_id, download_res.body);
                return;
            }

            // Erase existing trophy file if any before moving temp file
            if (fs::exists(trophy_file))
                fs::remove(trophy_file);

            // Move the temporary file to the final location after successful download
            fs::rename(trophy_temp_file, trophy_file);

            // Update the file's last modified time to match server's last_updated
            fs::last_write_time(trophy_file, server_sync);

            // Update local trophy progress map
            sync_downloaded.push_back(trophy_id);

            // Update local trophy progress
            processed_trophies++;
            sync_progress = static_cast<float>(processed_trophies) / static_cast<float>(total_operations);
        }

        net_utils::cleanup_curl_session(curl_download_session);

        if (resync_after_download) {
            if (!filter_active) {
                LOG_WARN("V3KN trophy sync: server XML format is outdated, please sync again to rebuild it");
                gui.info_message.title = "Warning";
                gui.info_message.level = spdlog::level::warn;
                gui.info_message.msg = "Trophy sync needs to run again to rebuild outdated data.";
                sync_progress = 1.0f;
            } else {
                LOG_INFO("V3KN trophy sync: server XML missing trophy fields, resync needed after download");
                // Reset progress and start sync again with filter to only upload trophies that are relevant to downloaded ones
                sync_progress = 0.f;
                trophy_progress.clear();
                collect_local_trophy_progress(emuenv, trophy_progress);
                start_trophy_sync_internal(gui, emuenv, trophy_progress, sync_progress, sync_cancel, sync_downloaded);
            }
            return;
        }

        // If result of xml is null, create first trophies child
        if (!result)
            doc.append_child("trophies");

        // Create a reusable CURL session with keep-alive for multiple uploads
        auto curl_session = net_utils::init_curl_upload_session(emuenv.v3kn.account_state.user_info.token);

        // Now perform uploads if needed
        for (const auto &npcomm_id : upload_trophy_progress) {
            if (sync_cancel) {
                break;
            }

            const auto local_it = trophy_progress.find(npcomm_id);
            if (local_it == trophy_progress.end())
                continue;
            const auto &local_sync = local_it->second;

            const auto trophy_file_path = trophies_path / "data" / npcomm_id / "TROPUSR.DAT";
            const auto trophy_file_path_str = fs_utils::path_to_utf8(trophy_file_path);

            // Update the XML document progressively with each trophy
            std::stringstream trophies_info_xml;
            pugi::xml_node trophies_node = doc.child("trophies");

            // Update last_sync attribute on trophies, which indicates the last time any trophy was synced
            pugi::xml_attribute last_sync_attr = trophies_node.attribute("last_sync");
            if (!last_sync_attr)
                last_sync_attr = trophies_node.append_attribute("last_sync");
            last_sync_attr.set_value(static_cast<unsigned long long>(std::time(0)));

            pugi::xml_node npcomid_node = trophies_node.find_child_by_attribute("np", "commid", npcomm_id.c_str());
            if (!npcomid_node) {
                npcomid_node = trophies_node.append_child("np");
                npcomid_node.append_attribute("commid") = npcomm_id.c_str();
            }

            // Update last_updated with file's last modified time
            pugi::xml_attribute last_updated_attr = npcomid_node.attribute("last_updated");
            if (!last_updated_attr)
                last_updated_attr = npcomid_node.append_attribute("last_updated");
            last_updated_attr.set_value(static_cast<unsigned long long>(local_sync.last_updated));

            pugi::xml_node progress_node = npcomid_node.child("progress");
            if (!progress_node)
                progress_node = npcomid_node.append_child("progress");

            // Update unlocked_count attribute
            pugi::xml_attribute unlocked_count_attr = progress_node.attribute("unlocked_count");
            if (!unlocked_count_attr)
                unlocked_count_attr = progress_node.append_attribute("unlocked_count");
            unlocked_count_attr.set_value(local_sync.unlocked_ids.size());

            // Update platinum/silver/gold/bronze attributes with the count of each trophy type
            pugi::xml_attribute platinum_attr = progress_node.attribute("platinum");
            if (!platinum_attr)
                platinum_attr = progress_node.append_attribute("platinum");
            platinum_attr.set_value(local_sync.platinum);

            pugi::xml_attribute gold_attr = progress_node.attribute("gold");
            if (!gold_attr)
                gold_attr = progress_node.append_attribute("gold");
            gold_attr.set_value(local_sync.gold);

            pugi::xml_attribute silver_attr = progress_node.attribute("silver");
            if (!silver_attr)
                silver_attr = progress_node.append_attribute("silver");
            silver_attr.set_value(local_sync.silver);

            pugi::xml_attribute bronze_attr = progress_node.attribute("bronze");
            if (!bronze_attr)
                bronze_attr = progress_node.append_attribute("bronze");
            bronze_attr.set_value(local_sync.bronze);

            // Update percent attribute
            pugi::xml_attribute percent_attr = progress_node.attribute("percent");
            if (!percent_attr)
                percent_attr = progress_node.append_attribute("percent");
            percent_attr.set_value(local_sync.progress);

            // Update unlocked trophy ids
            pugi::xml_node unlocked_node = npcomid_node.child("unlocked");
            if (!unlocked_node)
                unlocked_node = npcomid_node.append_child("unlocked");
            for (const auto &trophy_id : local_sync.unlocked_ids) {
                pugi::xml_node trophy_node = unlocked_node.find_child_by_attribute("trophy", "id", trophy_id.c_str());
                if (!trophy_node) {
                    trophy_node = unlocked_node.append_child("trophy");
                    trophy_node.append_attribute("id").set_value(trophy_id.c_str());
                }
            }

            // Save the progressively updated XML to stringstream
            doc.save(trophies_info_xml);

            // Upload the trophy file with the progressively updated XML using the reusable session
            const auto upload_url = get_v3kn_server_url(emuenv.v3kn.account_state.user_info.host, fmt::format("v3kn/upload_file?type=trophy&id={}", npcomm_id));
            const auto upload_res = net_utils::upload_file(upload_url, trophy_file_path_str, emuenv.v3kn.account_state.user_info.token, trophies_info_xml.str(), nullptr, &curl_session);
            if (!upload_res.body.starts_with("OK:")) {
                net_utils::cleanup_curl_session(curl_session);
                gui.info_message.function = spdlog::level::err;
                gui.info_message.title = emuenv.common_dialog.lang.common["error"];
                gui.info_message.msg = get_v3kn_error_message(emuenv, upload_res);
                sync_cancel.store(true);
                LOG_ERROR("Upload failed for {}, res: {}", trophy_file_path_str, upload_res.body);
                return;
            }

            // Expected format: "OK:quota_used:quota_total"
            std::smatch match;
            std::regex regex_pattern(R"(OK:(\d+):(\d+))");
            if (std::regex_search(res.body, match, regex_pattern) && match.size() == 3) {
                // Update user info of quota
                emuenv.v3kn.account_state.user_info.quota_used = std::stoull(match[1].str());
                emuenv.v3kn.account_state.user_info.quota_total = std::stoull(match[2].str());
            }

            processed_trophies++;
            sync_progress = static_cast<float>(processed_trophies) / static_cast<float>(total_operations);
        }

        if (total_operations == 0)
            LOG_INFO("V3KN trophy sync: all trophy data already up-to-date");

        net_utils::cleanup_curl_session(curl_session);
        sync_progress = 1.0f;
        if (!sync_cancel.load())
            update_trophy_sync_state_after_sync(emuenv, pending_ids);
    }).detach();
}

void start_trophy_sync(GuiState &gui, EmuEnvState &emuenv, std::map<std::string, TrophySync> &trophy_progress, std::atomic<float> &sync_progress, std::atomic<bool> &sync_cancel, std::vector<std::string> &sync_downloaded) {
    start_trophy_sync_internal(gui, emuenv, trophy_progress, sync_progress, sync_cancel, sync_downloaded);
}

void start_trophy_auto_sync(GuiState &gui, EmuEnvState &emuenv) {
    time_t last_sync = 0;

    std::unordered_set<std::string> pending_ids;
    std::map<std::string, TrophySync> trophy_progress;
    if (!load_trophy_sync_state(emuenv, last_sync, pending_ids)) {
        LOG_INFO("V3KN trophy sync: no local sync state found, initializing from local trophies");
        collect_local_trophy_progress(emuenv, trophy_progress);
        for (const auto &entry : trophy_progress)
            pending_ids.insert(entry.first);
    } else {
        for (const auto &np_com_id : pending_ids) {
            TrophySync sync_info{};
            if (load_local_trophy_sync(emuenv, np_com_id, sync_info))
                trophy_progress.emplace(np_com_id, sync_info);
        }
    }

    if (trophy_progress.empty())
        return;

    static std::atomic<float> auto_sync_progress{ 0.0f };
    static std::atomic<bool> auto_sync_cancel{ false };
    static std::vector<std::string> auto_sync_downloaded;
    auto_sync_cancel.store(false);
    auto_sync_downloaded.clear();

    start_trophy_sync_internal(gui, emuenv, trophy_progress, auto_sync_progress, auto_sync_cancel, auto_sync_downloaded, pending_ids, last_sync);
}

void add_pending_trophy_sync(EmuEnvState &emuenv, const std::string &np_com_id) {
    time_t last_sync = 0;
    std::unordered_set<std::string> pending_ids;
    load_trophy_sync_state(emuenv, last_sync, pending_ids);
    if (pending_ids.insert(np_com_id).second)
        save_trophy_sync_state(emuenv, last_sync, pending_ids);
}

// Online Storage
static bool get_savedata_info(GuiState &gui, EmuEnvState &emuenv, const std::string &titleid) {
    emuenv.v3kn.storage_state.savedata_info.clear();

    // Get local savedata info
    const fs::path save_data_path = { emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "savedata" / titleid };
    if (fs::exists(save_data_path) && !fs::is_empty(save_data_path)) {
        auto &save_data_info_local = emuenv.v3kn.storage_state.savedata_info[SAVE_DATA_TYPE_LOCAL];
        uint64_t total_size_local = 0;
        time_t global_last_updated = 0;
        for (auto &file : fs::recursive_directory_iterator(save_data_path)) {
            if (!file.is_regular_file())
                continue;

            const auto file_size = fs::file_size(file.path());
            const auto last_updated = fs::last_write_time(file.path());
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

    const std::string url = get_v3kn_server_url(emuenv.v3kn.account_state.user_info.host, fmt::format("v3kn/save_info?titleid={}", titleid));
    const auto response = net_utils::get_web_response_ex(url, emuenv.v3kn.account_state.user_info.token);

    // Handle response and update connection status
    handle_v3kn_status(emuenv, response);

    if (response.body.starts_with("WARN:")) {
        LOG_WARN("V3KN get savedata info: server warning: {}", response.body);
        return true;
    } else if (response.body.starts_with("ERR:")) {
        gui.info_message.title = emuenv.common_dialog.lang.common["error"];
        gui.info_message.level = spdlog::level::err;
        gui.info_message.msg = get_v3kn_error_message(emuenv, response);
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

    auto &save_data_info_cloud = emuenv.v3kn.storage_state.savedata_info[SAVE_DATA_TYPE_CLOUD];
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

    for (int i = 0; i < file_count; i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
            LOG_ERROR("Failed to stat file {}", i);
            continue;
        }

        fs::path out_path = output_dir / stat.m_filename;

        if (stat.m_is_directory) {
            fs::create_directories(out_path);
            continue;
        }

        fs::create_directories(out_path.parent_path());

        if (!mz_zip_reader_extract_to_file(&zip, i, out_path.string().c_str(), 0)) {
            LOG_ERROR("Failed to extract {}", stat.m_filename);
            continue;
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
    storage_state.progress_state.canceled = false;
}

void download_savedata(GuiState &gui, EmuEnvState &emuenv, const std::string &titleid) {
    const fs::path savedata_path = emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "savedata" / titleid;
    const fs::path savedata_cache_path = emuenv.cache_path / fmt::format("{}.psvimg", titleid);

    const auto &savedata_info_cloud = emuenv.v3kn.storage_state.savedata_info[SAVE_DATA_TYPE_CLOUD];

    reset_progress(emuenv.v3kn.storage_state);

    std::thread([&gui, &emuenv, titleid, savedata_path, savedata_cache_path, savedata_info_cloud]() mutable {
        auto &storage_state = emuenv.v3kn.storage_state;

        auto progress_callback = [&storage_state](float updated_progress, uint64_t updated_remaining, uint64_t updated_bytes_done) {
            storage_state.progress = updated_progress;
            storage_state.bytes_done = updated_bytes_done;
            storage_state.remaining = updated_remaining;
            return &storage_state.progress_state;
        };

        const std::string url = get_v3kn_server_url(emuenv.v3kn.account_state.user_info.host, fmt::format("v3kn/download_file?type=savedata&id={}", titleid));

        if (fs::exists(savedata_cache_path))
            fs::remove(savedata_cache_path);

        const auto savedata_cache_str = fs_utils::path_to_utf8(savedata_cache_path);
        const auto res = net_utils::download_file_ex(url, savedata_cache_str, progress_callback, emuenv.v3kn.account_state.user_info.token);

        // Handle response and update connection status
        handle_v3kn_status(emuenv, res);

        // Wait for progress to reach 100% and display please wait dialog a minimum time before proceeding for a better UX
        {
            std::unique_lock<std::mutex> lck(storage_state.mutex_progress);
            storage_state.cv_progress.wait(lck, [&] {
                return storage_state.please_wait_done.load();
            });
        }

        if (res.body.starts_with("ERR:") || (res.curl_res != 0)) {
            gui.info_message.function = spdlog::level::err;
            gui.info_message.title = emuenv.common_dialog.lang.common["error"];
            gui.info_message.msg = get_v3kn_error_message(emuenv, res);
            gui.vita_area.please_wait = false;
            LOG_ERROR("Failed to download {}", savedata_cache_str);
            return;
        }

        // Verify downloaded file size
        const auto file_size = fs::file_size(savedata_cache_path);
        if ((file_size == 0) || (file_size != savedata_info_cloud.total_size)) {
            fs::remove(savedata_cache_path);
            gui.info_message.function = spdlog::level::err;
            gui.info_message.title = emuenv.common_dialog.lang.common["error"];
            gui.info_message.msg = "Downloaded savedata size mismatch, please try again.";
            gui.vita_area.please_wait = false;
            LOG_ERROR("Downloaded savedata size mismatch: expected {}, got {}", savedata_info_cloud.total_size, file_size);
            return;
        }

        // Extract savedata zip in a temp folder first to avoid partial overwrite if extraction fails
        const auto save_data_temp_path = fs::path{ savedata_path.parent_path() / (titleid + "_v3kn") };
        const auto ret = extract_zip_to_directory(savedata_cache_path, save_data_temp_path);
        fs::remove(savedata_cache_path);
        if (!ret) {
            fs::remove_all(save_data_temp_path);
            gui.info_message.function = spdlog::level::err;
            gui.info_message.title = emuenv.common_dialog.lang.common["error"];
            gui.info_message.msg = "Failed to extract savedata zip, please try again.";
            gui.vita_area.please_wait = false;
            LOG_ERROR("Failed to extract savedata zip for titleid {}", titleid);
            return;
        }

        // Remove existing savedata if extracted successfully
        if (fs::exists(savedata_path))
            fs::remove_all(savedata_path);

        // Rename temp folder to actual savedata folder
        fs::rename(save_data_temp_path, savedata_path);

        auto &savedata_info_local = emuenv.v3kn.storage_state.savedata_info[SAVE_DATA_TYPE_LOCAL];
        savedata_info_local = savedata_info_cloud;
        uint64_t total_size_local = 0;
        for (auto &entry : savedata_info_cloud.files) {
            const fs::path local_file_path = savedata_path / entry.rel_path;
            total_size_local += fs::file_size(local_file_path);
            fs::last_write_time(local_file_path, entry.last_updated);
        }

        savedata_info_local.total_size = total_size_local;
        fs::last_write_time(savedata_path, savedata_info_cloud.last_updated);
        gui.vita_area.please_wait = false;
        LOG_INFO("Download successful for titleid {}", titleid);
    }).detach();
}

void upload_savedata(GuiState &gui, EmuEnvState &emuenv, const std::string &titleid) {
    const fs::path savedata_path = { emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "savedata" / titleid };
    if (!fs::exists(savedata_path) || fs::is_empty(savedata_path)) {
        LOG_WARN("No savedata to upload for titleid {}", titleid);
        return;
    }

    const fs::path savedata_cache_path{ emuenv.cache_path / fmt::format("{}.psvimg", titleid) };

    reset_progress(emuenv.v3kn.storage_state);

    std::thread([&gui, &emuenv, titleid, savedata_path, savedata_cache_path]() mutable {
        auto &storage_state = emuenv.v3kn.storage_state;

        auto progress_callback = [&storage_state](float updated_progress, uint64_t updated_remaining, uint64_t updated_bytes_done) {
            storage_state.progress = updated_progress;
            storage_state.bytes_done = updated_bytes_done;
            storage_state.remaining = updated_remaining;
            return &storage_state.progress_state;
        };

        auto &savedata_info_local = emuenv.v3kn.storage_state.savedata_info[SAVE_DATA_TYPE_LOCAL];

        if (!create_zip_from_directory(savedata_path, savedata_cache_path)) {
            LOG_ERROR("Failed to create savedata zip");
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

        const std::string url = get_v3kn_server_url(emuenv.v3kn.account_state.user_info.host, fmt::format("v3kn/upload_file?type=savedata&id={}", titleid));

        const std::string filepath_str = fs_utils::path_to_utf8(savedata_cache_path);
        const auto res = net_utils::upload_file(url, filepath_str, emuenv.v3kn.account_state.user_info.token, savedata_info_xml.str(), progress_callback);
        // Handle response and update connection status
        handle_v3kn_status(emuenv, res);

        // Wait for progress to reach 100% and display please wait dialog a minimum time before proceeding for a better UX
        {
            std::unique_lock<std::mutex> lck(storage_state.mutex_progress);
            storage_state.cv_progress.wait(lck, [&] {
                return storage_state.please_wait_done.load();
            });
        }

        fs::remove(savedata_cache_path);

        if (!res.body.starts_with("OK:")) {
            gui.info_message.function = spdlog::level::err;
            gui.info_message.title = emuenv.common_dialog.lang.common["error"];
            gui.info_message.msg = get_v3kn_error_message(emuenv, res);
            gui.vita_area.please_wait = false;
            LOG_ERROR("Upload failed for {}", filepath_str);
            return;
        }

        std::smatch match;
        // Expected format: "OK:quota_used:quota_total"
        std::regex regex_pattern(R"(OK:(\d+):(\d+))");
        if (std::regex_search(res.body, match, regex_pattern) && match.size() == 3) {
            // Update user info of quota
            emuenv.v3kn.account_state.user_info.quota_used = std::stoull(match[1].str());
            emuenv.v3kn.account_state.user_info.quota_total = std::stoull(match[2].str());

            // Update cloud savedata info
            savedata_info_local.last_uploaded = last_uploaded;
            auto &savedata_info_cloud = emuenv.v3kn.storage_state.savedata_info[SAVE_DATA_TYPE_CLOUD];
            savedata_info_cloud = savedata_info_local;
            savedata_info_cloud.total_size = total_size;
            LOG_INFO("Upload successful for titleid {}", titleid);
        } else
            LOG_WARN("Unexpected upload response format: {}", res.body);

        gui.vita_area.please_wait = false;
    }).detach();
}

void open_online_storage(GuiState &gui, EmuEnvState &emuenv, const std::string &titleid) {
    gui.vita_area.connecting_please_wait = true;
    std::thread([&gui, &emuenv, titleid]() mutable {
        if (get_savedata_info(gui, emuenv, titleid)) {
            gui.vita_area.online_storage = true;
            gui.vita_area.home_screen = false;
        }
        gui.vita_area.connecting_please_wait = false;
    }).detach();
}

} // namespace v3kn
