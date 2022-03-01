// Vita3K emulator project
// Copyright (C) 2021 Vita3K team
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

#include <cstdint>
#include <mutex>
#include <vector>

struct BitmapAllocator {
    std::vector<std::uint32_t> words;
    std::size_t max_offset;

protected:
    int force_fill(const std::uint32_t offset, const int size, const bool or_mode = false);

public:
    BitmapAllocator() = default;
    explicit BitmapAllocator(const std::size_t total_bits);

    void set_maximum(const std::size_t total_bits);

    int allocate_from(const std::uint32_t start_offset, int &size, const bool best_fit = false);
    int allocate_at(const std::uint32_t start_offset, int size);
    void free(const std::uint32_t offset, const int size);
    void reset();

    // Count free bits in [offset, offset_end) (exclusive)
    int free_slot_count(const std::uint32_t offset, const std::uint32_t offset_end) const;
};
