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
#include <util/log.h>

#include <miniz.h>
#include <pugixml.hpp>

#include <regex>
#include <span>

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

        if ((title_id.find("PCS") == std::string::npos) && (title_id != "NPXS10007")) {
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
        LOG_WARN("DB not found at {}", db_path.string());
        return false;
    }

    std::ifstream file(db_path, std::ios::binary | std::ios::ate);
    if (!file) {
        LOG_ERROR("Could not open {}", db_path.string());
        return false;
    }

    const auto size = static_cast<size_t>(file.tellg());
    file.seekg(0);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char *>(buffer.data()), size)) {
        LOG_ERROR("Failed to read {}", db_path.string());
        return false;
    }

    return parse_xml(state, buffer.data(), buffer.size());
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
        if (!mz_zip_reader_extract_to_file(&zip, i, out_path.string().c_str(), 0)) {
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
