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

namespace v3kn {

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
        if (fs::exists(save_data_temp_path))
            fs::remove_all(save_data_temp_path);
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
            emuenv.v3kn.account_state.user_info.quota_used = std::strtoull(match[1].str().c_str(), nullptr, 10);
            emuenv.v3kn.account_state.user_info.quota_total = std::strtoull(match[2].str().c_str(), nullptr, 10);

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
            gui.vita_area.live_area_screen = false;
        }
        gui.vita_area.connecting_please_wait = false;
    }).detach();
}

} // namespace v3kn
