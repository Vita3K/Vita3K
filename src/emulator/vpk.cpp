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
#include "sfo.h"

#include "load_self.h"

#include <io/state.h>
#include <util/string_convert.h>
#include <util/log.h>
#include <util/v3k_assert.h>

#include <cstring>
#include <vector>

typedef std::vector<uint8_t> Buffer;

static void delete_zip(mz_zip_archive *zip) {
    mz_zip_reader_end(zip);
    delete zip;
}

static size_t write_to_buffer(void *pOpaque, mz_uint64 file_ofs, const void *pBuf, size_t n) {
    Buffer *const buffer = static_cast<Buffer *>(pOpaque);
    v3k_assert(file_ofs == buffer->size());
    const uint8_t *const first = static_cast<const uint8_t *>(pBuf);
    const uint8_t *const last = &first[n];
    buffer->insert(buffer->end(), first, last);

    return n;
}

bool load_vpk(Ptr<const void> &entry_point, std::string& game_title, std::string& title_id, IOState &io, MemState &mem, const std::wstring& path) {
    const ZipPtr zip(new mz_zip_archive, delete_zip);
    std::memset(zip.get(), 0, sizeof(*zip));

    FILE* vpk_fp;

#ifdef WIN32
    if (_wfopen_s(&vpk_fp, path.c_str(), L"rb")) {
#else
    if (!(vpk_fp = fopen(wide_to_utf(path).c_str(), "rb"))) {
#endif
        return false;
    }

    if (!mz_zip_reader_init_cfile(zip.get(), vpk_fp, 0, 0)) {
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

    Buffer params;
    if (!mz_zip_reader_extract_file_to_callback(zip.get(), "sce_sys/param.sfo", &write_to_buffer, &params, 0)) {
        return false;
    }

    SfoFile sfo_file;
    load_sfo(sfo_file, params);

    game_title = find_data(sfo_file, "TITLE");
    title_id = find_data(sfo_file, "TITLE_ID");

    if (!load_self(entry_point, mem, eboot.data())) {
        return false;
    }

    io.vpk = zip;

    return true;
}
