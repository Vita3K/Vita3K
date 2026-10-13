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

#include <app/state.h>
#include <compat/functions.h>
#include <dialog/state.h>
#include <gui-imgui/functions.h>
#include <gui-imgui/popups.h>
#include <io/state.h>

#include <util/fs.h>
#include <util/log.h>
#include <util/net_utils.h>

#include <ios>
#include <span>
#include <utility>

namespace gui {

static bool update_app_compat_db(GuiState &gui, EmuEnvState &emuenv, compat::CompatState &state, const fs::path &cache_path) {
    std::map<std::string, std::string> lang;
    std::string error_title;
    std::string updated_title;
    {
        std::lock_guard lock(gui.render_mutex);
        lang = gui.lang.compat_db;
        error_title = gui.lang.common.main.at("error");
        updated_title = gui.lang.compatibility.name;
    }

    static constexpr const char *latest_link = "https://api.github.com/repos/Vita3K/compatibility/releases/latest";
    static constexpr const char *app_compat_db_link = "https://github.com/Vita3K/compatibility/releases/download/compat_db/app_compat_db.xml.zip";

    const auto version_response = net_utils::get_web_response(latest_link);
    if (version_response.empty()) {
        LOG_WARN("Could not check for compatibility database updates");
        init_info_message({
            .function = "compatibility database update",
            .title = error_title,
            .level = spdlog::level::err,
            .msg = lang.at("get_failed"),
        });
        return false;
    }

    const auto update = compat::parse_ver_resp(state, version_response);
    if (!update) {
        init_info_message({
            .function = "compatibility database update",
            .title = error_title,
            .level = spdlog::level::err,
            .msg = lang.at("get_failed"),
        });
        return false;
    }
    if (!update->needs_update) {
        LOG_INFO("Applications compatibility database is up to date.");
        return false;
    }

    const auto app_compat_db_path = cache_path / "app_compat_db.xml";
    const bool compat_db_exist = fs::exists(app_compat_db_path);
    const auto new_app_compat_db_path = cache_path / "new_app_compat_db.xml.zip";
    const auto archive_path_utf8 = fs_utils::path_to_utf8(new_app_compat_db_path);
    if (!net_utils::download_file(app_compat_db_link, archive_path_utf8)) {
        LOG_WARN("Could not download compatibility database version {}", update->latest_ver);
        fs::remove(new_app_compat_db_path);
        init_info_message({
            .function = "compatibility database update",
            .title = error_title,
            .level = spdlog::level::err,
            .msg = fmt::format(fmt::runtime(lang.at("download_failed")), update->latest_ver),
        });
        return false;
    }

    std::ifstream archive(new_app_compat_db_path, std::ios::binary | std::ios::ate);
    if (!archive) {
        LOG_ERROR("Could not open downloaded compatibility database at {}", archive_path_utf8);
        fs::remove(new_app_compat_db_path);
        init_info_message({
            .function = "compatibility database update",
            .title = error_title,
            .level = spdlog::level::err,
            .msg = fmt::format(fmt::runtime(lang.at("download_failed")), update->latest_ver),
        });
        return false;
    }

    const auto archive_size = static_cast<std::streamoff>(archive.tellg());
    if (archive_size <= 0) {
        LOG_ERROR("Downloaded compatibility database is empty");
        archive.close();
        fs::remove(new_app_compat_db_path);
        init_info_message({
            .function = "compatibility database update",
            .title = error_title,
            .level = spdlog::level::err,
            .msg = fmt::format(fmt::runtime(lang.at("download_failed")), update->latest_ver),
        });
        return false;
    }

    std::vector<uint8_t> archive_data(static_cast<size_t>(archive_size));
    archive.seekg(0);
    if (!archive.read(reinterpret_cast<char *>(archive_data.data()), static_cast<std::streamsize>(archive_size))) {
        LOG_ERROR("Failed to read downloaded compatibility database");
        archive.close();
        fs::remove(new_app_compat_db_path);
        init_info_message({
            .function = "compatibility database update",
            .title = error_title,
            .level = spdlog::level::err,
            .msg = fmt::format(fmt::runtime(lang.at("download_failed")), update->latest_ver),
        });
        return false;
    }
    archive.close();

    const auto old_db_updated_at = state.db_updated_at;
    const auto old_issue_count = state.db_issue_count;
    const auto old_app_count = state.app_compat_db.size();
    const bool installed = compat::install_db(state, std::filesystem::path(cache_path.native()),
        std::span<const uint8_t>(archive_data.data(), archive_data.size()), update->latest_ver);
    fs::remove(new_app_compat_db_path);
    if (!installed) {
        init_info_message({
            .function = "compatibility database update",
            .title = error_title,
            .level = spdlog::level::err,
            .msg = fmt::format(fmt::runtime(lang.at("load_failed")), update->latest_ver),
        });
        return false;
    }

    InfoMessage message{
        .function = "compatibility database update",
        .title = updated_title,
        .level = spdlog::level::info,
    };
    const auto added_app_count = static_cast<int32_t>(state.app_compat_db.size()) - static_cast<int32_t>(old_app_count);
    if (!compat_db_exist)
        message.msg = fmt::format(fmt::runtime(lang.at("download_app_listed")), state.db_updated_at, state.app_compat_db.size());
    else if (!old_db_updated_at.empty() && added_app_count > 0)
        message.msg = fmt::format(fmt::runtime(lang.at("new_app_listed")), old_db_updated_at, state.db_updated_at, added_app_count, state.app_compat_db.size());
    else
        message.msg = fmt::format(fmt::runtime(lang.at("app_listed")), old_db_updated_at, state.db_updated_at, state.app_compat_db.size());
    init_info_message(std::move(message));

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

static bool load_app_compat_db(GuiState &gui, EmuEnvState &emuenv, compat::CompatState &state, const fs::path &cache_path) {
    compat::load_from_disk(state, std::filesystem::path(cache_path.native()));
    return update_app_compat_db(gui, emuenv, state, cache_path);
}

void load_and_update_compat_user_apps(GuiState &gui, EmuEnvState &emuenv) {
    bool expected = false;
    if (!gui.compat_db_update_started.compare_exchange_strong(expected, true))
        return;

    const fs::path cache_path(emuenv.cache_path.native());
    const auto user = emuenv.app.user_list.users.find(emuenv.io.user_id);
    const bool reset_app_list_sort = !emuenv.io.user_id.empty()
        && user != emuenv.app.user_list.users.end()
        && user->second.sort_apps_type == COMPAT;
    auto state = gui.compat;

    try {
        gui.compat_db_update_thread = std::jthread([&gui, &emuenv, state = std::move(state), cache_path, reset_app_list_sort]() mutable {
            try {
                const bool updated = !state.compat_db_loaded
                    ? load_app_compat_db(gui, emuenv, state, cache_path)
                    : update_app_compat_db(gui, emuenv, state, cache_path);
                std::lock_guard lock(gui.render_mutex);
                gui.compat = std::move(state);
                if (updated && reset_app_list_sort)
                    gui.app_selector.is_app_list_sorted = false;
            } catch (const std::exception &e) {
                LOG_ERROR("Failed to update compatibility database: {}", e.what());
            } catch (...) {
                LOG_ERROR("Failed to update compatibility database");
            }
            gui.compat_db_update_started.store(false);
        });
    } catch (const std::exception &e) {
        gui.compat_db_update_started.store(false);
        LOG_ERROR("Failed to start compatibility database update: {}", e.what());
    }
}

ImVec4 compatibility_color(compat::CompatibilityState state) {
    switch (state) {
    case compat::NOTHING: return ImColor(220, 62, 62, 255);
    case compat::BOOTABLE: return ImColor(240, 126, 55, 255);
    case compat::INTRO: return ImColor(240, 174, 55, 255);
    case compat::MENU: return ImColor(220, 207, 58, 255);
    case compat::INGAME_LESS: return ImColor(157, 204, 69, 255);
    case compat::INGAME_MORE: return ImColor(100, 190, 92, 255);
    case compat::PLAYABLE: return ImColor(62, 176, 105, 255);
    default: return ImColor(160, 160, 160, 255);
    }
}

void draw_compatibility_badge(compat::CompatibilityState state, const ImVec2 &position, float compat_radius, float full_compat_radius) {
    const auto color = compatibility_color(state);
    const ImU32 color_u32 = IM_COL32(static_cast<int>(color.x * 255.0f), static_cast<int>(color.y * 255.0f), static_cast<int>(color.z * 255.0f), static_cast<int>(color.w * 255.0f));
    ImGui::SetCursorPos(position);
    ImGui::GetWindowDrawList()->AddCircleFilled(ImGui::GetCursorScreenPos(), full_compat_radius, IM_COL32(0, 0, 0, 255));
    ImGui::GetWindowDrawList()->AddCircleFilled(ImGui::GetCursorScreenPos(), compat_radius, color_u32);
}

} // namespace gui
