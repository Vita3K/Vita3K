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
#include "patch/instructions.h"
#include "patch/util.h"

#include <util/fs.h>
#include <util/log.h>

std::vector<Patch> get_patches(fs::path &path, const std::string &titleid, const std::string &bin) {
    // Find a file in the path with the titleid
    std::vector<Patch> patches;

    LOG_INFO("Looking for patches for titleid {}", titleid);

    for (auto &entry : fs::directory_iterator(path)) {
        auto filename = fs_utils::path_to_utf8(entry.path().filename());
        // Just in case users decide to use lowercase filenames
        std::transform(filename.begin(), filename.end(), filename.begin(), ::toupper);

        bool isPatchlist = filename.find("PATCHLIST.TXT") != std::string::npos;

        if ((filename.find(titleid) != std::string::npos && filename.ends_with(".TXT")) || isPatchlist) {
            // Read the file
            std::ifstream file(entry.path().c_str());
            PatchHeader patch_header = PatchHeader{
                "",
                "eboot.bin"
            };

            // Parse the file
            while (file.good()) {
                std::string line;
                std::getline(file, line);

                // If this is a header, remember the binary the next patches are for
                if (line[0] == '[') {
                    patch_header = read_header(line, isPatchlist);
                    continue;
                }

                // Ignore comments and patches for other binaries
                if (line[0] == '#' || line[0] == '\n' || bin.find(patch_header.bin) == std::string::npos || (isPatchlist && patch_header.titleid != titleid))
                    continue;

                try {
                    patches.push_back(parse_patch(line));
                } catch (std::exception &e) {
                    LOG_ERROR("Failed to parse patch line: {}", line);
                    LOG_ERROR("Failed with: {}", e.what());
                }
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

    // Clean up potential instructions by removing spaces in between brackets
    // Eg. t1_mov(0, 1) becomes t1_mov(0,1)
    strip_arg_spaces(values);

    // If there is only one value, set pos to the end of the string
    if ((pos = values.find(' ')) == std::string::npos)
        pos = values.length() - 1;

    do {
        pos = values.find(' ');
        std::string val = values.substr(0, pos);

        // Strip 0x from the value if it exists
        if (val.length() > 2 && val[0] == '0' && val[1] == 'x')
            val.erase(0, 2);

        unsigned long long bytes;
        uint8_t byte_count = 0;
        std::string inst = strip_args(val);
        Instruction instruction = to_instruction(inst);

        if (instruction != Instruction::INVALID) {
            auto args = get_args(val);
            std::vector<uint32_t> arg_conv;

            arg_conv.reserve(args.size());
            std::transform(args.begin(), args.end(), std::back_inserter(arg_conv), [](std::string &s) { return std::stoull(s, nullptr, 16); });

            bytes = translate(inst, arg_conv);

            LOG_INFO("Translated {} to 0x{:X}", val, bytes);
        } else {
            bytes = std::stoull(values.substr(0, pos), nullptr, 16);
            // We need to count this, as patches may have bytes of zeros that we don't want to just ignore by passing 0 to toBytes
            byte_count = val.length() % 2 == 0 ? val.length() / 2 : val.length() / 2 + 1;
        }

        auto byte_vec = to_bytes(bytes, byte_count);

        values_vec.reserve(values_vec.size() + byte_vec.size());
        values_vec.insert(values_vec.end(), byte_vec.begin(), byte_vec.end());

        values.erase(0, pos + 1);
    } while (pos != std::string::npos);

    return Patch{ seg, offset, values_vec };
}
