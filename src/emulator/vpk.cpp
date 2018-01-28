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

#include "vpk.h"

#include "load_self.h"

#include <io/state.h>

#include <cassert>
#include <vector>

typedef std::vector<uint8_t> Buffer;

static void delete_zip(mz_zip_archive *zip) {
    mz_zip_reader_end(zip);
    delete zip;
}

static size_t write_to_buffer(void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n) {
    Buffer *const buffer = static_cast<Buffer *>(pOpaque);
    assert(file_ofs == buffer->size());
    const uint8_t *const first = static_cast<const uint8_t *>(pBuf);
    const uint8_t *const last = &first[n];
    buffer->insert(buffer->end(), first, last);

    return n;
}

bool load_vpk(Ptr<const void> &entry_point, IOState &io, MemState &mem, const char *path) {
    const ZipPtr zip(new mz_zip_archive, delete_zip);
    std::memset(zip.get(), 0, sizeof(*zip));

    if (!mz_zip_reader_init_file(zip.get(), path, 0)) {
        return false;
    }

    const int eboot_index = mz_zip_reader_locate_file(zip.get(), "eboot.bin", nullptr, 0);
    if (eboot_index < 0) {
        return false;
    }

    Buffer eboot;
    if (!mz_zip_reader_extract_file_to_callback(zip.get(), "eboot.bin", &write_to_buffer, &eboot, 0)) {
        return false;
    }

    if (!load_self(entry_point, mem, eboot.data())) {
        return false;
    }

    io.vpk = zip;

    return true;
}
