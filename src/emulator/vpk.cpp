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

#include <host/state.h>
#include <io/state.h>
#include <io/vfs.h>
#include <util/string_convert.h>

#include <util/log.h>

#include <cassert>
#include <cstring>
#include <vector>

#include <experimental/filesystem>

#include <miniz.h>

namespace fs = std::experimental::filesystem;

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

// Extract to real file
bool extract_vpk_to_path(const ZipPtr zip, const std::string& pref_path) {
    int fileCount = (int)mz_zip_reader_get_num_files(zip.get());

    if (fileCount == 0)
    {
        mz_zip_reader_end(zip.get());
        return true;
    }

    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(zip.get(), 0, &file_stat))
    {
        mz_zip_reader_end(zip.get());
        return true;
    }

    fs::path lastDir;

    for (int i = 0; i < fileCount; i++)
    {
        if (!mz_zip_reader_file_stat(zip.get(), i, &file_stat)) continue;
        if (mz_zip_reader_is_file_a_directory(zip.get(), i)) continue; // skip directories

        fs::path pref_std_path(pref_path);
        fs::path std_filename(file_stat.m_filename);

        fs::path destFile = pref_std_path;
        if (destFile != "") destFile += fs::path("/");
        destFile += std_filename;

        if (fs::exists(destFile)) {
            continue;
        }

        fs::path dir = pref_std_path;
        if (dir != "") dir += fs::path("/");
        dir += std_filename.parent_path();

        fs::absolute(dir);

        if (lastDir != dir) {
            if (dir != "" && !fs::exists(dir)) fs::create_directories(dir);
            lastDir = dir;
        }

        mz_zip_reader_extract_to_file(zip.get(), i, destFile.string().c_str(), 0);
    }

    return true;
}

bool load_vpk(Ptr<const void> &entry_point, IOState &io, MemState &mem, std::string &game_title, std::string& title_id, const std::wstring& path) { 
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

    const int param_index = mz_zip_reader_locate_file(zip.get(), "sce_sys/param.sfo", nullptr, 0);

    if (param_index < 0) {
        LOG_INFO("Invalid package");
        return false;
    }

    Buffer param_sfo;

    if (!mz_zip_reader_extract_file_to_callback(zip.get(), "sce_sys/param.sfo", &write_to_buffer, &param_sfo, 0)) {
        return false;
    }

    SfoFile file;
    bool res = load_sfo(file, param_sfo);

    if (res == true) {
         title_id = find_data(file, "TITLE_ID");
         game_title = find_data(file, "TITLE");
         vfs::mount("app0", fs::absolute(vfs::physical_mount_path("ux0") + "/app/" + title_id + "/").string());

         // If the folder exists, dont reinstall it
         if (!fs::exists(vfs::physical_mount_path("ux0") + "/app/" + title_id)) {
             // If there is missing files, its user fault
             LOG_INFO("Try to install {} ({}) if you are done with the app/game and you want to remove, please remove folder: {}/app/{}", game_title, title_id,
                       vfs::physical_mount_path("vs0"), title_id);

             extract_vpk_to_path(zip, vfs::physical_mount_path("app0"));
         }
    } else {
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
