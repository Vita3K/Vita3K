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
#include "patch/patch.h"

#include <map>
#include <util/log.h>

// This function will return the binary name if a header is provided. Otherwise, it will return `eboot.bin`
std::string readBinFromHeader(const std::string &header) {
    std::string bin = "eboot.bin";
    auto open = header.find('[');
    auto close = header.find(']');

    if (open != std::string::npos && close != std::string::npos) {
        bin = header.substr(open + 1, close - open - 1);
    }

    return bin;
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

void removeInstructionSpaces(std::string &patchline) {
    bool inBrackets = false;

    for (size_t i = 0; i < patchline.size(); ++i) {
        if (patchline[i] == '(') {
            inBrackets = true;
        } else if (patchline[i] == ')') {
            inBrackets = false;
        }

        if (inBrackets && patchline[i] == ' ') {
            patchline.erase(i, 1);
            --i;
        }
    }
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

std::vector<uint32_t> getArgs(std::string inst) {
    auto open = inst.find('(');
    auto close = inst.find(')');
    std::vector<uint32_t> args;
    size_t pos = 0;

    if (open == std::string::npos || close == std::string::npos)
        return args;

    inst = inst.substr(open + 1, close - open - 1);

    // If there is only one value, set pos to the end of the string
    if ((pos = inst.find(',')) == std::string::npos)
        pos = inst.length() - 1;

    do {
        pos = inst.find(',');
        std::string val = inst.substr(0, pos);

        args.push_back(std::stoull(val, nullptr, 16));
        inst.erase(0, pos + 1);
    } while (pos != std::string::npos);

    return args;
}

uint32_t translate(std::string &inst, std::vector<uint32_t> &args) {
    auto it = instruction_funcs.find(inst);

    if (it != instruction_funcs.end())
        return it->second.translate(args);

    return 0;
}
