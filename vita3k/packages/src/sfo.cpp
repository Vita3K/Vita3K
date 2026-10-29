// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <packages/sfo.h>

#include <boost/algorithm/string/trim.hpp>

#include <algorithm>
#include <cstring>
#include <fmt/format.h>
#include <limits>

namespace sfo {

bool get_data_by_id(std::string &out_data, SfoFile &file, int id) {
    std::string key;
    switch (id) {
    case 6:
        key = "CONTENT_ID";
        break;
    case 7:
        key = "NP_COMMUNICATION_ID";
        break;
    case 8:
        key = "CATEGORY";
        break;
    case 9:
        key = "TITLE";
        break;
    case 10:
        key = "STITLE";
        break;
    case 0xc:
        key = "TITLE_ID";
        break;
    case 0xe: // Todo
    default:
        return false;
    }

    return get_data_by_key(out_data, file, key);
}

bool get_data_by_key(std::string &out_data, SfoFile &file, const std::string &key) {
    auto res = std::ranges::find_if(file.entries,
        [key](const auto &et) { return et.data.first == key; });

    if (res == file.entries.end()) {
        return false;
    }
    out_data = res->data.second;

    return true;
}

bool get_param_info(sfo::SfoAppInfo &app_info, const vfs::FileBuffer &param, int sys_lang) {
    SfoFile sfo_handle;
    if (!sfo::load(sfo_handle, param)) {
        app_info = {};
        return false;
    }

    SfoAppInfo parsed_info;
    sfo::get_data_by_key(parsed_info.app_version, sfo_handle, "APP_VER");
    if (!parsed_info.app_version.empty() && parsed_info.app_version[0] == '0')
        parsed_info.app_version.erase(parsed_info.app_version.begin());
    sfo::get_data_by_key(parsed_info.app_category, sfo_handle, "CATEGORY");
    sfo::get_data_by_key(parsed_info.app_content_id, sfo_handle, "CONTENT_ID");
    if (!sfo::get_data_by_key(parsed_info.app_addcont, sfo_handle, "INSTALL_DIR_ADDCONT"))
        sfo::get_data_by_key(parsed_info.app_addcont, sfo_handle, "TITLE_ID");
    if (!sfo::get_data_by_key(parsed_info.app_savedata, sfo_handle, "INSTALL_DIR_SAVEDATA"))
        sfo::get_data_by_key(parsed_info.app_savedata, sfo_handle, "TITLE_ID");
    sfo::get_data_by_key(parsed_info.app_parental_level, sfo_handle, "PARENTAL_LEVEL");
    if (!sfo::get_data_by_key(parsed_info.app_short_title, sfo_handle, fmt::format("STITLE_{:0>2d}", sys_lang)))
        sfo::get_data_by_key(parsed_info.app_short_title, sfo_handle, "STITLE");
    if (!sfo::get_data_by_key(parsed_info.app_title, sfo_handle, fmt::format("TITLE_{:0>2d}", sys_lang)))
        sfo::get_data_by_key(parsed_info.app_title, sfo_handle, "TITLE");
    std::ranges::replace(parsed_info.app_title, '\n', ' ');
    boost::trim(parsed_info.app_title);
    sfo::get_data_by_key(parsed_info.app_title_id, sfo_handle, "TITLE_ID");
    app_info = std::move(parsed_info);
    return true;
}

bool load(SfoFile &sfile, const std::vector<uint8_t> &content) {
    constexpr uint32_t SFO_MAGIC = 0x46535000;
    sfile = {};
    if (content.size() < sizeof(SfoHeader))
        return false;

    SfoFile parsed{};
    std::memcpy(&parsed.header, content.data(), sizeof(SfoHeader));
    if (parsed.header.magic != SFO_MAGIC
        || parsed.header.tables_entries > (content.size() - sizeof(SfoHeader)) / sizeof(SfoIndexTableEntry))
        return false;

    const size_t index_table_end = sizeof(SfoHeader)
        + static_cast<size_t>(parsed.header.tables_entries) * sizeof(SfoIndexTableEntry);
    if (parsed.header.key_table_start < index_table_end || parsed.header.key_table_start > content.size()
        || parsed.header.data_table_start < parsed.header.key_table_start
        || parsed.header.data_table_start > content.size())
        return false;

    const size_t key_table_size = parsed.header.data_table_start - parsed.header.key_table_start;
    if (key_table_size > std::numeric_limits<uint16_t>::max())
        return false;

    parsed.entries.resize(parsed.header.tables_entries + 1);

    for (uint32_t i = 0; i < parsed.header.tables_entries; i++) {
        std::memcpy(&parsed.entries[i].entry,
            content.data() + sizeof(SfoHeader) + static_cast<size_t>(i) * sizeof(SfoIndexTableEntry),
            sizeof(SfoIndexTableEntry));
    }

    parsed.entries[parsed.header.tables_entries].entry.key_offset = parsed.header.data_table_start - parsed.header.key_table_start;

    const size_t data_table_size = content.size() - parsed.header.data_table_start;
    for (uint32_t i = 0; i < parsed.header.tables_entries; i++) {
        const auto &entry = parsed.entries[i].entry;
        const auto next_key_offset = parsed.entries[i + 1].entry.key_offset;
        if (next_key_offset <= entry.key_offset || next_key_offset > key_table_size)
            return false;
        const size_t key_size = next_key_offset - entry.key_offset;
        const size_t data_offset = entry.data_offset;
        const size_t data_size = entry.data_len;
        if (key_size == 0 || entry.key_offset >= key_table_size
            || data_offset > data_table_size || data_size > data_table_size - data_offset)
            return false;

        const auto key_end = parsed.header.key_table_start + entry.key_offset + key_size;
        if (content[key_end - 1] != 0)
            return false;

        const auto data_begin = content.data() + parsed.header.data_table_start + data_offset;
        switch (entry.data_fmt) {
        case SfoDataFormat::UINT32_T:
            if (data_size < sizeof(uint32_t))
                return false;
            break;
        case SfoDataFormat::ASCII:
        case SfoDataFormat::UTF8:
            break;
        case SfoDataFormat::UTF8_NULL:
            if (!data_size || data_begin[data_size - 1] != 0)
                return false;
            break;
        default:
            return false;
        }
    }

    // Parse each SFO entry and extract its associated key
    for (uint32_t i = 0; i < parsed.header.tables_entries; i++) {
        // Calculate the size of the key for the current entry by subtracting the offsets
        const uint32_t keySize = parsed.entries[i + 1].entry.key_offset - parsed.entries[i].entry.key_offset;

        // Resize the 'key' data to hold the correct amount of characters for the key
        parsed.entries[i].data.first.resize(keySize);

        // Calculate the starting address of the key data in the content buffer
        const auto key_begin = content.begin() + parsed.header.key_table_start + parsed.entries[i].entry.key_offset;

        // Extract the key data from the content buffer and assign it to 'key' as a string
        // Subtract 1 from keySize to avoid including the null terminator
        parsed.entries[i].data.first = std::string(key_begin, key_begin + keySize - 1);
    }

    // Parse each SFO entry and extract its associated data
    for (uint32_t i = 0; i < parsed.header.tables_entries; i++) {
        const uint32_t dataSize = parsed.entries[i].entry.data_len;

        // Resize the destination string to match the data size
        parsed.entries[i].data.second.resize(dataSize);

        // Compute the data's starting position in the content buffer
        const auto data_begin = content.begin() + parsed.header.data_table_start + parsed.entries[i].entry.data_offset;

        // Copy the raw data into a temporary buffer
        std::vector<char> data(data_begin, data_begin + dataSize);

        // Interpret and convert the raw data based on its format
        switch (parsed.entries[i].entry.data_fmt) {
        case SfoDataFormat::UINT32_T:
            // Convert the first 4 bytes to a uint32_t and store as string
            {
                uint32_t value{};
                std::memcpy(&value, data.data(), sizeof(value));
                parsed.entries[i].data.second = std::to_string(value);
            }
            break;
        case SfoDataFormat::ASCII:
        case SfoDataFormat::UTF8:
            // Interpret the data as a raw string (may not be null-terminated)
            parsed.entries[i].data.second = std::string(data.begin(), data.end());
            break;
        case SfoDataFormat::UTF8_NULL:
            // Interpret the data as a null-terminated UTF-8 string (exclude the null byte)
            parsed.entries[i].data.second = std::string(data.begin(), data.end() - 1);
            break;
        default:
            // Unknown or unsupported data format
            return false;
        }
    }

    sfile = std::move(parsed);
    return true;
}

} // namespace sfo
