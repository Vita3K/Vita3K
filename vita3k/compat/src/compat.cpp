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

#include <config/state.h>

#include <compat/functions.h>
#include <compat/state.h>

#include <dialog/state.h>
#include <emuenv/state.h>
#include <gui/state.h>

#include <util/net_utils.h>

#include <miniz.h>
#include <pugixml.hpp>

enum LabelIdState {
    Nothing = 1260231569, // 0x4b1d9b91
    Bootable = 1344750319, // 0x502742ef
    Intro = 1260231381, // 0x4B9F5E5D
    Menu = 1344751053, // 0x4F1B9135
    Ingame_Less = 1344752299, // 0x4F7B6B3B
    Ingame_More = 1260231985, // 0x4B2A9819
    Playable = 920344019, // 0x36db55d3
};

namespace compat {

static std::string db_updated_at;
static const uint32_t db_version = 1;
static uint32_t db_issue_count = 0;

std::map<CompatibilityState, ImVec4> CompatState::compat_color{
    { UNKNOWN, ImVec4(0.54f, 0.54f, 0.54f, 1.f) },
    { NOTHING, ImVec4(1.00f, 0.00f, 0.00f, 1.f) }, // #ff0000
    { BOOTABLE, ImVec4(0.39f, 0.12f, 0.62f, 1.f) }, // #621fa5
    { INTRO, ImVec4(0.77f, 0.08f, 0.52f, 1.f) }, // #c71585
    { MENU, ImVec4(0.11f, 0.46f, 0.85f, 1.f) }, // #1d76db
    { INGAME_LESS, ImVec4(0.88f, 0.54f, 0.12f, 1.f) }, // #e08a1e
    { INGAME_MORE, ImVec4(1.00f, 0.84f, 0.00f, 1.f) }, // #ffd700
    { PLAYABLE, ImVec4(0.05f, 0.54f, 0.09f, 1.f) }, // #0e8a16
};

static bool extract_zip_file(const char *zip_filename, const fs::path &output_path) {
    // Open the ZIP file for reading
    mz_zip_archive zip_archive{};
    if (!mz_zip_reader_init_file(&zip_archive, zip_filename, 0)) {
        LOG_ERROR("Failed to initialize ZIP archive for reading");
        return false;
    }

    // Get the number of files in the ZIP archive
    mz_uint num_files = mz_zip_reader_get_num_files(&zip_archive);
    for (mz_uint i = 0; i < num_files; i++) {
        // Get information about the current file in the ZIP archive
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) {
            LOG_ERROR("Failed to get file information from ZIP archive");
            mz_zip_reader_end(&zip_archive);
            return false;
        }

        // Extract the file from the ZIP archive
        fs::path output_file_path = output_path / file_stat.m_filename;
        if (!mz_zip_reader_extract_to_file(&zip_archive, i, fs_utils::path_to_utf8(output_file_path).c_str(), 0)) {
            LOG_ERROR("Failed to extract file from ZIP archive to path: {}", output_file_path);
            mz_zip_reader_end(&zip_archive);
            return false;
        }
    }

    // Close the ZIP archive
    mz_zip_reader_end(&zip_archive);
    fs::remove(zip_filename);

    return true;
}

bool load_app_compat_db(GuiState &gui, EmuEnvState &emuenv) {
    const auto app_compat_db_path = emuenv.cache_path / "app_compat_db.xml";
    if (!fs::exists(app_compat_db_path)) {
        LOG_WARN("Compatibility database not found at {}.", app_compat_db_path);
        return false;
    }

    // Parse and load file of compatibility database
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(app_compat_db_path.c_str());
    if (!result) {
        LOG_ERROR("Compatibility database {} could not be loaded: {}", app_compat_db_path, result.description());
        return false;
    }

    // Check compatibility database version
    const auto compatibility = doc.child("compatibility");
    const auto version = compatibility.attribute("version").as_uint();
    db_issue_count = compatibility.attribute("issue_count").as_uint();
    if (db_version != version) {
        LOG_WARN("Compatibility database version {} is outdated, download it again.", version);
        return update_app_compat_db(gui, emuenv);
    }

    // Check if compatibility database is up to date in first load
    if (db_updated_at.empty()) {
        db_updated_at = compatibility.attribute("iso_db_updated_at").as_string();
        if (update_app_compat_db(gui, emuenv))
            return true;
    }

    // Clear old compat database
    gui.compat.compat_db_loaded = false;
    gui.compat.app_compat_db.clear();

    //  Load compatibility database
    for (const auto &app : doc.child("compatibility")) {
        const std::string title_id = app.attribute("title_id").as_string();
        const auto issue_id = app.child("issue_id").text().as_uint();

        // Check if title ID is valid
        if ((title_id.find("PCS") == std::string::npos) && (title_id != "NPXS10007")) {
            LOG_WARN_IF(emuenv.cfg.log_compat_warn, "Title ID {} is invalid. Please check GitHub issue {} and verify it!", title_id, issue_id);
            continue;
        }

        auto state = CompatibilityState::UNKNOWN;
        const auto labels = app.child("labels");
        if (!labels.empty()) {
            for (const auto &label : labels) {
                const auto label_id = static_cast<LabelIdState>(label.text().as_uint());
                switch (label_id) {
                case LabelIdState::Nothing: state = NOTHING; break;
                case LabelIdState::Bootable: state = BOOTABLE; break;
                case LabelIdState::Intro: state = INTRO; break;
                case LabelIdState::Menu: state = MENU; break;
                case LabelIdState::Ingame_Less: state = INGAME_LESS; break;
                case LabelIdState::Ingame_More: state = INGAME_MORE; break;
                case LabelIdState::Playable: state = PLAYABLE; break;
                default: break;
                }
            }
        }
        const auto updated_at = app.child("updated_at").text().as_llong();

        // Check if app missing a status label
        if (state == UNKNOWN)
            LOG_WARN_IF(emuenv.cfg.log_compat_warn, "App with Title ID {} has an issue but no status label. Please check GitHub issue {} and request a status label be added.", title_id, issue_id);

        // Check if app already exists in compatibility database
        if (gui.compat.app_compat_db.contains(title_id))
            LOG_WARN_IF(emuenv.cfg.log_compat_warn, "App with Title ID {} already exists in compatibility database. Please check and close GitHub issue {}.", title_id, gui.compat.app_compat_db[title_id].issue_id);

        gui.compat.app_compat_db[title_id] = { issue_id, state, updated_at };
    }

    // Update compatibility status of all user apps
    for (auto &apps : gui.app_selector.vita_apps) {
        for (auto &app : apps.second)
            app.compat = gui.compat.app_compat_db.contains(app.title_id) ? gui.compat.app_compat_db[app.title_id].state : CompatibilityState::UNKNOWN;
    }

    return !gui.compat.app_compat_db.empty();
}

