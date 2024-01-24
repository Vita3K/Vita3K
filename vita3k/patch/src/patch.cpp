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

#include "patch/patch.h"

#include <util/fs.h>
#include <util/log.h>

std::vector<Patch> get_patches(fs::path &path, const std::string &titleid) {
    // Find a file in the path with the titleid
    std::vector<Patch> patches;

    LOG_INFO("Looking for patches for titleid {}", titleid);

    for (auto &entry : fs::directory_iterator(path)) {
        auto filename = fs_utils::path_to_utf8(entry.path().filename());
        // Just in case users decide to use lowercase filenames
        std::transform(filename.begin(), filename.end(), filename.begin(), ::toupper);

        if (filename.find(titleid) != std::string::npos && filename.ends_with(".TXT")) {
            // Read the file
            std::ifstream file(entry.path().c_str());

            // Parse the file
            while (file.good()) {
                std::string line;
                std::getline(file, line);

                // If line is a comment, skip it
                if (line[0] == '#')
                    continue;

                patches.push_back(parse_patch(line));
            }
        }
    }

    LOG_INFO("Found {} patches for titleid {}", patches.size(), titleid);

    return patches;
}

Patch parse_patch(const std::string &patch) {
    // FORMAT: <seg>:<offset> <values>
    // Example, equivalent to `t1_mov(0, 1)`:
    // 0:0xA994 0x01 0x20
    // Keep in mind that we are in little endian
    uint8_t seg = std::stoi(patch.substr(0, patch.find(':')));

    // Everything after the first colon, and before the first space, is the offset
    uint32_t offset = std::stoull(patch.substr(patch.find(':') + 1, patch.find(' ') - patch.find(':') - 1), nullptr, 16);

    // All following values (separated by spaces) are the values to be written
    std::string values = patch.substr(patch.find(' ') + 1);
    std::vector<uint8_t> values_vec;

    // Get all additional values separated by spaces
    size_t pos = 0;

    while ((pos = values.find(' ')) != std::string::npos) {
        values_vec.push_back(std::stoull(values.substr(0, pos), nullptr, 16));
        values.erase(0, pos + 1);
    }

    values_vec.push_back(std::stoull(values, nullptr, 16));

    return Patch{ seg, offset, values_vec };
}
