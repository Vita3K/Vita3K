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

#include <rand/prng.h>

void MersenneTwister::twist() {
	for (uint32_t i=0; i<MT_SIZE-MT_M; i++) {
		mt[i]=mt[i+MT_M] ^ (((mt[i] & MT_MASK_UPPER) |
		     (mt[i+1] & MT_MASK_LOWER)) >> 1) ^
		     ((-(mt[i+1] & 1)) & MT_A);
	};

	for (uint32_t i=MT_SIZE-MT_M; i<MT_SIZE-1; i++) {
		mt[i]=mt[i+(MT_M-MT_SIZE)] ^ (((mt[i] & MT_MASK_UPPER) |
		     (mt[i+1] & MT_MASK_LOWER)) >> 1) ^
		     ((-(mt[i+1] & 1)) & MT_A);	
	}; 

	mt[MT_SIZE-1]=mt[MT_M-1] ^ (((mt[MT_SIZE-1] & MT_MASK_UPPER) |
		     (mt[0] & MT_MASK_LOWER)) >> 1) ^
		     ((-(mt[0] & 1)) & MT_A);

	index=0;
}

MersenneTwister::MersenneTwister(const uint32_t seed) {
	mt[0] = seed;
	for (uint32_t i = 1; i < MT_SIZE; i++) {
		mt[i] = MT_F*(mt[i-1] ^ (mt[i-1] >> 30)) + i; 	
	}

	index = MT_SIZE;
}

uint32_t MersenneTwister::gen32() {
	int i = index;

	if (index >= MT_SIZE) {
		twist();
		i=index;	
	}

	uint32_t res {0};

	res = mt[i]; index = i+1;
	res = res ^ (mt[i] >> MT_U);
	res = res ^ ((res << MT_S) & MT_B);
	res = res ^ ((res << MT_T) & MT_C);  
	res = res ^ (res >> MT_L);

	return res;
}