static const std::string latest_link = "https://api.github.com/repos/Vita3K/compatibility/releases/latest";
static const std::string app_compat_db_link = "https://github.com/Vita3K/compatibility/releases/download/compat_db/app_compat_db.xml.zip";

bool update_app_compat_db(GuiState &gui, EmuEnvState &emuenv) {
    const auto app_compat_db_path = emuenv.cache_path / "app_compat_db.xml";
    gui.info_message.function = SPDLOG_FUNCTION;

    auto &lang = gui.lang.compat_db;
    auto &common = emuenv.common_dialog.lang.common;

    // Get current date of last compat database updated at
    const auto updated_at = net_utils::get_web_regex_result(latest_link, std::regex(R"(Last updated: (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}Z))"));
    if (updated_at.empty()) {
        gui.info_message.title = common["error"];
        gui.info_message.level = spdlog::level::err;
        gui.info_message.msg = lang["get_failed"];
        return false;
    }

    // Check if database is up to date
    if (db_updated_at == updated_at) {
        LOG_INFO("Applications compatibility database is up to date.");
        return false;
    }

    const auto compat_db_exist = fs::exists(app_compat_db_path);

    LOG_INFO("Applications compatibility database is {}, attempting to download latest updated at: {}", compat_db_exist ? "outdated" : "missing", updated_at);

    const auto new_app_compat_db_path = emuenv.cache_path / "new_app_compat_db.xml.zip";

    if (!net_utils::download_file(app_compat_db_link, new_app_compat_db_path.string())) {
        gui.info_message.title = common["error"];
        gui.info_message.level = spdlog::level::err;
        gui.info_message.msg = fmt::format(fmt::runtime(lang["download_failed"]), updated_at);
        fs::remove(new_app_compat_db_path);
        return false;
    }

    // Unpack new database and replace old database
    if (!extract_zip_file(fs_utils::path_to_utf8(new_app_compat_db_path).c_str(), emuenv.cache_path))
        return false;

    const auto old_db_updated_at = db_updated_at;
    const auto old_compat_db_count = db_issue_count;
    db_updated_at = updated_at;

    gui.compat.compat_db_loaded = load_app_compat_db(gui, emuenv);
    if (!gui.compat.compat_db_loaded) {
        gui.info_message.title = common["error"];
        gui.info_message.level = spdlog::level::err;
        gui.info_message.msg = fmt::format(fmt::runtime(lang["load_failed"]), updated_at);
        db_updated_at.clear();
        return false;
    }

    gui.info_message.title = gui.lang.app_context.info["title"];
    gui.info_message.level = spdlog::level::info;

    if (compat_db_exist) {
        const auto dif = static_cast<int32_t>(db_issue_count - old_compat_db_count);
        if (!old_db_updated_at.empty() && dif > 0)
            gui.info_message.msg = fmt::format(fmt::runtime(lang["new_app_listed"]), old_db_updated_at, db_updated_at, dif, gui.compat.app_compat_db.size());
        else
            gui.info_message.msg = fmt::format(fmt::runtime(lang["app_listed"]), old_db_updated_at, db_updated_at, gui.compat.app_compat_db.size());
    } else
        gui.info_message.msg = fmt::format(fmt::runtime(lang["download_app_listed"]), db_updated_at, gui.compat.app_compat_db.size());

    return true;
}

} // namespace compat
