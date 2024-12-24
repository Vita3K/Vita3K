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

#include "patch/util.h"

#include <map>
#include <util/log.h>

// This function will return a PatchHeader struct, which contains the titleid and the binary name (if provided)
PatchHeader readHeader(std::string &header) {
    PatchHeader patch_header;

    stripArgSpaces(header, '[', ']');

    auto args = getArgs(header, '[', ']');

    // Header should either be [bin] or [titleid, bin]
    // (this is because some patches are seperated by titleid in filename, and don't need it provided in the header)
    if (args.size() == 2) {
        patch_header.titleid = args[0];
        patch_header.bin = args[1];
    } else {
        patch_header.titleid = "";
        patch_header.bin = args[0];
    }

    return patch_header;
}

std::vector<uint8_t> toBytes(unsigned long long value, uint8_t count) {
    std::vector<uint8_t> bytes;

    // If count is 0, go until we see a byte of all 0s
    if (count == 0) {
        while (value != 0) {
            bytes.push_back(value & 0xFF);
            value >>= 8;
        }

        return bytes;
    }

    // Otherwise, just go as much as count tells us
    for (uint8_t i = 0; i < count; i++) {
        bytes.push_back((value >> ((count - 1 - i) * 8)) & 0xFF);
    }

    return bytes;
}

void stripArgSpaces(std::string &line, char open, char close) {
    bool inBrackets = false;

    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == open) {
            inBrackets = true;
        } else if (line[i] == close) {
            inBrackets = false;
        }

        if (inBrackets && line[i] == ' ') {
            line.erase(i, 1);
            --i;
        }
    }
}

void stripArgSpaces(std::string &line) {
  return stripArgSpaces(line, '(', ')');
}

Instruction toInstruction(const std::string &inst) {
    auto it = instruction_funcs.find(inst);

    if (it != instruction_funcs.end())
        return it->second.instruction;

    return Instruction::INVALID;
}

bool isValidInstruction(std::string &inst) {
    return toInstruction(stripArgs(inst)) != Instruction::INVALID;
}

std::string stripArgs(std::string inst) {
    auto open = inst.find('(');
    auto close = inst.find(')');

    if (open == std::string::npos || close == std::string::npos)
        return inst;

    inst.erase(open, close - open + 1);

    return inst;
}

std::vector<std::string> getArgs(std::string inst, char open, char close) {
    auto open_pos = inst.find(open);
    auto close_pos = inst.find(close);
    std::vector<std::string> args;

    if (open_pos == std::string::npos || close_pos == std::string::npos)
        return args;

    inst = inst.substr(open_pos + 1, close_pos - open_pos - 1);

    // If there is only one value, set pos to the end of the string
    if ((open_pos = inst.find(',')) == std::string::npos)
        open_pos = inst.length() - 1;

    do {
        open_pos = inst.find(',');
        std::string val = inst.substr(0, open_pos);

        args.push_back(val);
        inst.erase(0, open_pos + 1);
    } while (open_pos != std::string::npos);

    return args;
}

std::vector<std::string> getArgs(std::string inst) {
    return getArgs(inst, '(', ')');
}

uint32_t translate(std::string &inst, std::vector<uint32_t> &args) {
    auto it = instruction_funcs.find(inst);

    if (it != instruction_funcs.end())
        return it->second.translate(args);

    LOG_WARN("Instruction {} could not be translated! It will be replaced with NOP", inst);

    return 0;
}
