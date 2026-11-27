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

#pragma once

#include <cheat/cheat.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct MemState;

namespace cheat {

enum class SearchType : uint8_t {
    u8,
    s8,
    u16,
    s16,
    u32,
    s32,
    f32,
};

uint32_t search_type_size(SearchType type);
CodeWidth search_type_width(SearchType type);

// Raw little-endian bits of `text` read as `type`, decimal or `0x` hexadecimal for the integers.
std::optional<uint32_t> parse_search_value(const std::string &text, SearchType type);
std::string format_search_value(uint32_t bits, SearchType type);

// Scans guest memory for a value, then narrows the addresses down as the value changes in game.
class MemorySearch {
public:
    // A first scan matching more than this keeps only the lowest addresses.
    static constexpr size_t max_results = 10'000'000;

    void new_search(MemState &mem, SearchType type, uint32_t value);
    void filter(MemState &mem, uint32_t value);
    void clear();

    bool has_searched() const {
        return m_searched;
    }
    bool truncated() const {
        return m_truncated;
    }
    SearchType type() const {
        return m_type;
    }
    const std::vector<uint32_t> &results() const {
        return m_results;
    }

private:
    SearchType m_type = SearchType::s32;
    std::vector<uint32_t> m_results;
    bool m_searched = false;
    bool m_truncated = false;
};

std::optional<uint32_t> read_search_value(MemState &mem, uint32_t address, SearchType type);
bool write_search_value(MemState &mem, uint32_t address, SearchType type, uint32_t bits);

// A `$0X00` write code that keeps `address` at `bits`.
CodeLine make_write_code(uint32_t address, SearchType type, uint32_t bits);

} // namespace cheat
