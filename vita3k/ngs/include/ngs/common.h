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
#include <vector>

namespace ngs {
enum class BussType {
    BUSS_MASTER = 0,
    BUSS_COMPRESSOR = 1,
    BUSS_SIDE_CHAIN_COMPRESSOR = 2,
    BUSS_DELAY = 3,
    BUSS_DISTORTION = 4,
    BUSS_ENVELOPE = 5,
    BUSS_EQUALIZATION = 6,
    BUSS_MIXER = 7,
    BUSS_PAUSER = 8,
    BUSS_PITCH_SHIFT = 9,
    BUSS_REVERB = 10,
    BUSS_SAS_EMULATION = 11,
    BUSS_SIMPLE = 12,
    BUSS_ATRAC9 = 13,
    BUSS_SIMPLE_ATRAC9 = 14,
    BUSS_SCREAM = 15,
    BUSS_SCREAM_ATRAC9 = 16,
    BUSS_NORMAL_PLAYER = 17
};

static constexpr std::uint32_t MAX_OUTPUT_PORT = 4;
} // namespace ngs