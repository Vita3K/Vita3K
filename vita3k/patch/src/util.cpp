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

#include "patch/util.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <map>
#include <optional>
#include <string_view>
#include <util/log.h>

std::optional<uint64_t> parse_hex(std::string_view text) {
    if (text.starts_with("0x") || text.starts_with("0X"))
        text.remove_prefix(2);
    uint64_t value = 0;
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    if (text.empty() || ec != std::errc{} || end != text.data() + text.size())
        return std::nullopt;
    return value;
}

namespace {

std::optional<std::vector<uint8_t>> parse_hex_bytes(const std::string &text) {
    std::string digits;
    for (size_t pos = 0; pos < text.size();) {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;
        const size_t start = pos;
        while (pos < text.size() && !std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;
        std::string token = text.substr(start, pos - start);
        if (token.starts_with("0x") || token.starts_with("0X"))
            token.erase(0, 2);
        if (token.size() % 2)
            token.insert(token.begin(), '0');
        digits += token;
    }

    if (digits.empty())
        return std::nullopt;

    std::vector<uint8_t> bytes;
    bytes.reserve(digits.size() / 2);
    for (size_t i = 0; i < digits.size(); i += 2) {
        const auto byte = parse_hex(std::string_view(digits).substr(i, 2));
        if (!byte)
            return std::nullopt;
        bytes.push_back(static_cast<uint8_t>(*byte));
    }
    return bytes;
}

std::optional<int64_t> parse_number(std::string_view text) {
    if (text.starts_with("0x") || text.starts_with("0X")) {
        const auto value = parse_hex(text);
        return value ? std::optional<int64_t>(static_cast<int64_t>(*value)) : std::nullopt;
    }
    int64_t value = 0;
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || ec != std::errc{} || end != text.data() + text.size())
        return std::nullopt;
    return value;
}

// Evaluates "a + b - c" over decimal or 0x numbers; vblank is fixed to 1 (60Hz)
std::optional<uint32_t> evaluate_expression(const std::string &expression) {
    int64_t total = 0;
    int sign = 1;
    size_t pos = 0;
    bool expect_number = true;
    while (pos < expression.size()) {
        const char c = expression[pos];
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++pos;
        } else if (!expect_number && (c == '+' || c == '-')) {
            sign = c == '+' ? 1 : -1;
            expect_number = true;
            ++pos;
        } else if (expect_number) {
            size_t end = pos;
            while (end < expression.size() && !std::isspace(static_cast<unsigned char>(expression[end])) && expression[end] != '+' && expression[end] != '-')
                ++end;
            const std::string_view token = std::string_view(expression).substr(pos, end - pos);
            const auto value = token == "vblank" ? std::optional<int64_t>(1) : parse_number(token);
            if (!value)
                return std::nullopt;
            total += sign * *value;
            expect_number = false;
            pos = end;
        } else {
            return std::nullopt;
        }
    }
    if (expect_number)
        return std::nullopt;
    return static_cast<uint32_t>(total);
}

std::optional<std::vector<uint8_t>> encode_instruction(std::string name, const std::string &args_text) {
    if (to_instruction(name) == Instruction::INVALID)
        return std::nullopt;

    std::vector<uint32_t> args;
    size_t pos = 0;
    while (!args_text.empty() && pos <= args_text.size()) {
        const auto comma = args_text.find(',', pos);
        const auto arg = evaluate_expression(args_text.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos));
        if (!arg)
            return std::nullopt;
        args.push_back(*arg);
        if (comma == std::string::npos)
            break;
        pos = comma + 1;
    }

    return to_bytes(translate(name, args), 0);
}

} // namespace

// This function will return a PatchHeader struct, which contains the titleid and the binary name (if provided)
PatchHeader read_header(std::string &header, bool is_patchlist) {
    PatchHeader patch_header;

    strip_arg_spaces(header, '[', ']');

    auto args = get_args(header, '[', ']');

    // When this is in a patchlist file, the possible values are [titleid, bin] and [titleid]
    // When this is in a title-specific patch file, the possible values are just [bin] (because the titleid is already known)
    if (is_patchlist) {
        patch_header.titleid = args[0];

        if (args.size() > 1)
            patch_header.bin = args[1];
    } else {
        patch_header.titleid = "";

        // If we have more than one argument, user probably made a mistake, so we can correct it
        if (args.size() > 1) {
            LOG_WARN("Found more than one argument in a title-specific patch file. You only need to specify the binary name because the TitleID is already known.");
            patch_header.bin = args[1];

            return patch_header;
        }

        patch_header.bin = args[0];
    }

    return patch_header;
}

