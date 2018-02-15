// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
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

class MersenneTwister {
    enum {
    	MT_SIZE = 624,
	MT_M = 397,
	MT_A = 0x9908B0DF,
	MT_B = 0x9D2C5680,
	MT_C = 0xEFC60000,
	MT_F = 1812433253,
	MT_R = 31,
	MT_U = 11,
	MT_S = 7,
	MT_T = 15,
	MT_L = 18,
	MT_MASK_LOWER = (1u << 31) - 1,
	MT_MASK_UPPER = 1u << 31
    };
  
    uint32_t mt[MT_SIZE];
    uint32_t index;

    void twist();
public:
    MersenneTwister(const uint32_t seed);
    
    uint32_t gen32();
};
