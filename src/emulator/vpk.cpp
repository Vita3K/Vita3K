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

#include <kernel/load_self.h>
#include <kernel/state.h>

#include <host/sfo.h>
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
    const int file_index = mz_zip_reader_locate_file(zip.get(), file, nullptr, 0);
    if (file_index < 0) {
        return false;
    }

    if (!mz_zip_reader_extract_file_to_callback(zip.get(), file, &write_to_buffer, &buf, 0)) {
        return false;
    }

    return true;
}

bool load_vpk(Ptr<const void> &entry_point, std::string &game_title, std::string &title_id, KernelState &kernel, IOState &io, MemState &mem, SfoFile &sfo_file, const std::wstring &path) {
    const ZipPtr zip(new mz_zip_archive, delete_zip);
    std::memset(zip.get(), 0, sizeof(*zip));

    FILE *vpk_fp;

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

    int num_files = mz_zip_reader_get_num_files(zip.get());

    std::string sfo_path = "sce_sys/param.sfo";

    for (int i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip.get(), i, &file_stat)) {
            continue;
        }
        if (strstr(file_stat.m_filename, "sce_sys/param.sfo")) {
            sfo_path = file_stat.m_filename;
            break;
        }
    }

    Buffer params;
    if (!read_file_from_zip(params, vpk_fp, sfo_path.c_str(), zip)) {
        return false;
    }

    load_sfo(sfo_file, params);

    find_data(game_title, sfo_file, "TITLE");
    find_data(title_id, sfo_file, "TITLE_ID");
    std::string category;
    find_data(category, sfo_file, "CATEGORY");

    LOG_INFO("Path: {}", wide_to_utf(path));
    LOG_INFO("Title: {}", game_title);
    LOG_INFO("Serial: {}", title_id);
    LOG_INFO("Category: {}", category);

    io.app0_prefix = "";

    Buffer eboot;
    if (!read_file_from_zip(eboot, vpk_fp, "eboot.bin", zip)) {
        std::string eboot_path = title_id + "/eboot.bin";
        if (!read_file_from_zip(eboot, vpk_fp, eboot_path.c_str(), zip)) {
            return false;
        } else {
            io.app0_prefix = title_id + "/";
        }
    }

    Buffer libc;
    std::string libc_path = io.app0_prefix + "sce_module/libc.suprx";
    if (read_file_from_zip(libc, vpk_fp, libc_path.c_str(), zip)) {
        if (load_self(entry_point, kernel, mem, libc.data(), "app0:sce_module/libc.suprx") == 0) {
            LOG_INFO("LIBC loaded");
        }
    }

    if (load_self(entry_point, kernel, mem, eboot.data(), "app0:eboot.bin") < 0) {
        return false;
    }

    io.vpk = zip;

    return true;
}
