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

#include <config/state.h>

#include <gui/functions.h>

#include <io/state.h>

#include <util/fs.h>
#include <util/hash.h>
#include <util/log.h>
#include <util/net_utils.h>
#include <util/safe_time.h>

#include <miniz.h>

#include <pugixml.hpp>

namespace gui {

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static std::string base64_encode(const std::string &input) {
    const unsigned char *data = reinterpret_cast<const unsigned char *>(input.data());
    size_t len = input.size();
    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        unsigned int val = data[i] << 16;
        if (i + 1 < len)
            val |= data[i + 1] << 8;
        if (i + 2 < len)
            val |= data[i + 2];

        out.push_back(b64_table[(val >> 18) & 0x3F]);
        out.push_back(b64_table[(val >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? b64_table[(val >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < len) ? b64_table[val & 0x3F] : '=');
    }

    return out;
}

static std::string get_v3kn_error_message(EmuEnvState &emuenv, const net_utils::WebResponse &response) {
    std::string error_msg;
    if (response.body.starts_with("ERR:") || !response.body.starts_with("OK:")) {
        if (response.body == "ERR:MissingToken")
            error_msg = emuenv.common_dialog.lang.common["must_sign_in_vita3k_network"];
        else if (response.body == "ERR:InvalidToken")
            error_msg = "V3KN get error Invalid token. Please log in again.";
        else if (response.body == "ERR:MissingTitleID")
            error_msg = "V3KN get error Missing title ID.";
        else if (response.body == "ERR:UserExists")
            error_msg = "V3KN create account error User already exists.";
        else if (response.body == "ERR:MissingPassword")
            error_msg = "V3KN get error Missing password.";
        else if (response.body == "ERR:InvalidNPID")
            error_msg = "V3KN create/login error Invalid NPID.";
        else if (response.body == "ERR:InvalidPassword")
            error_msg = "V3KN login/change password/delete error Invalid password.";
        else if (response.body == "ERR:ServerError")
            error_msg = "V3KN server error.";
        else if (response.body == "ERR:QuotaExceeded")
            error_msg = "V3KN error Quota exceeded.";
        else if (response.body == "ERR:UserNotFound")
            error_msg = "V3KN login error User not found.";
        else if (response.body == "ERR:InvalidType")
            error_msg = "V3KN error Invalid type of download file.";
        else if (response.body == "ERR:InvalidID")
            error_msg = "V3KN error Invalid ID of App.";
        else if (response.body == "ERR:FileNotFound")
            error_msg = "V3KN error File not found.";
        else if (response.body == "ERR:MissingNewPassword")
            error_msg = "V3KN change password error Missing new password.";
        else if (response.body == "ERR:MissingOldPassword")
            error_msg = "V3KN change password error Missing old password.";
        else if (response.body == "ERR:SamePassword")
            error_msg = "V3KN change password error New password cannot be the same as the old password.";
        else if (response.body.empty()) {
            if (response.curl_res == 3)
                error_msg = "V3KN get error Malformed URL, check server URL.";
            else if (response.curl_res == 6)
                error_msg = emuenv.common_dialog.lang.common["could_not_connect_internet"];
            else if (response.curl_res == 7)
                error_msg = emuenv.common_dialog.lang.common["could_not_connect_server"];
            else if ((response.curl_res == 55) || (response.curl_res == 56))
                error_msg = emuenv.common_dialog.lang.common["vita3k_network_connection_lost"];
            else
                error_msg = fmt::format("V3KN get Unexpected server error: {}", response.curl_res);
        } else {
            error_msg = fmt::format("V3KN: Unexpected server response: {}", response.body);
        }
    }

    return error_msg;
}

static const std::string V3KN_SERVER_URL = "https://np.vita3k.org";

struct UserInfo {
    std::string NPID;
    std::string password;
    std::string token;
    time_t created_at = 0;
    uint64_t quota_used = 0;
    uint64_t quota_total = 0;
};

UserInfo user_info;

static net_utils::WebResponse v3kn_create(const std::string &npid, const std::string &password) {
    const std::string url = fmt::format("{}/v3kn/create", V3KN_SERVER_URL);
    const std::string post_data = fmt::format("npid={}&password={}", npid, password);

    const auto res = net_utils::get_web_response_ex(url, "", post_data);

    return res;
}

static net_utils::WebResponse v3kn_delete(const std::string &password) {
    const std::string url = fmt::format("{}/v3kn/delete", V3KN_SERVER_URL);
    const std::string post_data = fmt::format("password={}", password);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);

    return res;
}

static net_utils::WebResponse v3kn_login(const std::string &npid, const std::string &password) {
    const std::string url = fmt::format("{}/v3kn/login", V3KN_SERVER_URL);
    const std::string post_data = fmt::format("npid={}&password={}", npid, password);

    const auto res = net_utils::get_web_response_ex(url, "", post_data);

    return res;
}

static net_utils::WebResponse v3kn_change_npid(const std::string &new_npid) {
    const std::string url = fmt::format("{}/v3kn/change_npid", V3KN_SERVER_URL);
    const std::string post_data = fmt::format("new_npid={}", new_npid);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);

    return res;
}

static net_utils::WebResponse v3kn_change_password(const std::string &old_password, const std::string &new_password) {
    const std::string url = fmt::format("{}/v3kn/change_password", V3KN_SERVER_URL);
    const std::string post_data = fmt::format("old_password={}&new_password={}", old_password, new_password);
    const auto res = net_utils::get_web_response_ex(url, user_info.token, post_data);

    return res;
}

bool init_v3kn_user_info(EmuEnvState &emuenv) {
    user_info = {}; // reset info
    const fs::path user_info_path{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "v3kn.xml" };
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(user_info_path.string().c_str());
    if (!result) {
        LOG_WARN("V3KN user info: failed to load v3kn.xml: {}", result.description());
        return false;
    }
    pugi::xml_node user_node = doc.child("user");
    if (!user_node) {
        LOG_WARN("V3KN user info: missing <user> node");
        return false;
    }
    const auto version = user_node.attribute("version").as_uint(1);
    if (version != 1) {
        LOG_WARN("V3KN user info: unsupported version {}", version);
        return false;
    }

    user_info.NPID = user_node.child("NPID").text().as_string();
    user_info.password = user_node.child("password").text().as_string();
    user_info.token = user_node.child("token").text().as_string();
    user_info.created_at = user_node.child("created_at").text().as_ullong(0);
    user_info.quota_used = user_node.child("quota_used").text().as_ullong(0);
    user_info.quota_total = user_node.child("quota_total").text().as_ullong(0);

    return true;
}

static void save_v3kn_user_info(EmuEnvState &emuenv) {
    const fs::path user_info_path{ emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "v3kn.xml" };
    pugi::xml_document doc;
    pugi::xml_node user_node = doc.append_child("user");
    user_node.append_attribute("version").set_value(1);
    user_node.append_child("NPID").text().set(user_info.NPID.c_str());
    user_node.append_child("password").text().set(user_info.password.c_str());
    user_node.append_child("token").text().set(user_info.token.c_str());
    user_node.append_child("created_at").text().set(user_info.created_at);
    user_node.append_child("quota_used").text().set(user_info.quota_used);
    user_node.append_child("quota_total").text().set(user_info.quota_total);
    doc.save_file(user_info_path.string().c_str());
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
                rel_path.string().c_str(), // chemin dans le zip
                file_path.string().c_str(), // fichier source
                nullptr,
                0,
                MZ_BEST_SPEED)) { // compression rapide
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

static std::atomic<float> progress(0);
static std::atomic<uint64_t> remaining(0);
static std::atomic<uint64_t> bytes_done(0);
static net_utils::ProgressState progress_state{};

enum SaveDataType {
    SAVE_DATA_TYPE_CLOUD = 0,
    SAVE_DATA_TYPE_LOCAL = 1,
};

struct SaveEntry {
    std::string rel_path;
    time_t last_updated;
};

struct SaveDataInfo {
    time_t last_updated;
    time_t last_uploaded;
    uint64_t total_size;
    std::vector<SaveEntry> files;
};

static std::map<SaveDataType, SaveDataInfo> savedata_info{};

static bool get_savedata_info(GuiState &gui, EmuEnvState &emuenv, const std::string &titleid) {
    savedata_info.clear();

    // Get local savedata info
    const fs::path save_data_path = { emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "savedata" / titleid };
    if (fs::exists(save_data_path) && !fs::is_empty(save_data_path)) {
        auto &save_data_info_local = savedata_info[SAVE_DATA_TYPE_LOCAL];
        uint64_t total_size_local = 0;
        time_t global_last_updated = 0;
        for (auto &file : fs::recursive_directory_iterator(save_data_path)) {
            if (!file.is_regular_file())
                continue;

            const auto file_size = fs::file_size(file.path());
            const auto last_updated = fs::last_write_time(file.path());
            SaveEntry entry{
                .rel_path = fs::relative(file.path(), save_data_path).string(),
                .last_updated = last_updated
            };
            save_data_info_local.files.push_back(entry);
            global_last_updated = std::max(global_last_updated, last_updated);
            total_size_local += file_size;
        }
        save_data_info_local.last_updated = global_last_updated;
        save_data_info_local.total_size = total_size_local;
    }

    const std::string url = fmt::format("{}/v3kn/save_info?titleid={}", V3KN_SERVER_URL, titleid);
    const auto response = net_utils::get_web_response_ex(url, user_info.token);
    if (response.body.starts_with("WARN:")) {
        LOG_WARN("V3KN get savedata info: server warning: {}", response.body);
        return true;
    } else if (response.body.starts_with("ERR:") || !response.body.starts_with("OK:")) {
        gui.info_message.title = emuenv.common_dialog.lang.common["error"];
        gui.info_message.level = spdlog::level::err;
        gui.info_message.msg = get_v3kn_error_message(emuenv, response);
        return false;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(response.body.substr(3).c_str());
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
    save_data_info_cloud.last_updated = savedata_node.attribute("last_updated").as_ullong();
    save_data_info_cloud.last_uploaded = savedata_node.attribute("last_uploaded").as_ullong();
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

static const auto progress_callback(float updated_progress, uint64_t updated_remaining, uint64_t updated_bytes_done) {
    progress = updated_progress;
    bytes_done = updated_bytes_done;
    remaining = updated_remaining;

    return &progress_state;
};

static uint64_t progress_done_timestamp = 0;
static uint64_t please_wait_timestamp = 0;
static std::atomic<bool> please_wait_done(false);
static std::mutex mutex_progress;
static std::condition_variable cv_progress;

static void reset_progress() {
    progress = 0.f;
    bytes_done = 0;
    remaining = 0;
    progress_done_timestamp = 0;
    please_wait_timestamp = 0;
    please_wait_done.store(false);
    progress_state.canceled = false;
}

enum CloudDialogState {
    CLOUD_DIALOG_NONE,
    CLOUD_DIALOG_UPLOAD,
    CLOUD_DIALOG_DOWNLOAD,
    CLOUD_DIALOG_COPYING,
};

static CloudDialogState cloud_dialog_state = CLOUD_DIALOG_NONE;

static void download_savedata(GuiState &gui, EmuEnvState &emuenv, const std::string &titleid) {
    const fs::path savedata_path = emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "savedata" / titleid;
    const fs::path savedata_cache_path = emuenv.cache_path / fmt::format("{}.psvimg", titleid);

    const auto &savedata_info_cloud = savedata_info[SAVE_DATA_TYPE_CLOUD];

    reset_progress();

    std::thread([&gui, &emuenv, titleid, savedata_path, savedata_cache_path, savedata_info_cloud]() mutable {
        const std::string url = fmt::format("{}/v3kn/download_file?type=savedata&id={}", V3KN_SERVER_URL, titleid);

        if (fs::exists(savedata_cache_path))
            fs::remove(savedata_cache_path);

        const auto savedata_cache_str = fs_utils::path_to_utf8(savedata_cache_path);
        const auto res = net_utils::download_file_ex(url, savedata_cache_str, progress_callback, user_info.token);

        // Wait for progress to reach 100% and display please wait dialog a minimum time before proceeding for a better UX
        {
            std::unique_lock<std::mutex> lck(mutex_progress);
            cv_progress.wait(lck, [&] {
                return please_wait_done.load();
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

        if (fs::exists(savedata_path))
            fs::remove_all(savedata_path);

        const auto ret = extract_zip_to_directory(savedata_cache_path, savedata_path);
        fs::remove(savedata_cache_path);
        if (!ret) {
            gui.info_message.function = spdlog::level::err;
            gui.info_message.title = emuenv.common_dialog.lang.common["error"];
            gui.info_message.msg = "Failed to extract savedata zip, please try again.";
            gui.vita_area.please_wait = false;
            LOG_ERROR("Failed to extract savedata zip for titleid {}", titleid);
            return;
        }

        auto &savedata_info_local = savedata_info[SAVE_DATA_TYPE_LOCAL];
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

static void upload_savedata(GuiState &gui, EmuEnvState &emuenv, const std::string &titleid) {
    const fs::path savedata_path = { emuenv.pref_path / "ux0/user" / emuenv.io.user_id / "savedata" / titleid };
    if (!fs::exists(savedata_path) || fs::is_empty(savedata_path)) {
        LOG_WARN("No savedata to upload for titleid {}", titleid);
        return;
    }

    const fs::path savedata_cache_path{ emuenv.cache_path / fmt::format("{}.psvimg", titleid) };

    reset_progress();

    std::thread([&gui, &emuenv, titleid, savedata_path, savedata_cache_path]() mutable {
        auto &savedata_info_local = savedata_info[SAVE_DATA_TYPE_LOCAL];

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
        savedata_node.append_attribute("last_uploaded").set_value(last_uploaded);
        savedata_node.append_attribute("last_updated").set_value(savedata_info_local.last_updated);
        savedata_node.append_attribute("total_size").set_value(total_size);
        doc.save(savedata_info_xml);

        const std::string url = fmt::format("{}/v3kn/upload_file?type=savedata&id={}", V3KN_SERVER_URL, titleid);

        const std::string filepath_str = fs_utils::path_to_utf8(savedata_cache_path);
        const auto res = net_utils::upload_file(url, filepath_str, user_info.token, base64_encode(savedata_info_xml.str()), progress_callback);

        // Wait for progress to reach 100% and display please wait dialog a minimum time before proceeding for a better UX
        {
            std::unique_lock<std::mutex> lck(mutex_progress);
            cv_progress.wait(lck, [&] {
                return please_wait_done.load();
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
            user_info.quota_used = std::stoull(match[1].str());
            user_info.quota_total = std::stoull(match[2].str());
            save_v3kn_user_info(emuenv);

            // Update cloud savedata info
            savedata_info_local.last_uploaded = last_uploaded;
            auto &savedata_info_cloud = savedata_info[SAVE_DATA_TYPE_CLOUD];
            savedata_info_cloud = savedata_info_local;
            savedata_info_cloud.total_size = total_size;
            LOG_INFO("Upload successful for titleid {}", titleid);
        } else
            LOG_WARN("Unexpected upload response format: {}", res.body);

        gui.vita_area.please_wait = false;
    }).detach();
}

static std::string get_remaining_str(LangState &lang, const uint64_t remaining) {
    if (remaining > 60)
        return fmt::format(fmt::runtime(lang.online_storage["minutes_left"]), remaining / 60);
    else
        return fmt::format(fmt::runtime(lang.online_storage["seconds_left"]), remaining);
}

void draw_cloud_save(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    const auto INDICATOR_SIZE = ImVec2(VIEWPORT_SIZE.x, 32.f * SCALE.y);
    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INDICATOR_SIZE.y);
    const ImVec2 WINDOW_POS(VIEWPORT_POS.x, VIEWPORT_POS.y + INDICATOR_SIZE.y);

    const auto &app_path = emuenv.cfg.show_live_area_screen && (gui.live_area_app_current_open >= 0) ? gui.live_area_current_open_apps_list[gui.live_area_app_current_open] : emuenv.app_path;
    const auto APP_INDEX = get_app_index(gui, app_path);
    const auto &app_titleid = APP_INDEX->title_id;
    const auto &app_title = APP_INDEX->title;
    const auto &app_icon = gui.app_selector.user_apps_icon[app_path];

    auto &lang = gui.lang.online_storage;
    auto &common_lang = emuenv.common_dialog.lang.common;
    const auto is_12_hour_format = emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always);
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::Begin("##cloud_save", &gui.vita_area.cloud_save, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);

    const auto &draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(ImVec2(VIEWPORT_POS.x, VIEWPORT_POS.y + INDICATOR_SIZE.y), ImVec2(VIEWPORT_POS.x + VIEWPORT_SIZE.x, VIEWPORT_POS.y + VIEWPORT_SIZE.y + INDICATOR_SIZE.y), IM_COL32(0, 55, 145, 255));

    const ImVec2 CLOSE_BUTTON_SIZE(46.f * SCALE.x, 46.f * SCALE.y);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.f * SCALE.x);

    ImGui::SetCursorPos(ImVec2(10.f * SCALE.x, 10.f * SCALE.y));
    if (ImGui::Button("X", CLOSE_BUTTON_SIZE)) {
        gui.vita_area.cloud_save = false;
        if (emuenv.cfg.show_live_area_screen)
            gui.vita_area.live_area_screen = true;
        else
            gui.vita_area.home_screen = true;
    }
    ImGui::PopStyleVar();

    ImGui::SetCursorPosX((WINDOW_SIZE.x - CLOSE_BUTTON_SIZE.x) / 2.f);
    ImGui::SetWindowFontScale(1.85f * RES_SCALE.y);
    const std::string window_title = lang["online_storage"];
    const auto window_title_width = ImGui::CalcTextSize(window_title.c_str()).x;

    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x - window_title_width) / 2.f, ((64.f * SCALE.y) - ImGui::GetFontSize()) / 2.f));
    ImGui::Text("%s", window_title.c_str());
    const auto SEPARATOR_CLOUD_POS_Y = 64.f * SCALE.y;
    const auto LOCATION_SECTION_SIZE_Y = 92.f * SCALE.y;
    ImGui::SetCursorPos(ImVec2(0.f, SEPARATOR_CLOUD_POS_Y));
    ImGui::Separator();
    ImGui::SetCursorPos(ImVec2(54.f * SCALE.x, SEPARATOR_CLOUD_POS_Y + ((LOCATION_SECTION_SIZE_Y - ImGui::GetFontSize()) / 2.f)));

    ImGui::Text("Cloud");

    const auto has_cloud_save = savedata_info.contains(SAVE_DATA_TYPE_CLOUD);
    const auto has_local_save = savedata_info.contains(SAVE_DATA_TYPE_LOCAL);

    const auto begin_text_pos_x = 182.f * SCALE.x;

    ImGui::SetWindowFontScale(1.68f * RES_SCALE.y);
    const auto app_title_height_size = ImGui::CalcTextSize(app_title.c_str()).y;
    if (has_cloud_save) {
        ImGui::SetCursorPos(ImVec2(begin_text_pos_x, SEPARATOR_CLOUD_POS_Y + (LOCATION_SECTION_SIZE_Y / 2.f) - app_title_height_size));
        ImGui::Text("%s", app_title.c_str());
        const auto &cloud_info = savedata_info[SAVE_DATA_TYPE_CLOUD];
        ImGui::SetCursorPos(ImVec2(begin_text_pos_x, SEPARATOR_CLOUD_POS_Y + (LOCATION_SECTION_SIZE_Y / 2.f) + (10.f * SCALE.y)));
        tm updated_at_tm = {};
        SAFE_LOCALTIME(&cloud_info.last_updated, &updated_at_tm);
        auto UPDATED_AT = get_date_time(gui, emuenv, updated_at_tm);
        ImGui::SetWindowFontScale(1.12f * RES_SCALE.y);
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", fmt::format("{} {} {} {}", UPDATED_AT[DateTime::DATE_MINI], UPDATED_AT[DateTime::CLOCK], is_12_hour_format ? UPDATED_AT[DateTime::DAY_MOMENT] : "", get_unit_size(cloud_info.total_size)).c_str());
    } else {
        ImGui::SetCursorPos(ImVec2(begin_text_pos_x, SEPARATOR_CLOUD_POS_Y + ((LOCATION_SECTION_SIZE_Y - ImGui::GetFontSize()) / 2.f)));
        ImGui::Text("-");
    }

    const auto SEPARATOR_BUTTON_POS_Y = SEPARATOR_CLOUD_POS_Y + LOCATION_SECTION_SIZE_Y;

    ImGui::SetCursorPos(ImVec2(0.f, SEPARATOR_BUTTON_POS_Y));
    ImGui::Separator();

    const auto BUTTONS_SECTION_SIZE_Y = 184.f * SCALE.y;
    const auto BUTTONS_SEPARATION = 40.f * SCALE.x;

    ImGui::SetWindowFontScale(1.06f * RES_SCALE.y);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0.08f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.12f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.18f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 15.f * SCALE.x);
    const ImVec2 CLOUD_BUTTON_SIZE(284.f * SCALE.x, 116.f * SCALE.y);
    const auto BUTTONS_POS_Y = SEPARATOR_BUTTON_POS_Y + ((BUTTONS_SECTION_SIZE_Y - CLOUD_BUTTON_SIZE.y) / 2.f);
    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) - CLOUD_BUTTON_SIZE.x - (BUTTONS_SEPARATION / 2.f), BUTTONS_POS_Y));
    ImGui::BeginDisabled(!has_local_save);
    if (ImGui::Button(lang["upload"].c_str(), CLOUD_BUTTON_SIZE)) {
        if (has_cloud_save) {
            cloud_dialog_state = CLOUD_DIALOG_UPLOAD;
        } else {
            cloud_dialog_state = CLOUD_DIALOG_COPYING;
            upload_savedata(gui, emuenv, app_titleid);
        }
    }
    ImGui::EndDisabled();
    ImGui::SetCursorPos(ImVec2((WINDOW_SIZE.x / 2.f) + (BUTTONS_SEPARATION / 2.f), BUTTONS_POS_Y));
    ImGui::BeginDisabled(!has_cloud_save);
    if (ImGui::Button(lang["download"].c_str(), CLOUD_BUTTON_SIZE)) {
        if (has_local_save) {
            cloud_dialog_state = CLOUD_DIALOG_DOWNLOAD;
        } else {
            cloud_dialog_state = CLOUD_DIALOG_COPYING;
            download_savedata(gui, emuenv, app_titleid);
        }
    }
    ImGui::EndDisabled();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    static const auto get_timestamp_ms = []() {
        return duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    };

    if (cloud_dialog_state != CLOUD_DIALOG_NONE) {
        const ImVec2 OVERLAY_SIZE(VIEWPORT_SIZE.x, VIEWPORT_SIZE.y - INDICATOR_SIZE.y);
        const ImVec2 OVERLAY_POS(VIEWPORT_POS.x, VIEWPORT_POS.y + INDICATOR_SIZE.y);
        ImGui::SetNextWindowPos(OVERLAY_POS, ImGuiCond_Always);
        ImGui::SetNextWindowSize(OVERLAY_SIZE, ImGuiCond_Always);
        ImGui::Begin("##cloud_save_overlay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        const auto &window_draw_list = ImGui::GetWindowDrawList();

        switch (cloud_dialog_state) {
        case CLOUD_DIALOG_UPLOAD:
        case CLOUD_DIALOG_DOWNLOAD: {
            const ImVec2 POPUP_SIZE(868.f * SCALE.x, 478.f * SCALE.y);
            const ImVec2 POPUP_POS(VIEWPORT_POS.x + ((VIEWPORT_SIZE.x - POPUP_SIZE.x) / 2.f), VIEWPORT_POS.y + ((VIEWPORT_SIZE.y - POPUP_SIZE.y) / 2.f));
            ImGui::SetNextWindowPos(POPUP_POS, ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 15.f * SCALE.x);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
            ImGui::BeginChild("##confirm_popup", POPUP_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));

            const ImVec2 ICON_SIZE(76.f * SCALE.x, 76.f * SCALE.y);
            const ImVec2 ICON_POS(50.f * SCALE.x, 26.f * SCALE.y);
            if (app_icon) {
                const ImVec2 ICON_POS_MIN(POPUP_POS.x + ICON_POS.x, POPUP_POS.y + ICON_POS.y);
                const ImVec2 ICON_POS_MAX(ICON_POS_MIN.x + ICON_SIZE.x, ICON_POS_MIN.y + ICON_SIZE.y);
                window_draw_list->AddImageRounded(app_icon, ICON_POS_MIN, ICON_POS_MAX, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, ICON_SIZE.x);
            }
            ImGui::SetWindowFontScale(1.52f * RES_SCALE.y);
            ImGui::SetCursorPos(ImVec2(ICON_POS.x + ICON_SIZE.x + (22.f * SCALE.x), ICON_POS.y + ((ICON_SIZE.y - app_title_height_size) / 2.f)));
            ImGui::Text("%s", app_title.c_str());

            const bool is_upload = (cloud_dialog_state == CLOUD_DIALOG_UPLOAD);

            const auto &src_save = is_upload ? savedata_info[SAVE_DATA_TYPE_LOCAL] : savedata_info[SAVE_DATA_TYPE_CLOUD];
            const auto &dst_save = is_upload ? savedata_info[SAVE_DATA_TYPE_CLOUD] : savedata_info[SAVE_DATA_TYPE_LOCAL];

            const auto &current_ps_system = emuenv.cfg.pstv_mode ? lang["ps_tv_system"] : lang["ps_vita_system"];
            const auto &src_title = is_upload ? current_ps_system : lang["online_storage"];
            const auto &dst_title = is_upload ? lang["online_storage"] : current_ps_system;

            const auto draw_save_info = [&](const ImVec2 &begin_pos, const std::string &title, const SaveDataInfo &save_info) {
                ImGui::SetCursorPos(begin_pos);
                ImGui::SetWindowFontScale(1.1f * RES_SCALE.y);
                ImGui::Text("%s", title.c_str());
                ImGui::SetCursorPos(ImVec2(begin_pos.x, ImGui::GetCursorPosY() + (15.f * SCALE.y)));
                tm updated_at_tm = {};
                SAFE_LOCALTIME(&save_info.last_updated, &updated_at_tm);
                auto UPDATED_AT = get_date_time(gui, emuenv, updated_at_tm);
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", fmt::format("{} {} {}", UPDATED_AT[DateTime::DATE_MINI], UPDATED_AT[DateTime::CLOCK], is_12_hour_format ? UPDATED_AT[DateTime::DAY_MOMENT] : "").c_str());
                ImGui::SetCursorPos(ImVec2(begin_pos.x, ImGui::GetCursorPosY() + (5.f * SCALE.y)));
                ImGui::TextColored(GUI_COLOR_TEXT, "%s", get_unit_size(save_info.total_size).c_str());
            };

            const float INFO_START_Y = ICON_POS.y + ICON_SIZE.y + (22.f * SCALE.y);

            // Draw source save info
            const auto SRC_POS_X = 50.f * SCALE.x;
            draw_save_info(ImVec2(SRC_POS_X, INFO_START_Y), src_title, src_save);

            // Draw arrow
            const auto ARROW_COLOR = IM_COL32(161, 162, 165, 255);
            const auto ARROW_BAR_SIZE = ImVec2(42.f * SCALE.x, 8.f * SCALE.y);
            const auto ARROW_HEAD_SIZE = ImVec2(12.f * SCALE.x, 18.f * SCALE.y);
            const ImVec2 ARROW_BAR_POS_MIN(POPUP_POS.x + (POPUP_SIZE.x / 2.f) - (5.f * SCALE.x) - ARROW_BAR_SIZE.x - ARROW_HEAD_SIZE.x, POPUP_POS.y + (132.f * SCALE.y));
            const ImVec2 ARROW_BAR_POS_MAX(ARROW_BAR_POS_MIN.x + ARROW_BAR_SIZE.x, ARROW_BAR_POS_MIN.y + ARROW_BAR_SIZE.y);
            window_draw_list->AddRectFilled(ARROW_BAR_POS_MIN, ARROW_BAR_POS_MAX, ARROW_COLOR);
            const ImVec2 ARROW_HEAD_POS1(ARROW_BAR_POS_MAX.x, ARROW_BAR_POS_MIN.y - ((ARROW_HEAD_SIZE.y - ARROW_BAR_SIZE.y) / 2.f));
            const ImVec2 ARROW_HEAD_POS2(ARROW_BAR_POS_MAX.x + ARROW_HEAD_SIZE.x, ARROW_BAR_POS_MIN.y + (ARROW_BAR_SIZE.y / 2.f));
            const ImVec2 ARROW_HEAD_POS3(ARROW_BAR_POS_MAX.x, ARROW_BAR_POS_MAX.y + ((ARROW_HEAD_SIZE.y - ARROW_BAR_SIZE.y) / 2.f));
            window_draw_list->AddTriangleFilled(ARROW_HEAD_POS1, ARROW_HEAD_POS2, ARROW_HEAD_POS3, ARROW_COLOR);

            // Draw destination save info
            const auto DST_POS_X = (POPUP_SIZE.x / 2.f) + 42.f * SCALE.x;
            draw_save_info(ImVec2(DST_POS_X, INFO_START_Y), dst_title, dst_save);

            ImGui::SetCursorPosY(215.f * SCALE.y);
            ImGui::Separator();
            ImGui::SetWindowFontScale(1.38f * RES_SCALE.y);
            ImGui::SetCursorPos(ImVec2(60.f * SCALE.x, ImGui::GetCursorPosY() + (14.f * SCALE.y)));
            ImGui::Text("%s", lang["overwrite_saved_data"].c_str());
            const auto CONFIRM_BUTTON_SIZE = ImVec2(310.f * SCALE.x, 46.f * SCALE.y);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 15.f * SCALE.x);
            const auto BUTTON_SPACING = 40.f * SCALE.x;
            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) - CONFIRM_BUTTON_SIZE.x - (BUTTON_SPACING / 2.f), POPUP_SIZE.y - CONFIRM_BUTTON_SIZE.y - (24.f * SCALE.y)));
            if (ImGui::Button(common_lang["no"].c_str(), CONFIRM_BUTTON_SIZE)) {
                cloud_dialog_state = CLOUD_DIALOG_NONE;
            }
            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x / 2.f) + (BUTTON_SPACING / 2.f), POPUP_SIZE.y - CONFIRM_BUTTON_SIZE.y - (24.f * SCALE.y)));
            if (ImGui::Button(common_lang["yes"].c_str(), CONFIRM_BUTTON_SIZE)) {
                if (cloud_dialog_state == CLOUD_DIALOG_UPLOAD)
                    upload_savedata(gui, emuenv, app_titleid);
                else
                    download_savedata(gui, emuenv, app_titleid);
                cloud_dialog_state = CLOUD_DIALOG_COPYING;
            }
            ImGui::PopStyleVar(2);
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            break;
        }
        case CLOUD_DIALOG_COPYING: {
            const ImVec2 ICON_SIZE(64.f * SCALE.x, 64.f * SCALE.y);
            const ImVec2 POPUP_SIZE(760 * SCALE.x, 436.f * SCALE.y);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 15.f * SCALE.x);
            const auto PUPUP_POS = ImVec2(VIEWPORT_POS.x + ((VIEWPORT_SIZE.x - POPUP_SIZE.x) / 2.f), VIEWPORT_POS.y + ((VIEWPORT_SIZE.y - POPUP_SIZE.y) / 2.f));
            ImGui::SetNextWindowPos(PUPUP_POS, ImGuiCond_Always);
            ImGui::BeginChild("##copying_popup", POPUP_SIZE, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
            const auto &copy_str = lang["copying"];
            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x - ImGui::CalcTextSize(copy_str.c_str()).x) / 2.f, 82.f * SCALE.y));
            ImGui::Text("%s", copy_str.c_str());
            const auto ICON_POS = ImVec2(100.f * SCALE.x, 132.f * SCALE.y);
            if (app_icon) {
                const ImVec2 ICON_POS_MIN(PUPUP_POS.x + ICON_POS.x, PUPUP_POS.y + ICON_POS.y);
                const ImVec2 ICON_POS_MAX(ICON_POS_MIN.x + ICON_SIZE.x, ICON_POS_MIN.y + ICON_SIZE.y);
                window_draw_list->AddImageRounded(app_icon, ICON_POS_MIN, ICON_POS_MAX, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, ICON_SIZE.x);
            }
            ImGui::SetCursorPos(ImVec2(ICON_POS.x + ICON_SIZE.x + (12.f * SCALE.x), ICON_POS.y + ((ICON_SIZE.y - ImGui::GetFontSize()) / 2.f)));
            ImGui::Text("%s", app_title.c_str());
            const auto remaining_str = get_remaining_str(gui.lang, remaining);
            ImGui::SetCursorPos(ImVec2(POPUP_SIZE.x - (100 * SCALE.x) - (ImGui::CalcTextSize(remaining_str.c_str()).x), 214.f * SCALE.y));
            ImGui::Text("%s", remaining_str.c_str());
            const float PROGRESS_BAR_WIDTH = 560.f * SCALE.x;
            ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() / 2) - (PROGRESS_BAR_WIDTH / 2.f), 236.f * SCALE.y));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, GUI_PROGRESS_BAR);
            ImGui::ProgressBar(progress, ImVec2(PROGRESS_BAR_WIDTH, 14.f * SCALE.y), "");
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (16.f * SCALE.y));
            TextColoredCentered(GUI_COLOR_TEXT, std::to_string(static_cast<uint32_t>(progress * 100.f)).append("%").c_str());
            ImGui::PopStyleColor();
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 15.f * SCALE.x);
            const ImVec2 CANCEL_BUTTON_SIZE(320.f * SCALE.x, 46.f * SCALE.y);
            ImGui::SetCursorPos(ImVec2((POPUP_SIZE.x - CANCEL_BUTTON_SIZE.x) / 2.f, POPUP_SIZE.y - CANCEL_BUTTON_SIZE.y - (24.f * SCALE.y)));
            ImGui::BeginDisabled(progress >= 1.f);
            if (ImGui::Button(emuenv.common_dialog.lang.common["cancel"].c_str(), CANCEL_BUTTON_SIZE)) {
                std::lock_guard<std::mutex> lock(progress_state.mutex);
                progress_state.canceled = true;
                cloud_dialog_state = CLOUD_DIALOG_NONE;
            }
            ImGui::EndDisabled();

            // Hold the dialog at 100% for a short moment to ensure the user perceives the completion state
            if (progress >= 1.f) {
                const auto now = get_timestamp_ms();

                if (progress_done_timestamp == 0)
                    progress_done_timestamp = now;

                if (now - progress_done_timestamp >= 900) {
                    gui.vita_area.please_wait = true;
                    please_wait_timestamp = now;
                    cloud_dialog_state = CLOUD_DIALOG_NONE;
                }
            }

            ImGui::PopStyleVar();
            ImGui::EndChild();
            ImGui::PopStyleVar();
            break;
        }
        default:
            break;
        }
        ImGui::End();
    }

    // Hide please wait after a short delay to improve UX
    if (gui.vita_area.please_wait && !please_wait_done.load()) {
        const auto now = get_timestamp_ms();

        if (now - please_wait_timestamp >= 900) {
            please_wait_done.store(true);

            {
                std::lock_guard<std::mutex> lck(mutex_progress);
                cv_progress.notify_all();
            }
        }
    }

    const auto END_SEPARATOR_POS_Y = SEPARATOR_BUTTON_POS_Y + BUTTONS_SECTION_SIZE_Y;
    ImGui::SetCursorPos(ImVec2(0.f, END_SEPARATOR_POS_Y));
    ImGui::Separator();

    ImGui::SetWindowFontScale(1.85f * RES_SCALE.y);
    ImGui::SetCursorPos(ImVec2(54.f * SCALE.x, END_SEPARATOR_POS_Y + ((LOCATION_SECTION_SIZE_Y - ImGui::GetFontSize()) / 2.f)));
    ImGui::Text("Local");

    ImGui::SetWindowFontScale(1.68f * RES_SCALE.y);
    if (has_local_save) {
        const auto &local_info = savedata_info[SAVE_DATA_TYPE_LOCAL];
        ImGui::SetCursorPos(ImVec2(begin_text_pos_x, END_SEPARATOR_POS_Y + (LOCATION_SECTION_SIZE_Y / 2.f) - app_title_height_size));
        ImGui::Text("%s", app_title.c_str());
        ImGui::SetCursorPos(ImVec2(begin_text_pos_x, END_SEPARATOR_POS_Y + (LOCATION_SECTION_SIZE_Y / 2.f) + (10.f * SCALE.y)));
        tm updated_at_tm = {};
        SAFE_LOCALTIME(&local_info.last_updated, &updated_at_tm);
        auto UPDATED_AT = get_date_time(gui, emuenv, updated_at_tm);
        ImGui::SetWindowFontScale(1.12f * RES_SCALE.y);
        ImGui::TextColored(GUI_COLOR_TEXT, "%s", fmt::format("{} {} {} {}", UPDATED_AT[DateTime::DATE_MINI], UPDATED_AT[DateTime::CLOCK], is_12_hour_format ? UPDATED_AT[DateTime::DAY_MOMENT] : "", get_unit_size(local_info.total_size)).c_str());
    } else {
        ImGui::SetCursorPos(ImVec2(begin_text_pos_x, END_SEPARATOR_POS_Y + (LOCATION_SECTION_SIZE_Y - ImGui::GetFontSize()) / 2.f));
        ImGui::Text("-");
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

void open_cloud_save(GuiState &gui, EmuEnvState &emuenv, const std::string &titleid) {
    gui.vita_area.connecting_please_wait = true;
    std::thread([&gui, &emuenv, titleid]() mutable {
        if (get_savedata_info(gui, emuenv, titleid)) {
            gui.vita_area.cloud_save = true;
            gui.vita_area.home_screen = false;
            gui.vita_area.live_area_screen = false;
        }
        gui.vita_area.connecting_please_wait = false;
    }).detach();
}

void draw_v3kn_dialog(GuiState &gui, EmuEnvState &emuenv) {
    const ImVec2 VIEWPORT_POS(emuenv.logical_viewport_pos.x, emuenv.logical_viewport_pos.y);
    const ImVec2 VIEWPORT_SIZE(emuenv.logical_viewport_size.x, emuenv.logical_viewport_size.y);
    const auto RES_SCALE = ImVec2(emuenv.gui_scale.x, emuenv.gui_scale.y);
    const auto SCALE = ImVec2(RES_SCALE.x * emuenv.manual_dpi_scale, RES_SCALE.y * emuenv.manual_dpi_scale);
    const ImVec2 WINDOW_SIZE(VIEWPORT_SIZE.x / 1.6f, 0);
    const ImVec2 WINDOW_POS(VIEWPORT_POS.x + (VIEWPORT_SIZE.x - WINDOW_SIZE.x) / 2.f,
        VIEWPORT_POS.y + (VIEWPORT_SIZE.y - WINDOW_SIZE.y) / 2.f);
    ImGui::SetNextWindowPos(WINDOW_POS, ImGuiCond_Always, ImVec2(0.f, 0.5f));
    ImGui::SetNextWindowSize(WINDOW_SIZE, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::Begin("##v3kn_dialog", &gui.configuration_menu.v3kn_dialog, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    const ImVec2 BUTTON_SIZE(200.f * SCALE.x, 40.f * SCALE.y);
    const ImVec2 SMALL_BUTTON_SIZE(100.f * SCALE.x, 40.f * SCALE.y);
    static std::string result_message;
    static char npid[16] = "";
    static char password[64] = "";
    static char confirm[64] = "";

    auto &commong_lang = emuenv.common_dialog.lang.common;
    const std::string OK_STR = commong_lang["ok"];
    const std::string CANCEL_STR = commong_lang["cancel"];

    static auto result_popup = [&]() {
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Result", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            const auto popup_width_size = ImGui::GetWindowWidth();
            ImGui::Text("%s", result_message.c_str());
            ImGui::SetCursorPosX((popup_width_size - SMALL_BUTTON_SIZE.x) / 2.f);
            if (ImGui::Button("OK", SMALL_BUTTON_SIZE)) {
                ImGui::CloseCurrentPopup();

                if (result_message.find("successfully") != std::string::npos) {
                    result_message = "close";
                    npid[0] = '\0';
                    password[0] = '\0';
                    confirm[0] = '\0';
                } else
                    result_message.clear();
            }
            ImGui::EndPopup();
        }
    };
    ImGui::SetWindowFontScale(RES_SCALE.y);
    const auto FONT_SCALE = 0.8f;
    std::string title = "V3KN Account Management";
    const auto title_size = ImGui::CalcTextSize(title.c_str());
    const ImVec2 POPUP_SIZE = ImGui::GetWindowSize();
    const ImVec2 POPUP_POS = ImGui::GetWindowPos();
    const auto BLOCK_TITLE_SIZE_HEIGHT = 40.f * SCALE.y;
    const ImVec2 title_pos((POPUP_SIZE.x - title_size.x) / 2.f, (BLOCK_TITLE_SIZE_HEIGHT - (title_size.y * FONT_SCALE)) / 2.f);
    ImGui::SetCursorPos(title_pos);
    ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title.c_str());
    ImGui::SetCursorPosY(BLOCK_TITLE_SIZE_HEIGHT);
    ImGui::Separator();
    const auto AVATAR_SIZE = ImVec2(static_cast<float>(AvatarSize::V3KN) * SCALE.x, static_cast<float>(AvatarSize::V3KN) * SCALE.y);
    const auto BLOCK_INFO_SIZE_HEIGHT = AVATAR_SIZE.y + (12.f * SCALE.y);
    const auto INFO_SIZE_HEIGHT = BLOCK_INFO_SIZE_HEIGHT / 3.f;
    const auto BEGIN_INFO_POS = BLOCK_TITLE_SIZE_HEIGHT + 1.f;
    const auto FIRST_INFO_POS = BEGIN_INFO_POS + ((INFO_SIZE_HEIGHT - (ImGui::GetFontSize() * FONT_SCALE)) / 2.f);
    ImGui::SetCursorPosY(FIRST_INFO_POS);
    ImGui::Text("Current NPID: %s", user_info.NPID.empty() ? "-" : user_info.NPID.c_str());
    std::string created_at = "-";
    if (user_info.created_at > 0) {
        tm created_tm = {};
        SAFE_LOCALTIME(&user_info.created_at, &created_tm);
        auto CREATED_AT = get_date_time(gui, emuenv, created_tm);
        created_at = fmt::format("{} {} {}", CREATED_AT[DateTime::DATE_MINI], CREATED_AT[DateTime::CLOCK], emuenv.cfg.sys_time_format == SCE_SYSTEM_PARAM_TIME_FORMAT_12HOUR ? CREATED_AT[DateTime::DAY_MOMENT] : "");
    }
    ImGui::SetCursorPosY(FIRST_INFO_POS + INFO_SIZE_HEIGHT);
    ImGui::Text("Account Created At: %s", created_at.c_str());
    ImGui::SetCursorPosY(FIRST_INFO_POS + (INFO_SIZE_HEIGHT * 2));
    ImGui::Text("Quota Used: %s / %s", user_info.NPID.empty() ? "-" : get_unit_size(user_info.quota_used).c_str(), user_info.NPID.empty() ? "-" : get_unit_size(user_info.quota_total).c_str());

    const auto END_INFO_POS = BEGIN_INFO_POS + BLOCK_INFO_SIZE_HEIGHT;

    // Draw user avatar
    if (gui.users_avatar[emuenv.io.user_id]) {
        const auto &user_avatar_infos = gui.users_avatar_infos[emuenv.io.user_id][AvatarSize::V3KN];
        const ImVec2 AVATAR_FINAL_SIZE = ImVec2(user_avatar_infos.size.x * SCALE.x, user_avatar_infos.size.y * SCALE.y);
        const ImVec2 AVATAR_POS = ImVec2(
            POPUP_SIZE.x - AVATAR_SIZE.x - (10.f * SCALE.x) + (user_avatar_infos.pos.x * SCALE.x),
            BEGIN_INFO_POS + ((BLOCK_INFO_SIZE_HEIGHT - AVATAR_SIZE.y) / 2.f) + (user_avatar_infos.pos.y * SCALE.y));
        const float BORDER_THICKNESS = 2.f * SCALE.x;
        ImGui::GetWindowDrawList()->AddRect(ImVec2(POPUP_POS.x + AVATAR_POS.x - BORDER_THICKNESS, POPUP_POS.y + AVATAR_POS.y - BORDER_THICKNESS), ImVec2(POPUP_POS.x + AVATAR_POS.x + AVATAR_FINAL_SIZE.x + BORDER_THICKNESS, POPUP_POS.y + AVATAR_POS.y + AVATAR_FINAL_SIZE.y + BORDER_THICKNESS), IM_COL32(100, 149, 237, 255), 0.f, 0, BORDER_THICKNESS);
        ImGui::SetCursorPos(AVATAR_POS);
        ImGui::Image(gui.users_avatar[emuenv.io.user_id], AVATAR_FINAL_SIZE);
    }

    ImGui::SetCursorPosY(END_INFO_POS);
    ImGui::Separator();
    ImGui::Spacing();
    const auto AFTER_INFO_POS = ImGui::GetCursorPosY();

    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - BUTTON_SIZE.x - (20.f * SCALE.x));
    if (ImGui::Button("Refresh Quota", BUTTON_SIZE)) {
        const std::string url = fmt::format("{}/v3kn/quota", V3KN_SERVER_URL);
        const auto res = net_utils::get_web_response_ex(url, user_info.token);
        if (res.body.starts_with("OK:")) {
            std::smatch match;
            // Parse response using regex, expecting format "OK:used:total"
            std::regex regex_pattern(R"(OK:([^:]+):([^:]+))");
            if (std::regex_search(res.body, match, regex_pattern) && (match.size() == 3)) {
                user_info.quota_used = std::stoull(match[1]);
                user_info.quota_total = std::stoull(match[2]);
                save_v3kn_user_info(emuenv);
                result_message = fmt::format("Quota refreshed successfully: used {} / {}", get_unit_size(user_info.quota_used), get_unit_size(user_info.quota_total));
            } else {
                result_message = get_v3kn_error_message(emuenv, res);
                LOG_WARN("V3KN quota refresh: failed to parse server response");
            }
        } else {
            result_message = get_v3kn_error_message(emuenv, res);
            LOG_WARN("V3KN quota refresh failed, server response: {}", res.body);
        }
        ImGui::OpenPopup("Result Quota");
    }
    SetTooltipEx("Refresh quota — use only if you uploaded from another device. Avoid repeated requests!");
    if (ImGui::BeginPopupModal("Result Quota", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        const auto popup_width_size = ImGui::GetWindowWidth();
        ImGui::Text("%s", result_message.c_str());
        ImGui::SetCursorPosX((popup_width_size - SMALL_BUTTON_SIZE.x) / 2.f);
        if (ImGui::Button("OK", SMALL_BUTTON_SIZE)) {
            ImGui::CloseCurrentPopup();
            result_message.clear();
        }
        ImGui::EndPopup();
    }

    // Create Account Popup
    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - BUTTON_SIZE.x - (20.f * SCALE.x));
    if (ImGui::Button("Create Account", BUTTON_SIZE))
        ImGui::OpenPopup("V3KN Create");
    if (ImGui::BeginPopupModal("V3KN Create", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        const auto title = "Create V3KN Account";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title).x) / 2.f);
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title);
        ImGui::Separator();

        ImGui::InputText("NPID", npid, sizeof(npid), ImGuiInputTextFlags_CharsNoBlank);
        ImGui::InputText("Password", password, sizeof(password), ImGuiInputTextFlags_Password);
        ImGui::InputText("Confirm Password", confirm, sizeof(confirm), ImGuiInputTextFlags_Password);

        ImGui::BeginDisabled(std::string(password).empty() || std::string(confirm).empty() || std::string(npid).empty() || strlen(password) < 8);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - SMALL_BUTTON_SIZE.x - (20.f * SCALE.x));
        if (ImGui::Button(OK_STR.c_str(), SMALL_BUTTON_SIZE)) {
            if (std::string(password) != std::string(confirm)) {
                result_message = "Password and confirmation do not match.";
                LOG_WARN("V3KN create account: password and confirmation do not match");
            } else {
                const std::string derived_password = derive_password(password);
                const std::string base64_password = base64_encode(derived_password);
                const auto res = v3kn_create(npid, base64_password);
                if (res.body.starts_with("OK:")) {
                    std::smatch match;
                    // Parse response using regex, expecting format "OK:token:created_at:total_quota"
                    std::regex regex_pattern(R"(OK:([^:]+):([^:]+):([^:]+))");
                    if (std::regex_search(res.body, match, regex_pattern) && (match.size() == 4)) {
                        user_info.NPID = npid;
                        user_info.password = base64_password;
                        user_info.token = match[1];
                        user_info.created_at = std::stoull(match[2]);
                        user_info.quota_used = 0;
                        user_info.quota_total = std::stoull(match[3]);
                        save_v3kn_user_info(emuenv);
                        result_message = fmt::format("Account created successfully for user {}", user_info.NPID);
                        LOG_INFO("V3KN account created successfully for NPID {}", user_info.NPID);
                    } else {
                        result_message = get_v3kn_error_message(emuenv, res);
                        LOG_WARN("V3KN create account: failed to parse server response");
                    }
                } else {
                    result_message = get_v3kn_error_message(emuenv, res);
                    LOG_WARN("V3KN create account failed for NPID {}, reason: {}", npid, result_message);
                }
            }
            ImGui::OpenPopup("Result");
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 40.f * SCALE.x);
        if (ImGui::Button(CANCEL_STR.c_str(), SMALL_BUTTON_SIZE)) {
            // Reset input fields
            npid[0] = '\0';
            password[0] = '\0';
            confirm[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        result_popup();
        if (result_message == "close") {
            ImGui::CloseCurrentPopup();
            result_message.clear();
        }

        ImGui::EndPopup();
    }

    // Delete Account Popup
    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - BUTTON_SIZE.x - (20.f * SCALE.x));
    if (ImGui::Button("Delete Account", BUTTON_SIZE))
        ImGui::OpenPopup("V3KN Delete");
    if (ImGui::BeginPopupModal("V3KN Delete", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        const auto title = "Delete V3KN Account";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title).x) / 2.f);
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title);
        ImGui::Separator();

        ImGui::InputText("Password", password, sizeof(password), ImGuiInputTextFlags_Password);
        ImGui::BeginDisabled(std::string(password).empty());
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - SMALL_BUTTON_SIZE.x - (20.f * SCALE.x));
        if (ImGui::Button(OK_STR.c_str(), SMALL_BUTTON_SIZE)) {
            result_message.clear();
            const std::string derived_password = derive_password(password);
            const std::string base64_password = base64_encode(derived_password);
            const auto res = v3kn_delete(base64_password);
            if (res.body.starts_with("OK:")) {
                result_message = "Account deleted successfully for user " + user_info.NPID;
                user_info = {};
                save_v3kn_user_info(emuenv);
            } else {
                result_message = get_v3kn_error_message(emuenv, res);
                LOG_ERROR("V3KN delete account failed for NPID {}, reason: {}", user_info.NPID, result_message);
            }
            ImGui::OpenPopup("Result");
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 40.f * SCALE.x);
        if (ImGui::Button(CANCEL_STR.c_str(), SMALL_BUTTON_SIZE)) {
            password[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        result_popup();
        if (result_message == "close") {
            ImGui::CloseCurrentPopup();
            result_message.clear();
        }
        ImGui::EndPopup();
    }

    // Login Popup
    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) - BUTTON_SIZE.x - (20.f * SCALE.x));
    if (ImGui::Button("Login", BUTTON_SIZE))
        ImGui::OpenPopup("V3KN Login");
    if (ImGui::BeginPopupModal("V3KN Login", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        const auto title = "V3KN Login";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title).x) / 2.f);
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title);
        ImGui::Separator();

        ImGui::InputText("NPID", npid, sizeof(npid));
        ImGui::InputText("Password", password, sizeof(password), ImGuiInputTextFlags_Password);

        ImGui::BeginDisabled((std::string(password).empty()) || std::string(npid).empty());
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - SMALL_BUTTON_SIZE.x - (20.f * SCALE.x));
        if (ImGui::Button(OK_STR.c_str(), SMALL_BUTTON_SIZE)) {
            const std::string derived_password = derive_password(password);
            const std::string base64_password = base64_encode(derived_password);
            const auto res = v3kn_login(npid, base64_password);
            if (res.body.starts_with("OK:")) {
                std::smatch match;
                // Parse response using regex, expecting format "OK:token:created_at:used_quota:total_quota"
                std::regex regex_pattern(R"(OK:([^:]+):([^:]+):([^:]+):([^:]+))");
                if (std::regex_search(res.body, match, regex_pattern) && (match.size() == 5)) {
                    user_info.NPID = npid;
                    user_info.password = base64_password;
                    user_info.token = match[1];
                    user_info.created_at = std::stoull(match[2]);
                    user_info.quota_used = std::stoull(match[3]);
                    user_info.quota_total = std::stoull(match[4]);
                    result_message = "Login successfully for user " + user_info.NPID;
                    save_v3kn_user_info(emuenv);
                } else {
                    result_message = get_v3kn_error_message(emuenv, res);
                    LOG_WARN("V3KN login: failed to parse server response, reason: {}", result_message);
                }
            } else {
                result_message = get_v3kn_error_message(emuenv, res);
                LOG_ERROR("V3KN login failed for NPID {}, reason: {}", npid, result_message);
            }

            ImGui::OpenPopup("Result");
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 40.f * SCALE.x);
        if (ImGui::Button(CANCEL_STR.c_str(), SMALL_BUTTON_SIZE)) {
            npid[0] = '\0';
            password[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        result_popup();
        if (result_message == "close") {
            ImGui::CloseCurrentPopup();
            result_message.clear();
        }
        ImGui::EndPopup();
    }

    const auto AFTER_LAST_BUTTON_POS = ImGui::GetCursorPosY();

    ImGui::SetCursorPosY(AFTER_INFO_POS);

    // Logout Button
    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) + (20.f * SCALE.x));
    if (ImGui::Button("Logout", BUTTON_SIZE)) {
        user_info = {};
        save_v3kn_user_info(emuenv);
        result_message = "Logged out successfully.";
        ImGui::OpenPopup("Result Logout");
    }
    if (ImGui::BeginPopupModal("Result Logout", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        const auto popup_width_size = ImGui::GetWindowWidth();
        ImGui::Text("%s", result_message.c_str());
        ImGui::SetCursorPosX((popup_width_size - SMALL_BUTTON_SIZE.x) / 2.f);
        if (ImGui::Button("OK", SMALL_BUTTON_SIZE)) {
            ImGui::CloseCurrentPopup();
            result_message.clear();
        }
        ImGui::EndPopup();
    }

    // Change NPID Popup
    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) + (20.f * SCALE.x));
    if (ImGui::Button("Change NPID", BUTTON_SIZE))
        ImGui::OpenPopup("V3KN Change NPID");
    if (ImGui::BeginPopupModal("V3KN Change NPID", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        const auto title = "Change V3KN NPID";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title).x) / 2.f);
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title);
        ImGui::Separator();
        ImGui::InputText("New NPID", npid, sizeof(npid), ImGuiInputTextFlags_CharsNoBlank);
        ImGui::BeginDisabled(std::string(npid).empty());
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - SMALL_BUTTON_SIZE.x - (20.f * SCALE.x));
        if (ImGui::Button(OK_STR.c_str(), SMALL_BUTTON_SIZE)) {
            const auto res = v3kn_change_npid(npid);
            if (res.body.starts_with("OK:")) {
                user_info.NPID = npid;
                save_v3kn_user_info(emuenv);
                result_message = "NPID changed successfully to " + user_info.NPID;
                LOG_INFO("V3KN NPID changed successfully to {}", user_info.NPID);
            } else {
                result_message = get_v3kn_error_message(emuenv, res);
                LOG_WARN("V3KN change NPID failed for NPID {}, reason: {}", user_info.NPID, result_message);
            }
            ImGui::OpenPopup("Result");
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 40.f * SCALE.x);
        if (ImGui::Button(CANCEL_STR.c_str(), SMALL_BUTTON_SIZE)) {
            npid[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        result_popup();
        if (result_message == "close") {
            ImGui::CloseCurrentPopup();
            result_message.clear();
        }
        ImGui::EndPopup();
    }

    // Change Password Popup
    ImGui::SetCursorPosX((WINDOW_SIZE.x / 2.f) + (20.f * SCALE.x));
    if (ImGui::Button("Change Password", BUTTON_SIZE))
        ImGui::OpenPopup("V3KN Change Password");
    if (ImGui::BeginPopupModal("V3KN Change Password", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        const auto title = "Change V3KN Password";
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(title).x) / 2.f);
        ImGui::TextColored(GUI_COLOR_TEXT_TITLE, "%s", title);
        ImGui::Separator();
        ImGui::InputText("Current Password", password, sizeof(password), ImGuiInputTextFlags_Password);
        ImGui::InputText("New Password", confirm, sizeof(confirm), ImGuiInputTextFlags_Password);
        ImGui::BeginDisabled(std::string(password).empty() || std::string(confirm).empty());
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - SMALL_BUTTON_SIZE.x - (20.f * SCALE.x));
        if (ImGui::Button(OK_STR.c_str(), SMALL_BUTTON_SIZE)) {
            const std::string derived_current_password = derive_password(password);
            const std::string base64_current_password = base64_encode(derived_current_password);
            if (base64_current_password != user_info.password) {
                result_message = "Current password is incorrect.";
                LOG_WARN("V3KN change password: current password is incorrect for NPID {}", user_info.NPID);
            } else {
                const std::string derived_new_password = derive_password(confirm);
                const std::string base64_new_password = base64_encode(derived_new_password);
                const auto res = v3kn_change_password(base64_current_password, base64_new_password);
                if (res.body.starts_with("OK")) {
                    user_info.password = base64_new_password;
                    save_v3kn_user_info(emuenv);
                    result_message = "Password changed successfully for user " + user_info.NPID;
                    LOG_INFO("V3KN password changed successfully for NPID {}", user_info.NPID);
                } else {
                    result_message = get_v3kn_error_message(emuenv, res);
                    LOG_WARN("V3KN change password failed for NPID {}, reason: {}", user_info.NPID, result_message);
                }
            }
            ImGui::OpenPopup("Result");
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 40.f * SCALE.x);
        if (ImGui::Button(CANCEL_STR.c_str(), SMALL_BUTTON_SIZE)) {
            password[0] = '\0';
            confirm[0] = '\0';
            ImGui::CloseCurrentPopup();
        }

        result_popup();
        if (result_message == "close") {
            ImGui::CloseCurrentPopup();
            result_message.clear();
        }
        ImGui::EndPopup();
    }

    ImGui::SetCursorPosY(AFTER_LAST_BUTTON_POS);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Close Button
    ImGui::SetCursorPosX((WINDOW_SIZE.x - BUTTON_SIZE.x) / 2.f);
    if (ImGui::Button("Close", BUTTON_SIZE)) {
        gui.configuration_menu.v3kn_dialog = false;
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace gui