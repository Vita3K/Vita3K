// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#pragma once

#include <string>
#include <vector>

namespace string_utils {

std::vector<std::string> split_string(const std::string &str, char delimiter);

std::wstring utf_to_wide(const std::string &str);
std::string wide_to_utf(const std::wstring &str);
std::string utf16_to_utf8(const std::u16string &str);
std::u16string utf8_to_utf16(const std::string &str);
std::string remove_special_chars(std::string str);
void replace(std::string &str, const std::string &in, const std::string &out);
std::basic_string<uint8_t> string_to_byte_array(std::string string);
std::string toupper(const std::string &s);
std::string tolower(const std::string &s);

} // namespace string_utils
