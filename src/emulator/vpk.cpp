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

#include <kernel/load_self.h>
#include <kernel/state.h>

#include <io/state.h>
#include <util/log.h>
#include <util/string_convert.h>

#include <cassert>
#include <cstring>
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

static bool read_file_from_zip(Buffer &buf, FILE *&vpk_fp, const char *file, const ZipPtr zip) {
    if (!mz_zip_reader_init_cfile(zip.get(), vpk_fp, 0, 0)) {
        return false;
    }

    const int file_index = mz_zip_reader_locate_file(zip.get(), file, nullptr, 0);
    if (file_index < 0) {
        return false;
    }

    if (!mz_zip_reader_extract_file_to_callback(zip.get(), file, &write_to_buffer, &buf, 0)) {
        return false;
    }

    return true;
}

bool load_vpk(Ptr<const void> &entry_point, std::string &game_title, std::string &title_id, KernelState &kernel, IOState &io, MemState &mem, const std::wstring &path) {
    const ZipPtr zip(new mz_zip_archive, delete_zip);
    std::memset(zip.get(), 0, sizeof(*zip));

    {
        FILE *vpk_fp;

#ifdef WIN32
        if (_wfopen_s(&vpk_fp, path.c_str(), L"rb")) {
#else
        if (!(vpk_fp = fopen(wide_to_utf(path).c_str(), "rb"))) {
#endif
            return false;
        }
        Buffer libc;
        if (read_file_from_zip(libc, vpk_fp, "sce_module/libc.suprx", zip)) {
            if (load_self(entry_point, kernel, mem, libc.data(), "app0:sce_module/libc.suprx") == 0) {
                LOG_INFO("LIBC loaded");
            }
        }
    }

    std::memset(zip.get(), 0, sizeof(*zip));

    FILE *vpk_fp;

#ifdef WIN32
    if (_wfopen_s(&vpk_fp, path.c_str(), L"rb")) {
#else
    if (!(vpk_fp = fopen(wide_to_utf(path).c_str(), "rb"))) {
#endif
        return false;
    }

    Buffer eboot;
    if (!read_file_from_zip(eboot, vpk_fp, "eboot.bin", zip)) {
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

    if (load_self(entry_point, kernel, mem, eboot.data(), "app0:eboot.bin") < 0) {
        return false;
    }

    io.vpk = zip;

    return true;
}
