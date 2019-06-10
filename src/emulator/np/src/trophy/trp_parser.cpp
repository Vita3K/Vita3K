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

#include <np/trophy/trp_parser.h>
#include <algorithm>

namespace emu::np::trophy {

TRPFile::TRPFile(std::istream *stream)
    : stream(stream) {
}

static constexpr std::uint32_t NP_TRP_HEADER_MAGIC = 0x004DA2DC;

bool TRPFile::header_parse() {
    std::uint32_t magic = 0;
    if (!stream->read((char*)(&magic), 4) || magic != NP_TRP_HEADER_MAGIC) {
        // Magic not match.... Return
        return false;
    }

    // Seek and read entry count and start of data section
    stream->seekg(0x10, std::ios::beg);
    std::uint32_t entry_count = 0;
    std::uint32_t entry_info_off = 0;
    if (!stream->read((char*)(&entry_count), 4)) {
        return false;
    }

    // Resize to total of entry
    entries.resize(entry_count);

    // Read the data area offset
    if (!stream->read((char*)&entry_info_off, 4)) {
        return false;
    }

    // Start reading all entry infos
    for (std::uint32_t i = 0; i < entry_count; i++) {
        // Read null terminated string. That's the filename
        char temp = 0;

        do {
            if (!stream->read(&temp, 1)) {
                return false;
            }

            entries[i].filename += temp;
        } while (temp != '\0');

        // Read offset from the beginning and size
        if (!stream->read((char*)&(entries[i].offset), 8)) {
            return false;
        }

        if (!stream->read((char*)&entries[i].size, 8)) {
            return false;
        }

        // Other 16 bytes are not known, so ignore
        if (!stream->seekg(0x10, std::ios::cur)) {
            return false;
        }
    }

    // Reading header done. Bye!
    return true;
}

bool TRPFile::get_entry_data(const std::uint32_t idx, std::ostream *out) {
    // Check if the index is not out of range
    if (idx >= entries.size()) {
        return false;
    }

    // Get the target offset
    if (!stream->seekg(entries[idx].offset)) {
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

        if (!stream->read(&copy_buffer[0], copy_size)) {
            return false;
        }

        if (!out->write(&copy_buffer[0], copy_size)) {
            return false;
        }
    }

    return true;
}

}