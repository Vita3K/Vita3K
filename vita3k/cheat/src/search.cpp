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

#include <cheat/search.h>

#include <mem/functions.h>
#include <mem/ptr.h>
#include <mem/state.h>
#include <mem/util.h>
#include <util/string_utils.h>

#include <fmt/format.h>

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>

namespace cheat {

namespace {

constexpr uint32_t page_size = KiB(4);
// Pages checked per allocator query and per lock, most of the address space is never allocated.
constexpr uint32_t pages_per_block = 32;

bool is_signed(SearchType type) {
    return (type == SearchType::s8) || (type == SearchType::s16) || (type == SearchType::s32);
}

uint32_t type_mask(SearchType type) {
    const uint32_t size = search_type_size(type);
    return (size == 4) ? 0xFFFFFFFFu : ((1u << (size * 8)) - 1);
}

uint32_t load(const uint8_t *data, uint32_t size) {
    uint32_t value = 0;
    std::memcpy(&value, data, size);
    return value;
}

bool matches(uint32_t bits, uint32_t wanted, SearchType type) {
    if (type != SearchType::f32)
        return bits == wanted;

    // Games rarely hold exactly the decimal that was typed, compare with a small tolerance.
    const float actual = std::bit_cast<float>(bits);
    const float expected = std::bit_cast<float>(wanted);
    if (!std::isfinite(actual) || !std::isfinite(expected))
        return bits == wanted;
    return std::fabs(actual - expected) <= std::max(1e-5f, std::fabs(expected) * 1e-5f);
}

} // namespace

uint32_t search_type_size(SearchType type) {
    switch (type) {
    case SearchType::u8:
    case SearchType::s8: return 1;
    case SearchType::u16:
    case SearchType::s16: return 2;
    default: return 4;
    }
}

CodeWidth search_type_width(SearchType type) {
    switch (search_type_size(type)) {
    case 1: return CodeWidth::bits8;
    case 2: return CodeWidth::bits16;
    default: return CodeWidth::bits32;
    }
}

std::optional<uint32_t> parse_search_value(const std::string &text, SearchType type) {
    const std::string trimmed = string_utils::trim_copy(text);
    if (trimmed.empty())
        return std::nullopt;

    if (type == SearchType::f32) {
        // libc++ only gained floating point from_chars recently.
        char *end = nullptr;
        const float value = std::strtof(trimmed.c_str(), &end);
        if (end != trimmed.c_str() + trimmed.size())
            return std::nullopt;
        return std::bit_cast<uint32_t>(value);
    }

    std::string_view digits = trimmed;
    const bool negative = digits.starts_with('-');
    if (negative)
        digits.remove_prefix(1);
    int base = 10;
    if (digits.starts_with("0x") || digits.starts_with("0X")) {
        digits.remove_prefix(2);
        base = 16;
    }

    uint64_t magnitude = 0;
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), magnitude, base);
    if (digits.empty() || (error != std::errc()) || (end != digits.data() + digits.size()))
        return std::nullopt;

    // Either the signed or the unsigned reading of the bits is accepted, whichever type is picked.
    const uint64_t bits = uint64_t(search_type_size(type)) * 8;
    if (negative ? (magnitude > (uint64_t(1) << (bits - 1))) : (magnitude > (uint64_t(1) << bits) - 1))
        return std::nullopt;

    const uint64_t value = negative ? (0 - magnitude) : magnitude;
    return static_cast<uint32_t>(value) & type_mask(type);
}

std::string format_search_value(uint32_t bits, SearchType type) {
    switch (type) {
    case SearchType::s8: return std::to_string(static_cast<int8_t>(bits));
    case SearchType::s16: return std::to_string(static_cast<int16_t>(bits));
    case SearchType::s32: return std::to_string(static_cast<int32_t>(bits));
    case SearchType::f32: return fmt::format("{}", std::bit_cast<float>(bits));
    default: return std::to_string(bits & type_mask(type));
    }
}

void MemorySearch::new_search(MemState &mem, SearchType type, uint32_t value) {
    m_type = type;
    m_results.clear();
    m_searched = true;
    m_truncated = false;

    const uint32_t size = search_type_size(type);
    const auto page_count = static_cast<uint32_t>(std::min<size_t>(mem.allocator.max_offset, 0x100000000ull / page_size));

    // Page 0 is never handed out, an address of 0 would read as "no result" anyway.
    for (uint32_t block = 0; block < page_count; block += pages_per_block) {
        const uint32_t block_end = std::min(block + pages_per_block, page_count);

        const std::lock_guard<std::mutex> lock(mem.generation_mutex);
        if (mem.allocator.free_slot_count(block, block_end) == static_cast<int>(block_end - block))
            continue;

        for (uint32_t page = std::max(block, 1u); page < block_end; ++page) {
            if (mem.allocator.free_slot_count(page, page + 1) != 0)
                continue;

            const uint32_t base = page * page_size;
            const uint8_t *data = Ptr<uint8_t>(base).get(mem);
            for (uint32_t offset = 0; offset < page_size; offset += size) {
                if (!matches(load(data + offset, size), value, type))
                    continue;

                if (m_results.size() == max_results) {
                    m_truncated = true;
                    return;
                }
                m_results.push_back(base + offset);
            }
        }
    }
}

void MemorySearch::filter(MemState &mem, uint32_t value) {
    if (!m_searched)
        return;

    std::erase_if(m_results, [&](uint32_t address) {
        const auto bits = read_search_value(mem, address, m_type);
        return !bits || !matches(*bits, value, m_type);
    });
}

void MemorySearch::clear() {
    m_results.clear();
    m_searched = false;
    m_truncated = false;
}

std::optional<uint32_t> read_search_value(MemState &mem, uint32_t address, SearchType type) {
    const uint32_t size = search_type_size(type);
    const std::lock_guard<std::mutex> lock(mem.generation_mutex);
    if ((address == 0) || !is_valid_addr_range(mem, address, address + size))
        return std::nullopt;

    uint8_t bytes[4] = {};
    for (uint32_t i = 0; i < size; ++i)
        bytes[i] = *Ptr<uint8_t>(address + i).get(mem);
    return load(bytes, size);
}

bool write_search_value(MemState &mem, uint32_t address, SearchType type, uint32_t bits) {
    const uint32_t size = search_type_size(type);
    const std::lock_guard<std::mutex> lock(mem.generation_mutex);
    if ((address == 0) || !is_valid_addr_range(mem, address, address + size))
        return false;

    for (uint32_t i = 0; i < size; ++i)
        *Ptr<uint8_t>(address + i).get(mem) = static_cast<uint8_t>(bits >> (i * 8));
    return true;
}

CodeLine make_write_code(uint32_t address, SearchType type, uint32_t bits) {
    CodeLine code;
    code.control = static_cast<uint16_t>((uint32_t(CodeType::write) << 12) | (uint32_t(search_type_width(type)) << 8));
    code.first = address;
    code.second = bits & type_mask(type);
    return code;
}

} // namespace cheat
