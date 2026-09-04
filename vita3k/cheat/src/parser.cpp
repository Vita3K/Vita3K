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

#include <cheat/cheat.h>

#include <util/log.h>
#include <util/string_utils.h>

#include <fstream>
#include <optional>
#include <sstream>

namespace cheat {

namespace {

constexpr std::string_view utf8_bom = "\xEF\xBB\xBF";

std::string clean_line(std::string_view line) {
    if (line.starts_with(utf8_bom))
        line.remove_prefix(utf8_bom.size());

    return string_utils::trim_copy(line);
}

bool is_comment(std::string_view line) {
    return line.starts_with('#') || line.starts_with(';') || line.starts_with("//");
}

// `_S <title id>` opens the section of one game inside a combined database.
bool is_section_declaration(std::string_view line) {
    return (line.size() > 2) && (line[0] == '_') && ((line[1] == 'S') || (line[1] == 's'))
        && ((line[2] == ' ') || (line[2] == '\t'));
}

// Title id of a `_S` line, the rest of the line is a game name some databases add.
std::string section_title_id(std::string_view line) {
    const std::string rest = string_utils::trim_copy(line.substr(2));
    return string_utils::toupper(rest.substr(0, rest.find_first_of(" \t")));
}

// A cheat declaration is `_V0 <name>` (off) or `_V1 <name>` (on when the game boots).
bool is_cheat_declaration(std::string_view line) {
    return line.size() >= 3 && line[0] == '_' && (line[1] == 'V' || line[1] == 'v')
        && (line[2] == '0' || line[2] == '1');
}

bool parse_hex(std::string_view token, uint32_t &value) {
    if (token.starts_with('$'))
        token.remove_prefix(1);
    else if (token.starts_with("0x") || token.starts_with("0X"))
        token.remove_prefix(2);

    if (token.empty() || token.size() > 8)
        return false;

    uint32_t result = 0;
    for (const char c : token) {
        uint32_t digit;
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;
        else
            return false;

        result = (result << 4) | digit;
    }

    value = result;
    return true;
}

// `$XXXX AAAAAAAA BBBBBBBB`
bool parse_code_line(const std::string &line, CodeLine &code) {
    std::istringstream stream(line);
    std::string control_token;
    std::string first_token;
    std::string second_token;
    if (!(stream >> control_token >> first_token >> second_token))
        return false;

    uint32_t control = 0;
    if (!parse_hex(control_token, control) || (control > 0xFFFF))
        return false;
    if (!parse_hex(first_token, code.first) || !parse_hex(second_token, code.second))
        return false;

    code.control = static_cast<uint16_t>(control);
    return true;
}

} // namespace

// True when `path` is a combined database holding a section for this game.
static bool has_section(const fs::path &path, const std::string &wanted) {
    std::ifstream stream(path.c_str());
    if (!stream.is_open())
        return false;

    std::string raw;
    while (std::getline(stream, raw)) {
        const std::string line = clean_line(raw);
        if (is_section_declaration(line) && (section_title_id(line) == wanted))
            return true;
    }

    return false;
}

fs::path find_cheat_file(const fs::path &cheats_dir, const std::string &title_id) {
    if (title_id.empty() || !fs::is_directory(cheats_dir))
        return {};

    const auto wanted = string_utils::toupper(title_id);
    std::vector<fs::path> databases;

    for (const auto &entry : fs::directory_iterator(cheats_dir)) {
        if (!fs::is_regular_file(entry.path()))
            continue;

        const auto filename = string_utils::toupper(fs_utils::path_to_utf8(entry.path().filename()));
        // The databases ship `<title id>.psv`, but plain `.txt` files are common too.
        const bool per_title = filename.ends_with(".PSV") || filename.ends_with(".TXT");
        if (per_title && filename.starts_with(wanted))
            return entry.path();

        // `cheat.db` and the like hold every game in one file, keyed by `_S <title id>`.
        if (filename.ends_with(".DB") || filename.ends_with(".TXT"))
            databases.push_back(entry.path());
    }

    // Reading those is only worth it once no file is named after the title.
    for (const auto &database : databases) {
        if (has_section(database, wanted))
            return database;
    }

    return {};
}

CheatFile parse_cheat_file(const fs::path &path, const std::string &title_id) {
    CheatFile file;
    file.path = path;
    file.title_id = title_id;

    std::ifstream stream(path.c_str());
    if (!stream.is_open()) {
        LOG_ERROR("Failed to open cheat file {}", path);
        return file;
    }

    std::optional<size_t> current;
    std::string raw;
    size_t line_number = 0;
    size_t code_count = 0;
    const std::string wanted = string_utils::toupper(title_id);
    // A file without any `_S` line holds the cheats of a single game, so all of them count.
    bool in_section = true;
    while (std::getline(stream, raw)) {
        ++line_number;

        const std::string line = clean_line(raw);
        if (line.empty())
            continue;

        if (is_section_declaration(line)) {
            in_section = section_title_id(line) == wanted;
            current.reset();
            // Whatever came before the section belongs to the database, not to this game.
            file.header.clear();
            continue;
        }

        if (!in_section)
            continue;

        if (is_comment(line)) {
            if (file.header.empty())
                file.header = string_utils::trim_copy(std::string_view(line).substr(line.starts_with("//") ? 2 : 1));
            continue;
        }

        if (is_cheat_declaration(line)) {
            Cheat cheat;
            cheat.enabled_on_boot = line[2] == '1';
            cheat.name = string_utils::trim_copy(std::string_view(line).substr(3));
            cheat.line_number = line_number;
            if (cheat.name.empty())
                cheat.name = fmt::format("Cheat {}", file.cheats.size() + 1);

            file.cheats.push_back(std::move(cheat));
            current = file.cheats.size() - 1;
            continue;
        }

        if (line[0] != '$') {
            LOG_DEBUG("Ignoring unknown line {} of cheat file {}: {}", line_number, path, line);
            continue;
        }

        if (!current) {
            LOG_WARN("Ignoring code line {} of cheat file {}, it does not belong to any cheat", line_number, path);
            continue;
        }

        CodeLine code;
        if (!parse_code_line(line, code)) {
            LOG_ERROR("Failed to parse code line {} of cheat file {}: {}", line_number, path, line);
            continue;
        }

        file.cheats[*current].lines.push_back(code);
        ++code_count;
    }

    // A declaration without any code line would silently do nothing, drop it.
    std::erase_if(file.cheats, [&](const Cheat &cheat) {
        if (!cheat.lines.empty())
            return false;
        LOG_WARN("Cheat '{}' of {} has no code line, ignoring it", cheat.name, path);
        return true;
    });

    LOG_INFO("Loaded {} cheats ({} codes) for {} from {}", file.cheats.size(), code_count, title_id, path);

    return file;
}

bool save_cheat_file(const CheatFile &file) {
    if (file.path.empty())
        return false;

    std::vector<std::string> lines;
    {
        std::ifstream stream(file.path.c_str(), std::ios::binary);
        if (!stream.is_open()) {
            LOG_ERROR("Failed to open cheat file {} for reading", file.path);
            return false;
        }

        // Read the file as it is, only the digit of the `_V` markers is rewritten.
        std::string raw;
        while (std::getline(stream, raw))
            lines.push_back(std::move(raw));
    }

    // Declaration line numbers stay valid even though empty cheats were dropped while parsing.
    for (const auto &cheat : file.cheats) {
        if ((cheat.line_number == 0) || (cheat.line_number > lines.size())) {
            LOG_WARN("Cheat file {} changed on disk since it was loaded, not saving", file.path);
            return false;
        }

        std::string &line = lines[cheat.line_number - 1];
        const auto marker = line.find("_V");
        if ((marker == std::string::npos) || !is_cheat_declaration(std::string_view(line).substr(marker))) {
            LOG_WARN("Cheat file {} changed on disk since it was loaded, not saving", file.path);
            return false;
        }

        line[marker + 2] = cheat.enabled ? '1' : '0';
    }

    std::ofstream out(file.path.c_str(), std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        LOG_ERROR("Failed to open cheat file {} for writing", file.path);
        return false;
    }

    for (const auto &line : lines)
        out << line << '\n';

    LOG_INFO("Saved the state of {} cheats to {}", file.cheats.size(), file.path);

    return true;
}

} // namespace cheat
