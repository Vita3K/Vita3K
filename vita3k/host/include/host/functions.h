// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

struct SfoFile;

void install_pup(const std::wstring &pref_path, const std::string &pup_path);

namespace sfo {
bool get_data_by_id(std::string &out_data, SfoFile &file, int id);
bool get_data_by_key(std::string &out_data, SfoFile &file, const std::string &key);
bool load(SfoFile &sfile, const std::vector<uint8_t> &content);
} // namespace sfo
