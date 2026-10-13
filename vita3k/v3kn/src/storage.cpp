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
#include <ctime>
#include <optional>
#include <sstream>
#include <thread>
#include <unordered_set>
#include <utility>

namespace v3kn {

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
