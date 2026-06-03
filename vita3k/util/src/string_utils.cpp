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

#include <util/string_utils.h>

#include <util/log.h>

#include <algorithm>
#include <charconv>
#include <codecvt>
#include <locale>
#include <ranges>

namespace string_utils {

std::wstring utf_to_wide(const std::string &str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.from_bytes(str);
}

std::string wide_to_utf(const std::wstring &str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.to_bytes(str);
}

std::string trim_copy(std::string_view str) {
    return str
        | std::views::drop_while([](unsigned char ch) { return std::isspace(ch); })
        | std::views::reverse
        | std::views::drop_while([](unsigned char ch) { return std::isspace(ch); })
        | std::views::reverse
        | std::ranges::to<std::string>();
}

std::string remove_special_chars(std::string str) {
    for (char &c : str) {
        switch (c) {
        case '/':
        case '\\':
        case ':':
        case '?':
        case '"':
        case '<':
        case '>':
        case '|':
        case '*':
            c = '_';
            break;
        default:
            continue;
        }
    }
    return str;
}

// Based on: https://stackoverflow.com/a/23135441
// Search and replace "in" with "out" in the given string
void replace(std::string &str, std::string_view in, std::string_view out) {
    size_t pos = 0;
    while ((pos = str.find(in, pos)) != std::string::npos) {
        str.replace(pos, in.length(), out);
        pos += out.length();
    }
}

std::vector<uint8_t> string_to_byte_array(std::string_view string) {
    if (string.length() % 2 != 0) {
        LOG_ERROR("Invalid hex string: {:?}", string);
        return {};
    }

    std::vector<uint8_t> hex_bytes;
    hex_bytes.reserve(string.length() / 2);
    for (const char *p = string.data(); p < std::to_address(string.end()); p += 2) {
        uint8_t byte = 0;
        std::from_chars(p, p + 2, byte, 16);
        hex_bytes.push_back(byte);
    }
    return hex_bytes;
}

#ifdef _MSC_VER
std::string utf16_to_utf8(const std::u16string &str) {
    std::wstring_convert<std::codecvt_utf8_utf16<int16_t>, int16_t> myconv;
    auto p = reinterpret_cast<const int16_t *>(str.data());
    return myconv.to_bytes(p, p + str.size());
}

std::u16string utf8_to_utf16(const std::string &str) {
    static_assert(sizeof(std::wstring::value_type) == sizeof(std::u16string::value_type),
        "std::wstring and std::u16string are expected to have the same character size");

    std::wstring_convert<std::codecvt_utf8_utf16<int16_t>, int16_t> myconv;
    const char *p = str.data();
    auto a = myconv.from_bytes(p, p + str.size());
    return std::u16string(a.begin(), a.end());
}
#else
std::string utf16_to_utf8(const std::u16string &str) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> myconv;
    return myconv.to_bytes(str);
}

std::u16string utf8_to_utf16(const std::string &str) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> myconv;
    return myconv.from_bytes(str);
}
#endif

std::string toupper(std::string s) {
    std::ranges::transform(s, s.begin(), [](unsigned char c) { return std::toupper(c); });
    return s;
}

std::string tolower(std::string s) {
    std::ranges::transform(s, s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

int stoi_def(const std::string &str, int default_value, const char *name) {
    try {
        return std::stoi(str);
    } catch (std::invalid_argument &_) {
        LOG_ERROR("Invalid {}: \"{}\"", name, str);
    } catch (std::out_of_range &_) {
        LOG_ERROR("Out of range {}: \"{}\"", name, str);
    }
    return default_value;
}

} // namespace string_utils
