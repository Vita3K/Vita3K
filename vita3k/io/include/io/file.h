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

#include <io/util.h>

#include <cassert>
#include <cstring>
#include <vector>

class ReadOnlyInMemFile {
    std::vector<char> buf;
    size_t currentPos = 0;

public:
    ReadOnlyInMemFile();
    ReadOnlyInMemFile(const char *data, size_t size);

    char *alloc_data(size_t bufsize);

    size_t tell() const { return currentPos; }
    size_t size() const { return buf.size(); }

    bool valid() const { return !buf.empty(); }
    operator bool() const { return valid(); }

    size_t read(void *ibuf, size_t size);
    const char *data();
    bool seek(int offset, int origin);
};
