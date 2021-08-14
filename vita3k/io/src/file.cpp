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

#include <io/file.h>

ReadOnlyInMemFile::ReadOnlyInMemFile() = default;

ReadOnlyInMemFile::ReadOnlyInMemFile(const char *data, size_t size)
    : buf(data, data + size) {
}

char *ReadOnlyInMemFile::alloc_data(size_t bufsize) {
    buf.resize(bufsize);
    return buf.data();
}

size_t ReadOnlyInMemFile::read(void *ibuf, size_t size) {
    size_t res = size;

    if (currentPos + size > buf.size()) {
        res = buf.size() - currentPos;
    }

    memcpy(ibuf, buf.data() + currentPos, res);
    currentPos += res;

    return res;
}

const char *ReadOnlyInMemFile::data() {
    return buf.data();
}

bool ReadOnlyInMemFile::seek(int offset, int origin) {
    if ((offset < 0) || !valid()) {
        return false;
    }

    if (origin == SCE_SEEK_SET) {
        currentPos = offset;
        return true;
    }

    else if (origin == SCE_SEEK_CUR) {
        currentPos += offset;
        return true;
    }

    currentPos = buf.size() + offset;
    return true;
}
