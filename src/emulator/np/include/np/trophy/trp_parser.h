// Vita3K emulator project
// Copyright (C) 2019 Vita3K team
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

namespace emu::np::trophy {

struct TRPEntry {
    std::string filename;
    std::uint64_t offset;
    std::uint64_t size;
};

struct TRPFile {
    std::vector<TRPEntry> entries;
    std::istream *stream;

    bool header_parse();
    explicit TRPFile(std::istream *stream);

    bool get_entry_data(const std::uint32_t idx, std::ostream *out);
};

}