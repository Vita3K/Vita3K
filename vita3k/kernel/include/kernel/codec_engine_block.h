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

#include <algorithm>
#include <cstdint>
#include <map>

struct CodecEngineBlock {
    uint32_t base = 0;
    uint32_t size = 0;
    std::map<uint32_t, uint32_t> allocations;

    uint32_t allocate(uint32_t requested_size, uint32_t alignment) {
        if (requested_size == 0)
            return 0;
        alignment = std::max(alignment, 256u);
        if (alignment > 0x100000 || (alignment & (alignment - 1)) != 0)
            return 0;

        const uint64_t allocation_size = (static_cast<uint64_t>(requested_size) + 255) & ~uint64_t{ 255 };
        const uint64_t end = static_cast<uint64_t>(base) + size;
        uint64_t candidate = base;
        for (const auto &[address, length] : allocations) {
            candidate = (candidate + alignment - 1) & ~static_cast<uint64_t>(alignment - 1);
            if (candidate + allocation_size <= address)
                break;
            candidate = static_cast<uint64_t>(address) + length;
        }
        candidate = (candidate + alignment - 1) & ~static_cast<uint64_t>(alignment - 1);
        if (candidate == 0 || candidate + allocation_size > end || candidate + allocation_size > (uint64_t{ 1 } << 32))
            return 0;

        const auto address = static_cast<uint32_t>(candidate);
        allocations.emplace(address, static_cast<uint32_t>(allocation_size));
        return address;
    }

    bool release(uint32_t address) {
        return allocations.erase(address) != 0;
    }
};