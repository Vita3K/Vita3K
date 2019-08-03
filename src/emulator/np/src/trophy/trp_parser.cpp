// Vita3K emulator project
// Copyright (C) 2019 Vita3K team
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

#include <algorithm>
#include <cstring>
#include <np/trophy/trp_parser.h>
#include <util/bytes.h>

namespace emu::np::trophy {

static constexpr std::uint32_t NP_TRP_HEADER_MAGIC = 0x004DA2DC;

bool TRPFile::header_parse() {
    std::uint32_t magic = 0;
    if (!read_func(&magic, 4) || magic != NP_TRP_HEADER_MAGIC) {
        // Magic not match.... Return
        return false;
    }

    // Seek and read entry count and start of data section
    seek_func(0x10);

    std::int32_t entry_count = 0;
    std::int32_t entry_info_off = 0;
    if (!read_func(&entry_count, 4)) {
        return false;
    }

    entry_count = network_to_host_order(entry_count);

    // Resize to total of entry
    entries.resize(entry_count);

    // Read the data area offset
    if (!read_func(&entry_info_off, 4)) {
        return false;
    }

    entry_info_off = network_to_host_order(entry_info_off);

    seek_func(entry_info_off);

    // Start reading all entry infos
    for (std::int32_t i = 0; i < entry_count; i++) {
        // Read null terminated string. That's the filename
        char temp = 0;

        do {
            if (!read_func(&temp, 1)) {
                return false;
            }

            entries[i].filename += temp;
        } while (temp != '\0');

        seek_func(entry_info_off + (i * 0x40) + 0x20);

        // Read offset from the beginning and size
        if (!read_func((char *)&(entries[i].offset), 8)) {
            return false;
        }

        if (!read_func((char *)&entries[i].size, 8)) {
            return false;
        }

        entries[i].offset = network_to_host_order(entries[i].offset);
        entries[i].size = network_to_host_order(entries[i].size);

        // Other 16 bytes are not known, so ignore
        if (!seek_func(entry_info_off + (i * 0x40) + 0x20 + 16 + 0x10)) {
            return false;
        }
    }

    // Reading header done. Bye!
    return true;
}

bool TRPFile::get_entry_data(const std::uint32_t idx, TRPWriteFunc write_func) {
    // Check if the index is not out of range
    if (idx >= entries.size()) {
        return false;
    }

    // Get the target offset
    if (!seek_func(static_cast<int>(entries[idx].offset))) {
        return false;
    }

    // Read chunk by chunk and write to out
    static constexpr std::uint32_t COPY_CHUNK_SIZE = 0x10000;
    std::uint64_t bytes_left = entries[idx].size;

    std::vector<char> copy_buffer;
    copy_buffer.resize(COPY_CHUNK_SIZE);

    while (bytes_left != 0) {
        const std::uint32_t copy_size = std::min<std::uint32_t>(static_cast<std::uint32_t>(bytes_left),
            COPY_CHUNK_SIZE);

        if (!read_func(&copy_buffer[0], copy_size)) {
            return false;
        }

        if (!write_func(&copy_buffer[0], copy_size)) {
            return false;
        }

        bytes_left -= copy_size;
    }

    return true;
}

const std::int32_t TRPFile::search_file(const char *name) {
    for (std::size_t i = 0; i < entries.size(); i++) {
        if (strncmp(entries[i].filename.c_str(), name, strlen(name)) == 0) {
            return static_cast<std::int32_t>(i);
        }
    }

    return -1;
}

} // namespace emu::np::trophy
