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

#include <kernel/load_self.h>
#include <kernel/state.h>

#include <host/app.h>
#include <host/sfo.h>
#include <host/state.h>
#include <io/state.h>
#include <util/log.h>
#include <util/string_convert.h>

#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <istream>
#include <iterator>

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

bool read_file_from_disk(Buffer &buf, const char *file, HostState &host) {
    std::string file_path = host.pref_path + "ux0/app/" + host.io.title_id + "/" + file;
    std::ifstream f(file_path, std::ifstream::binary);
    if (f.fail()) {
        return false;
    }
    f.unsetf(std::ios::skipws);
    std::streampos size;
    f.seekg(0, std::ios::end);
    size = f.tellg();
    f.seekg(0, std::ios::beg);
    buf.reserve(size);
    buf.insert(buf.begin(), std::istream_iterator<uint8_t>(f), std::istream_iterator<uint8_t>());
    return true;
}

static bool read_file_from_zip(Buffer &buf, FILE *&vpk_fp, const char *file, const ZipPtr zip) {
    const int file_index = mz_zip_reader_locate_file(zip.get(), file, nullptr, 0);
    if (file_index < 0) {
        LOG_CRITICAL("Failed to locate {}.", file);
        return false;
    }

    if (!mz_zip_reader_extract_file_to_callback(zip.get(), file, &write_to_buffer, &buf, 0)) {
        LOG_CRITICAL("Failed to extract {}.", file);
        return false;
    }

    return true;
}

bool install_vpk(Ptr<const void> &entry_point, HostState &host, const std::wstring &path) {
    const ZipPtr zip(new mz_zip_archive, delete_zip);
    std::memset(zip.get(), 0, sizeof(*zip));
    
    FILE *vpk_fp;

#ifdef WIN32
    if (_wfopen_s(&vpk_fp, path.c_str(), L"rb")) {
#else
    if (!(vpk_fp = fopen(wide_to_utf(path).c_str(), "rb"))) {
#endif
        LOG_CRITICAL("Failed to open the vpk.");
        return false;
    }

    if (!mz_zip_reader_init_cfile(zip.get(), vpk_fp, 0, 0)) {
        LOG_CRITICAL("Cannot init miniz reader");
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

    load_sfo(host.sfo_handle, params);
    find_data(host.io.title_id, host.sfo_handle, "TITLE_ID");
    
    std::string output_base_path;
    output_base_path = host.pref_path + "ux0/app/";
    std::string title_base_path = output_base_path;
    if (sfo_path.length() < 20) {
        output_base_path += host.io.title_id;
    }
    title_base_path += host.io.title_id;
#ifdef WIN32
    bool exists = (!CreateDirectoryA(title_base_path.c_str(), nullptr));
#else
    const int mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    bool exists = mkdir(title_base_path.c_str(), mode);
#endif
    if (exists) {
        LOG_INFO("{} already installed, launching application...", host.io.title_id);
        return true;
    }

    for (int i = 0; i < num_files; i++) {
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(zip.get(), i, &file_stat)) {
            continue;
        }
        std::string output_path = output_base_path;
        output_path += "/";
        output_path += file_stat.m_filename;
        if (mz_zip_reader_is_file_a_directory(zip.get(), i)) {
#ifdef WIN32
            CreateDirectoryA(output_path.c_str(), nullptr);
#else
            const int mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
            mkdir(output_path.c_str(), mode);
#endif
        } else {
            const size_t slash = output_path.rfind('/');
            if (std::string::npos != slash){
                std::string directory = output_path.substr(0, slash);
#ifdef WIN32
                CreateDirectoryA(directory.c_str(), nullptr);
#else
                const int mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
                mkdir(directory.c_str(), mode);
#endif
            }
            
            mz_zip_reader_extract_to_file(zip.get(), i, output_path.c_str(), 0);
        }
    }
    
    std::string savedata_path = host.pref_path + "ux0/user/00/savedata/" + host.io.title_id;

#ifdef WIN32
    CreateDirectoryA(savedata_path.c_str(), nullptr);
#else
    mkdir(savedata_path.c_str(), mode);
#endif

    host.io.savedata0_path = "ux0:/user/00/savedata/" + host.io.title_id + "/";
    
    LOG_INFO("{} installed succesfully!", host.io.title_id);
    return true;
}

bool load_app(Ptr<const void> &entry_point, HostState &host, const std::wstring &path, bool is_vpk) {
    if (is_vpk) {
        if (!install_vpk(entry_point, host, path)){
            return false;
        }
    }

    if (!is_vpk) {
        host.io.title_id = wide_to_utf(path);
    }
    
    Buffer params;
    if (!read_file_from_disk(params, "sce_sys/param.sfo", host)) {
        return false;
    }

    load_sfo(host.sfo_handle, params);

    find_data(host.game_title, host.sfo_handle, "TITLE");
    find_data(host.io.title_id, host.sfo_handle, "TITLE_ID");
    std::string category;
    find_data(category, host.sfo_handle, "CATEGORY");

    LOG_INFO("Path: {}", wide_to_utf(path));
    LOG_INFO("Title: {}", host.game_title);
    LOG_INFO("Serial: {}", host.io.title_id);
    LOG_INFO("Category: {}", category);

    Buffer eboot;
    if (!read_file_from_disk(eboot, "eboot.bin", host)) {
        return false;
    }

    Buffer libc;
    if (read_file_from_disk(libc, "sce_module/libc.suprx", host)) {
        if (load_self(entry_point, host.kernel, host.mem, libc.data(), "app0:sce_module/libc.suprx") == 0) {
            LOG_INFO("LIBC loaded");
        }
    }

    if (load_self(entry_point, host.kernel, host.mem, eboot.data(), "app0:eboot.bin") < 0) {
        return false;
    }

    return true;
}
