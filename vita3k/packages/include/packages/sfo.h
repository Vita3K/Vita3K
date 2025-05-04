// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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
 * @file sfo.h
 * @brief PlayStation setting file (`.sfo`) handling
 *
 * PlayStation setting files (`.sfo`) contain metadata information usually describing
 * the content they are accompanying.
 */

#pragma once

#include <io/vfs.h>

#include <cstdint>
#include <string>
#include <utility> // pair
#include <vector>

static const char *EBOOT_PATH = "eboot.bin";

enum SfoDataFormat : uint16_t {
    UTF8 = 0x0004, // UTF-8 Encoding
    UTF8_NULL = 0x0204, // UTF-8 Null Terminated
    ASCII = 0x0402, // ASCII Encoding
    UINT32_T = 0x0404, // 32-bit Unsigned Integer
};

struct SfoHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t key_table_start;
    uint32_t data_table_start;
    uint32_t tables_entries;
};

struct SfoIndexTableEntry {
    uint16_t key_offset;
    uint16_t data_fmt;
    uint32_t data_len;
    uint32_t data_max_len;
    uint32_t data_offset;
};

struct SfoFile {
    SfoHeader header;

    struct SfoEntry {
        SfoIndexTableEntry entry;
        std::pair<std::string, std::string> data;
    };

    std::vector<SfoEntry> entries;
};

namespace sfo {

/**
 * @brief Information about an app as described in its `param.sfo` file
 */
struct SfoAppInfo {
    std::string app_version;
    std::string app_category;
    std::string app_content_id;
    std::string app_addcont;
    std::string app_savedata;
    std::string app_parental_level;
    std::string app_short_title;
    std::string app_title;
    std::string app_title_id;
};

bool get_data_by_id(std::string &out_data, SfoFile &file, int id);
bool get_data_by_key(std::string &out_data, SfoFile &file, const std::string &key);
bool load(SfoFile &sfile, const std::vector<uint8_t> &content);

/**
 * @brief Get the param info object
 *
 * @param app_info App information struct to store the information retrieved from the `.sfo` file
 specified in `param`
 * @param param File buffer pointing to the `param.sfo` file to parse
 * @param sys_lang System language. It is used to get translated strings from `param.sfo`
 */
void get_param_info(sfo::SfoAppInfo &app_info, const vfs::FileBuffer &param, int sys_lang);
} // namespace sfo
