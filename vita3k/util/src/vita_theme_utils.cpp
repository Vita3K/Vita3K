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

#include <util/vita_theme_utils.h>

#include <util/string_utils.h>

#include <fmt/format.h>

#include <cctype>

namespace vita_theme_utils {

std::string sanitize_theme_id(std::string value) {
    value = string_utils::trim_copy(string_utils::remove_special_chars(std::move(value)));

    std::string sanitized;
    sanitized.reserve(value.size());

    bool last_was_separator = false;
    for (const unsigned char raw_char : value) {
        const char ch = static_cast<char>(raw_char);
        if (std::isalnum(raw_char) || (ch == '-') || (ch == '_')) {
            sanitized.push_back(ch);
            last_was_separator = false;
        } else if (!last_was_separator) {
            sanitized.push_back('_');
            last_was_separator = true;
        }
    }

    while (!sanitized.empty() && ((sanitized.front() == '_') || (sanitized.front() == '-')))
        sanitized.erase(sanitized.begin());
    while (!sanitized.empty() && ((sanitized.back() == '_') || (sanitized.back() == '-')))
        sanitized.pop_back();

    return sanitized;
}

std::string resolve_theme_id(
    const std::string &content_id,
    const std::string &folder_name,
    const std::string &title,
    const std::uint8_t *fallback_hash_data,
    const std::size_t fallback_hash_size) {
    if (const std::string trimmed_content_id = string_utils::trim_copy(content_id); !trimmed_content_id.empty())
        return trimmed_content_id;

    if (const std::string sanitized_folder = sanitize_theme_id(folder_name); !sanitized_folder.empty())
        return sanitized_folder;

    if (const std::string sanitized_title = sanitize_theme_id(title); !sanitized_title.empty())
        return sanitized_title;

    if (fallback_hash_data && fallback_hash_size > 0) {
        uint32_t hash = 2166136261u;
        for (std::size_t index = 0; index < fallback_hash_size; ++index) {
            hash ^= fallback_hash_data[index];
            hash *= 16777619u;
        }
        return fmt::format("theme-{:08X}", hash);
    }

    return "theme";
}

} // namespace vita_theme_utils