std::vector<uint8_t> to_bytes(unsigned long long value, uint8_t count) {
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

void strip_arg_spaces(std::string &line, char open, char close) {
    bool in_brackets = false;

    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == open) {
            in_brackets = true;
        } else if (line[i] == close) {
            in_brackets = false;
        }

        if (in_brackets && line[i] == ' ') {
            line.erase(i, 1);
            --i;
        }
    }
}

void strip_arg_spaces(std::string &line) {
    return strip_arg_spaces(line, '(', ')');
}

Instruction to_instruction(const std::string &inst) {
    auto it = instruction_funcs.find(inst);

    if (it != instruction_funcs.end())
        return it->second.instruction;

    return Instruction::INVALID;
}

bool is_valid_instruction(std::string &inst) {
    return to_instruction(strip_args(inst)) != Instruction::INVALID;
}

std::string strip_args(std::string inst) {
    auto open = inst.find('(');
    auto close = inst.find(')');

    if (open == std::string::npos || close == std::string::npos)
        return inst;

    inst.erase(open, close - open + 1);

    return inst;
}

std::vector<std::string> get_args(std::string inst, char open, char close) {
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

std::vector<std::string> get_args(std::string inst) {
    return get_args(inst, '(', ')');
}

uint32_t translate(std::string &inst, std::vector<uint32_t> &args) {
    auto it = instruction_funcs.find(inst);

    if (it != instruction_funcs.end())
        return it->second.translate(args);

    LOG_WARN("Instruction {} could not be translated! It will be replaced with NOP", inst);

    return 0;
}

// Value is a list of hex byte strings and instructions such as "t1_mov(1, 4 - 1)"
std::optional<std::vector<uint8_t>> parse_value(const std::string &text) {
    std::vector<uint8_t> bytes;
    size_t pos = 0;
    while (pos < text.size()) {
        if (std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
            continue;
        }

        size_t end = pos;
        while (end < text.size() && !std::isspace(static_cast<unsigned char>(text[end])) && text[end] != '(')
            ++end;

        std::optional<std::vector<uint8_t>> chunk;
        if (end < text.size() && text[end] == '(') {
            const auto close = text.find(')', end);
            if (close == std::string::npos)
                return std::nullopt;
            chunk = encode_instruction(text.substr(pos, end - pos), text.substr(end + 1, close - end - 1));
            end = close + 1;
        } else {
            chunk = parse_hex_bytes(text.substr(pos, end - pos));
        }

        if (!chunk)
            return std::nullopt;
        bytes.insert(bytes.end(), chunk->begin(), chunk->end());
        pos = end;
    }

    if (bytes.empty())
        return std::nullopt;
    return bytes;
}

// Address is "<offset>" in segment 0 or "<segment>:<offset>"
bool parse_address(const std::string &address, uint8_t &seg, uint32_t &offset) {
    std::string_view offset_text = address;
    seg = 0;
    if (const auto colon = address.find(':'); colon != std::string::npos) {
        const auto seg_value = parse_hex(std::string_view(address).substr(0, colon));
        if (!seg_value || *seg_value > 0xFF)
            return false;
        seg = static_cast<uint8_t>(*seg_value);
        offset_text = std::string_view(address).substr(colon + 1);
    }

    const auto offset_value = parse_hex(offset_text);
    if (!offset_value || *offset_value > UINT32_MAX)
        return false;
    offset = static_cast<uint32_t>(*offset_value);
    return true;
}

// Versions are compared per component so that "1.02" equals "01.02"
bool patch_versions_equal(const std::string &a, const std::string &b) {
    const auto split = [](const std::string &version) -> std::optional<std::vector<int>> {
        std::vector<int> parts;
        size_t pos = 0;
        while (pos <= version.size()) {
            const auto dot = version.find('.', pos);
            const std::string part = version.substr(pos, dot == std::string::npos ? std::string::npos : dot - pos);
            int value = 0;
            const auto [end, ec] = std::from_chars(part.data(), part.data() + part.size(), value);
            if (part.empty() || ec != std::errc{} || end != part.data() + part.size())
                return std::nullopt;
            parts.push_back(value);
            if (dot == std::string::npos)
                break;
            pos = dot + 1;
        }
        while (parts.size() > 1 && parts.back() == 0)
            parts.pop_back();
        return parts;
    };
    const auto parts_a = split(a);
    const auto parts_b = split(b);
    if (!parts_a || !parts_b)
        return a == b;
    return *parts_a == *parts_b;
}
