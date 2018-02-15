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

#include <SceMt19937/exports.h>
#include <rand/prng.h>

EXPORT(int, sceMt19937Init, Ptr<MersenneTwister> mt, unsigned int seed) {
    if (!mt) {
	return -1;    
    }

    MersenneTwister *const mtc =  mt.get(host.mem);

    new (mtc) MersenneTwister(seed);
    return 0;
}

EXPORT(unsigned int, sceMt19937UInt, Ptr<MersenneTwister> mt) {
    if (!mt) {
    	return -1;
    }

    MersenneTwister *const mtc = mt.get(host.mem);
    return mtc->gen32();
}

BRIDGE_IMPL(sceMt19937Init)
BRIDGE_IMPL(sceMt19937UInt)
