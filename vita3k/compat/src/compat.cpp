// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <boost/algorithm/string/trim.hpp>
#include <compat/state.h>

#include <emuenv/state.h>
#include <gui/state.h>

#include <pugixml.hpp>

#ifdef __APPLE__
#include <pwd.h>
#include <unistd.h>
#endif

namespace compat {

static std::string db_updated_at;
static const uint32_t db_version = 1;

bool load_compat_app_db(GuiState &gui, EmuEnvState &emuenv) {
    const auto app_compat_db_path = fs::path(emuenv.base_path) / "cache/app_compat_db.xml";
    if (!fs::exists(app_compat_db_path)) {
        LOG_WARN("Compatibility database not found at {}.", app_compat_db_path.string());
        return false;
    }

    // Parse and load file of compatibility database
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(app_compat_db_path.c_str());
    if (!result) {
        LOG_ERROR("Compatibility database {} could not be loaded: {}", app_compat_db_path.string(), result.description());
        return false;
    }

    // Check compatibility database version
    const auto compatibility = doc.child("compatibility");
    const auto version = compatibility.attribute("version").as_uint();
    if (db_version != version) {
        LOG_WARN("Compatibility database version {} is outdated, download it again.", version);
        return false;
    }

    db_updated_at = compatibility.attribute("db_updated_at").as_string();

    //  Load compatibility database
    for (const auto &app : doc.child("compatibility")) {
        const std::string title_id = app.attribute("title_id").as_string();
        const auto issue_id = app.child("issue_id").text().as_uint();
        auto state = Unknown;
        const auto labels = app.child("labels");
        if (!labels.empty()) {
            for (const auto &label : labels) {
                const auto label_id = static_cast<CompatibilityState>(label.text().as_uint());
                switch (label_id) {
                case Nothing: state = Nothing; break;
                case Bootable: state = Bootable; break;
                case Intro: state = Intro; break;
                case Menu: state = Menu; break;
                case Ingame_Less: state = Ingame_Less; break;
                case Ingame_More: state = Ingame_More; break;
                case Playable: state = Playable; break;
                default: break;
                }
            }
        }
        const auto updated_at = app.child("updated_at").text().as_llong();

        if (state == Unknown)
            LOG_WARN("App with title ID {} has an issue but no status label. Please check GitHub issue {} and request a status label to be added.", title_id, issue_id);

        gui.compat.app_compat_db[title_id] = { issue_id, state, updated_at };
    }

    return !gui.compat.app_compat_db.empty();
}

static std::string get_string_output(const std::string cmd) {
    std::string tmpres = std::tmpnam(nullptr);
    std::string res_cmd = cmd + " >> " + tmpres;
    std::system(res_cmd.c_str());
    std::ifstream res(tmpres, std::ios::in | std::ios::binary);
    std::remove(tmpres.c_str());

    std::string result;
    if (res.is_open()) {
        if (!std::getline(res, result))
            LOG_ERROR("Failed to read from input stream");

        res.close();
    } else
        LOG_ERROR("Input stream is not open");

    boost::trim(result);

    return result;
}

bool update_compat_app_db(GuiState &gui, EmuEnvState &emuenv) {
#ifndef __APPLE__
    const auto compat_db_path = fs::path(emuenv.base_path) / "cache/app_compat_db.xml";
#else
    struct passwd *pwent = getpwuid(getuid());
    const auto compat_db_path = fs::path(fmt::format("{}/cache/app_compat_db.yml", pwent->pw_dir));
#endif

    const auto latest_link = "https://api.github.com/repos/Vita3K/compatibility/releases/latest";

#ifdef WIN32
    std::string power_shell_version;
    std::getline(std::ifstream(_popen("powershell (Get-Host).Version.major", "r")), power_shell_version);
    if (power_shell_version.empty() || !std::isdigit(power_shell_version[0]) || (std::stoi(power_shell_version) < 3)) {
        LOG_WARN("You powershell version {} is outdated and incompatible with Vita3K Update, consider to update it", power_shell_version);
        return false;
    }

    const auto github_updated_at_cmd = fmt::format(R"(powershell ((Invoke-RestMethod {}).body.split([Environment]::NewLine) ^| Select-String -Pattern \"Updated at: \") -replace \"Updated at: \")", latest_link);
#else
    const auto github_curl_url = fmt::format("curl -sL {}", latest_link);
    const auto github_updated_at_cmd = github_curl_url + R"( | grep '"body"' | head -n 1 | awk -F "Updated at: " '{print $2}' | awk -F '"' '{print $1}')";
#endif

    // Get current date of last issue updated
    const auto updated_at = get_string_output(github_updated_at_cmd);
    if (updated_at.empty()) {
        LOG_WARN("Failed to get current updated at, try again later {}", updated_at);
        return false;
    }

    // Check if database is up to date
    if (db_updated_at == updated_at) {
        LOG_INFO("Applications compatibility database is up to date.");
        return false;
    }

    const auto compat_db_exist = fs::exists(compat_db_path);

    LOG_INFO("Applications compatibility database is {}, attempting to download latest updated at: {}", compat_db_exist ? "outdated" : "missing", updated_at);

    // Clear old database and remove it if exists
    gui.compat.app_compat_db.clear();
    if (fs::exists(compat_db_path))
        fs::remove(compat_db_path);

    const auto app_compat_db_link = "https://github.com/Vita3K/compatibility/releases/download/compat_db/app_compat_db.xml";

#ifdef WIN32
    const auto download_command = fmt::format("powershell Invoke-WebRequest {} -Outfile {}", app_compat_db_link, compat_db_path.string());
#else
    const auto download_command = fmt::format("curl -L {} -o {}", app_compat_db_link, compat_db_path.string());
#endif

    system(download_command.c_str());

    const auto old_db_updated_at = db_updated_at;

    if (!load_compat_app_db(gui, emuenv) || (db_updated_at != updated_at)) {
        LOG_WARN("Failed to load Applications compatibility database downloaded updated at: {}", updated_at);
        return false;
    }

    if (compat_db_exist)
        LOG_INFO("Successfully updated app compatibility database from {} to {} with a total of {} applications", old_db_updated_at, updated_at, gui.compat.app_compat_db.size());
    else
        LOG_INFO("Successfully downloaded app compatibility database updated at: {} with a total of {} applications", updated_at, gui.compat.app_compat_db.size());

    return true;
}

} // namespace compat
