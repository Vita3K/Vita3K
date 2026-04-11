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

#include <modules/tai_config.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace tai {

namespace {

/// Trim leading and trailing whitespace (including CR) from a string.
static std::string trim(std::string s) {
    // Erase leading whitespace.
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) {
        return !std::isspace(c);
    }));
    // Erase trailing whitespace.
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) {
        return !std::isspace(c);
    }).base(), s.end());
    return s;
}

/// Return true if the string looks like a Vita title ID.
/// Title IDs are exactly 9 characters: 4 uppercase letters + 5 digits
/// (e.g. "PCSX00000").
static bool is_title_id(const std::string &s) {
    if (s.size() != 9)
        return false;
    for (int i = 0; i < 4; ++i) {
        if (!std::isupper(static_cast<unsigned char>(s[i])))
            return false;
    }
    for (int i = 4; i < 9; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i])))
            return false;
    }
    return true;
}

} // anonymous namespace

PluginConfig parse(const std::string &content) {
    PluginConfig cfg;

    // Section enum to avoid repeated string comparisons per line.
    enum class Section {
        None,
        Kernel,
        All,
        Title,
    };

    Section current_section = Section::None;
    std::string current_title;

    std::istringstream stream(content);
    std::string raw_line;
    while (std::getline(stream, raw_line)) {
        const std::string line = trim(raw_line);

        // Skip empty lines and comments.
        if (line.empty() || line[0] == '#')
            continue;

        // Check for section headers.
        if (line == "*KERNEL") {
            current_section = Section::Kernel;
            continue;
        }
        if (line == "*ALL") {
            current_section = Section::All;
            continue;
        }
        if (is_title_id(line)) {
            current_section = Section::Title;
            current_title = line;
            continue;
        }

        // Everything else that starts with a known device prefix is a plugin path.
        // We accept any line that contains ':' as a potential path — this matches
        // the "device:path" convention used on the Vita (ux0:, ur0:, vs0:, etc.).
        if (line.find(':') == std::string::npos)
            continue; // Not a path, skip.

        switch (current_section) {
        case Section::Kernel:
            cfg.kernel_plugins.push_back(line);
            break;
        case Section::All:
            cfg.all_plugins.push_back(line);
            break;
        case Section::Title:
            cfg.title_plugins[current_title].push_back(line);
            break;
        case Section::None:
            // Path before any section header — ignore.
            break;
        }
    }

    return cfg;
}

std::vector<std::string> plugins_for_title(const PluginConfig &config, const std::string &title_id) {
    std::vector<std::string> result;

    // *ALL plugins first.
    result.insert(result.end(), config.all_plugins.begin(), config.all_plugins.end());

    // Title-specific plugins appended after.
    const auto it = config.title_plugins.find(title_id);
    if (it != config.title_plugins.end())
        result.insert(result.end(), it->second.begin(), it->second.end());

    return result;
}

} // namespace tai
