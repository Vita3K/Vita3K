// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

/**
 * @file sfo.cpp
 * @brief PlayStation setting file (`.sfo`) handling
 *
 * PlayStation setting files (`.sfo`) contain metadata information usually describing
 * the content they are accompanying.
 */

#include <packages/functions.h>
#include <packages/sfo.h>

#include <boost/algorithm/string/trim.hpp>

#include <algorithm>
#include <cstring>
#include <fmt/format.h>

namespace sfo {

bool get_data_by_id(std::string &out_data, SfoFile &file, int id) {
    if (id < 0 || file.entries.size() <= id) {
        return false;
    }

    out_data = file.entries[id].data.second;
    return true;
}

bool get_data_by_key(std::string &out_data, SfoFile &file, const std::string &key) {
    auto res = std::find_if(file.entries.begin(), file.entries.end(),
        [key](auto et) { return et.data.first == key; });

    if (res == file.entries.end()) {
        return false;
    }
    out_data = res->data.second;

    return true;
}

void get_param_info(sfo::SfoAppInfo &app_info, const vfs::FileBuffer &param, int sys_lang) {
    SfoFile sfo_handle;
    sfo::load(sfo_handle, param);
    sfo::get_data_by_key(app_info.app_version, sfo_handle, "APP_VER");
    if (app_info.app_version[0] == '0')
        app_info.app_version.erase(app_info.app_version.begin());
    sfo::get_data_by_key(app_info.app_category, sfo_handle, "CATEGORY");
    sfo::get_data_by_key(app_info.app_content_id, sfo_handle, "CONTENT_ID");
    if (!sfo::get_data_by_key(app_info.app_addcont, sfo_handle, "INSTALL_DIR_ADDCONT"))
        sfo::get_data_by_key(app_info.app_addcont, sfo_handle, "TITLE_ID");
    if (!sfo::get_data_by_key(app_info.app_savedata, sfo_handle, "INSTALL_DIR_SAVEDATA"))
        sfo::get_data_by_key(app_info.app_savedata, sfo_handle, "TITLE_ID");
    sfo::get_data_by_key(app_info.app_parental_level, sfo_handle, "PARENTAL_LEVEL");
    if (!sfo::get_data_by_key(app_info.app_short_title, sfo_handle, fmt::format("STITLE_{:0>2d}", sys_lang)))
        sfo::get_data_by_key(app_info.app_short_title, sfo_handle, "STITLE");
    if (!sfo::get_data_by_key(app_info.app_title, sfo_handle, fmt::format("TITLE_{:0>2d}", sys_lang)))
        sfo::get_data_by_key(app_info.app_title, sfo_handle, "TITLE");
    std::replace(app_info.app_title.begin(), app_info.app_title.end(), '\n', ' ');
    boost::trim(app_info.app_title);
    sfo::get_data_by_key(app_info.app_title_id, sfo_handle, "TITLE_ID");
}

bool load(SfoFile &sfile, const std::vector<uint8_t> &content) {
    if (content.empty()) {
        return false;
    }

    memcpy(&sfile.header, content.data(), sizeof(SfoHeader));

    sfile.entries.resize(sfile.header.tables_entries + 1);

    for (uint32_t i = 0; i < sfile.header.tables_entries; i++) {
        memcpy(&sfile.entries[i].entry, content.data() + sizeof(SfoHeader) + i * sizeof(SfoIndexTableEntry), sizeof(SfoIndexTableEntry));
    }

    sfile.entries[sfile.header.tables_entries].entry.key_offset = sfile.header.data_table_start - sfile.header.key_table_start;

    for (uint32_t i = 0; i < sfile.header.tables_entries; i++) {
        uint32_t keySize = sfile.entries[i + 1].entry.key_offset - sfile.entries[i].entry.key_offset;

        sfile.entries[i].data.first.resize(keySize);

        memcpy(&sfile.entries[i].data.first[0], &content[sfile.header.key_table_start + sfile.entries[i].entry.key_offset], keySize);

        // Quick hack to remove garbage null terminator caused by reading directly
        // to buffer
        sfile.entries[i].data.first = sfile.entries[i].data.first.c_str();
    }

    for (uint32_t i = 0; i < sfile.header.tables_entries; i++) {
        uint32_t dataSize = sfile.entries[i].entry.data_len;

        sfile.entries[i].data.second.resize(dataSize);

        // The last of data is a terminator
        memcpy(&sfile.entries[i].data.second[0], &content[sfile.header.data_table_start + sfile.entries[i].entry.data_offset], dataSize - 1);

        // Quick hack to remove garbage null terminator caused by reading directly
        // to buffer
        sfile.entries[i].data.second = sfile.entries[i].data.second.c_str();
    }

    return true;
}

} // namespace sfo
