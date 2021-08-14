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

#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace np::trophy {

struct TRPEntry {
    std::string filename;
    std::uint64_t offset;
    std::uint64_t size;
};

using TRPSeekFunc = std::function<bool(int amount)>;
using TRPReadFunc = std::function<bool(void *source, std::uint32_t amount)>;
using TRPWriteFunc = std::function<bool(void *dest, std::uint32_t amount)>;

struct TRPFile {
    std::vector<TRPEntry> entries;

    TRPSeekFunc seek_func;
    TRPReadFunc read_func;

    bool header_parse();

    explicit TRPFile() = default;

    bool get_entry_data(const std::uint32_t idx, TRPWriteFunc write_func);
    const std::int32_t search_file(const char *name);
};

} // namespace np::trophy
