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

#include <compat/functions.h>
#include <compat/state.h>
#include <config/state.h>
#include <emuenv/state.h>
#include <fmt/std.h>
#include <util/fs.h>
#include <util/info_message.h>
#include <util/log.h>
#include <util/net_utils.h>

#include <miniz.h>
#include <pugixml.hpp>

#include <fstream>
#include <regex>
#include <span>
#include <vector>

enum class LabelId : uint32_t {
    Nothing = 1260231569, // 0x4b1d9b91
    Bootable = 1344750319, // 0x502742ef
    Intro = 1260231381, // 0x4B9F5E5D
    Menu = 1344751053, // 0x4F1B9135
    Ingame_Less = 1344752299, // 0x4F7B6B3B
    Ingame_More = 1260231985, // 0x4B2A9819
    Playable = 920344019, // 0x36db55d3
};

namespace compat {

static constexpr uint32_t db_version = 1;

static CompatibilityState label_to_state(uint32_t raw) {
    switch (static_cast<LabelId>(raw)) {
    case LabelId::Nothing: return NOTHING;
    case LabelId::Bootable: return BOOTABLE;
    case LabelId::Intro: return INTRO;
    case LabelId::Menu: return MENU;
    case LabelId::Ingame_Less: return INGAME_LESS;
    case LabelId::Ingame_More: return INGAME_MORE;
    case LabelId::Playable: return PLAYABLE;
    default: return UNKNOWN;
    }
}

static bool parse_xml(CompatState &state, const uint8_t *data, size_t size) {
    pugi::xml_document doc;
    if (!doc.load_buffer(data, size)) {
        LOG_ERROR("XML parse failed");
        return false;
    }

    const auto compatibility = doc.child("compatibility");
    const uint32_t ver = compatibility.attribute("version").as_uint();
    if (ver != db_version) {
        LOG_WARN("DB version {} does not match expected {}", ver, db_version);
    }

    state.db_issue_count = compatibility.attribute("issue_count").as_uint();
    state.db_updated_at = compatibility.attribute("iso_db_updated_at").as_string();
    state.compat_db_loaded = false;
    state.app_compat_db.clear();

    for (const auto &app : compatibility) {
        const std::string title_id = app.attribute("title_id").as_string();
        const uint32_t issue_id = app.child("issue_id").text().as_uint();

        if (!title_id.contains("PCS") && (title_id != "NPXS10007")) {
            LOG_WARN_IF(state.log_compat_warn, "Title ID {} is invalid. Please check GitHub issue {} and verify it!", title_id, issue_id);
            continue;
        }

        if (state.app_compat_db.contains(title_id)) {
            LOG_WARN_IF(state.log_compat_warn, "Duplicate title ID {} (issue {})", title_id, issue_id);
            continue;
        }

        auto compat_state = UNKNOWN;
        for (const auto &label : app.child("labels")) {
            const auto s = label_to_state(label.text().as_uint());
            if (s != UNKNOWN)
                compat_state = s;
        }

        if (compat_state == UNKNOWN)
            LOG_WARN_IF(state.log_compat_warn, "App with Title ID {} has an issue but no status label. Please check GitHub issue {} and request a status label be added.", title_id, issue_id);

        state.app_compat_db[title_id] = {
            .issue_id = issue_id,
            .state = compat_state,
            .updated_at = app.child("updated_at").text().as_llong(),
        };
    }

    state.compat_db_loaded = !state.app_compat_db.empty();
    return state.compat_db_loaded;
}

std::optional<UpdateInfo> parse_ver_resp(const CompatState &state, const std::string &body) {
    static const std::regex re(
        R"(Last updated: (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}Z))");

    std::smatch match;
    if (!std::regex_search(body, match, re)) {
        spdlog::error("Compat: Could not find version string in response");
        return std::nullopt;
    }

    const std::string latest = match[1].str();
    return UpdateInfo{
        .latest_ver = latest,
        .needs_update = latest != state.db_updated_at,
    };
}

bool load_from_disk(CompatState &state, const std::filesystem::path &cache_path) {
    const auto db_path = cache_path / "app_compat_db.xml";

    if (!std::filesystem::exists(db_path)) {
        LOG_WARN("DB not found at {}", db_path);
        return false;
    }

    std::ifstream file(db_path, std::ios::binary | std::ios::ate);
    if (!file) {
        LOG_ERROR("Could not open {}", db_path);
        return false;
    }

    const auto size = static_cast<size_t>(file.tellg());
    file.seekg(0);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
        LOG_ERROR("Failed to read {}", db_path);
        return false;
    }

    return parse_xml(state, buffer.data(), buffer.size());
}

bool load_app_compat_db(CompatState &state, EmuEnvState &emuenv, const UpdateMessageTexts &message_texts) {
    const std::filesystem::path cache_path(emuenv.cache_path.native());
    load_from_disk(state, cache_path);
    return update_app_compat_db(state, emuenv, message_texts);
}

bool update_app_compat_db(CompatState &state, EmuEnvState &emuenv, const UpdateMessageTexts &message_texts) {
    static constexpr const char *latest_link = "https://api.github.com/repos/Vita3K/compatibility/releases/latest";
    static constexpr const char *app_compat_db_link = "https://github.com/Vita3K/compatibility/releases/download/compat_db/app_compat_db.xml.zip";

    const auto version_response = net_utils::get_web_response(latest_link);
    if (version_response.empty()) {
        LOG_WARN("Could not check for compatibility database updates");
        util::info_message_queue().push({
            .function = "compatibility database update",
            .title = message_texts.error_title,
            .level = spdlog::level::err,
            .msg = message_texts.check_failed,
        });
        return false;
    }

    const auto update = parse_ver_resp(state, version_response);
    if (!update) {
        util::info_message_queue().push({
            .function = "compatibility database update",
            .title = message_texts.error_title,
            .level = spdlog::level::err,
            .msg = message_texts.check_failed,
        });
        return false;
    }
    if (!update->needs_update) {
        LOG_INFO("Applications compatibility database is up to date.");
        return false;
    }

    const auto app_compat_db_path = emuenv.cache_path / "app_compat_db.xml";
    const bool compat_db_exist = fs::exists(app_compat_db_path);
    const auto new_app_compat_db_path = emuenv.cache_path / "new_app_compat_db.xml.zip";
    const auto archive_path_utf8 = fs_utils::path_to_utf8(new_app_compat_db_path);
    if (!net_utils::download_file(app_compat_db_link, archive_path_utf8)) {
        LOG_WARN("Could not download compatibility database version {}", update->latest_ver);
        fs::remove(new_app_compat_db_path);
        util::info_message_queue().push({
            .function = "compatibility database update",
            .title = message_texts.error_title,
            .level = spdlog::level::err,
            .msg = fmt::format(fmt::runtime(message_texts.download_failed), update->latest_ver),
        });
        return false;
    }

    fs::ifstream archive(new_app_compat_db_path, std::ios::binary | std::ios::ate);
    if (!archive) {
        LOG_ERROR("Could not open downloaded compatibility database at {}", archive_path_utf8);
        fs::remove(new_app_compat_db_path);
        util::info_message_queue().push({
            .function = "compatibility database update",
            .title = message_texts.error_title,
            .level = spdlog::level::err,
            .msg = fmt::format(fmt::runtime(message_texts.download_failed), update->latest_ver),
        });
        return false;
    }

    const auto archive_size = static_cast<std::streamoff>(archive.tellg());
    if (archive_size <= 0) {
        LOG_ERROR("Downloaded compatibility database is empty");
        archive.close();
        fs::remove(new_app_compat_db_path);
        util::info_message_queue().push({
            .function = "compatibility database update",
            .title = message_texts.error_title,
            .level = spdlog::level::err,
            .msg = fmt::format(fmt::runtime(message_texts.download_failed), update->latest_ver),
        });
        return false;
    }

    std::vector<uint8_t> archive_data(static_cast<size_t>(archive_size));
    archive.seekg(0);
    if (!archive.read(reinterpret_cast<char *>(archive_data.data()), static_cast<std::streamsize>(archive_size))) {
        LOG_ERROR("Failed to read downloaded compatibility database");
        archive.close();
        fs::remove(new_app_compat_db_path);
        util::info_message_queue().push({
            .function = "compatibility database update",
            .title = message_texts.error_title,
            .level = spdlog::level::err,
            .msg = fmt::format(fmt::runtime(message_texts.download_failed), update->latest_ver),
        });
        return false;
    }
    archive.close();

    const auto old_db_updated_at = state.db_updated_at;
    const auto old_issue_count = state.db_issue_count;
    const auto old_app_count = state.app_compat_db.size();
    const std::filesystem::path cache_path(emuenv.cache_path.native());
    const bool installed = install_db(state, cache_path,
        std::span<const uint8_t>(archive_data.data(), archive_data.size()), update->latest_ver);
    fs::remove(new_app_compat_db_path);
    if (!installed) {
        util::info_message_queue().push({
            .function = "compatibility database update",
            .title = message_texts.error_title,
            .level = spdlog::level::err,
            .msg = fmt::format(fmt::runtime(message_texts.load_failed), update->latest_ver),
        });
        return false;
    }

    util::InfoMessage message{
        .function = "compatibility database update",
        .title = message_texts.updated_title,
        .level = spdlog::level::info,
    };
    const auto added_app_count = static_cast<int32_t>(state.app_compat_db.size()) - static_cast<int32_t>(old_app_count);
    if (!compat_db_exist)
        message.msg = fmt::format(fmt::runtime(message_texts.download_app_listed), state.db_updated_at, state.app_compat_db.size());
    else if (!old_db_updated_at.empty() && added_app_count > 0)
        message.msg = fmt::format(fmt::runtime(message_texts.new_app_listed), old_db_updated_at, state.db_updated_at, added_app_count);
    else
        message.msg = fmt::format(fmt::runtime(message_texts.app_listed), old_db_updated_at, state.db_updated_at, state.app_compat_db.size());
    util::info_message_queue().push(std::move(message));

    if (compat_db_exist) {
        const auto dif = static_cast<int32_t>(state.db_issue_count) - static_cast<int32_t>(old_issue_count);
        if (!old_db_updated_at.empty() && dif > 0)
            LOG_INFO("Compatibility database updated from {} to {}: {} new issues, {} apps", old_db_updated_at, state.db_updated_at, dif, state.app_compat_db.size());
        else
            LOG_INFO("Compatibility database updated to {}: {} apps", state.db_updated_at, state.app_compat_db.size());
    } else
        LOG_INFO("Compatibility database downloaded: {} apps", state.app_compat_db.size());

    return true;
}

bool install_db(CompatState &state, const std::filesystem::path &cache_path,
    std::span<const uint8_t> zip_data, const std::string &new_version) {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, zip_data.data(), zip_data.size(), 0)) {
        LOG_ERROR("Failed to open zip from memory");
        return false;
    }

    const mz_uint num_files = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < num_files; ++i) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
            LOG_ERROR("Failed to stat file {} in zip", i);
            mz_zip_reader_end(&zip);
            return false;
        }

        const auto out_path = cache_path / stat.m_filename;
        if (!mz_zip_reader_extract_to_file(&zip, i, reinterpret_cast<const char *>(out_path.u8string().c_str()), 0)) {
            LOG_ERROR("Failed to extract {} from zip", stat.m_filename);
            mz_zip_reader_end(&zip);
            return false;
        }
    }

    mz_zip_reader_end(&zip);

    if (!load_from_disk(state, cache_path))
        return false;

    state.db_updated_at = new_version;
    return true;
}

CompatibilityState get_app_compat(const CompatState &state, const std::string &title_id) {
    const auto it = state.app_compat_db.find(title_id);
    return it != state.app_compat_db.end() ? it->second.state : UNKNOWN;
}

} // namespace compat
