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

#include <stdint.h>

namespace util {
static inline void cpuid(int output[4], int functionnumber, int ecxleaf = 0);
static inline uint64_t xgetbv(int ctr);
namespace instrset {
// functions in instrset_detect.cpp:
int instrset_detect(void); // tells which instruction sets are supported
bool hasFMA3(void); // true if FMA3 instructions supported
bool hasFMA4(void); // true if FMA4 instructions supported
bool hasXOP(void); // true if XOP  instructions supported
bool hasF16C(void); // true if F16C instructions supported
bool hasAVX512ER(void); // true if AVX512ER instructions supported
bool hasAVX512VBMI(void); // true if AVX512VBMI instructions supported
bool hasAVX512VBMI2(void); // true if AVX512VBMI2 instructions supported

// return values of function instrset_detect.
// usage sample: if (instrset_detect()>=instrset_AVX) {/*AVX supported*/}
constexpr int instrset_AVX512VL = 10;
constexpr int instrset_AVX512 = 9;
constexpr int instrset_AVX2 = 8;
constexpr int instrset_AVX = 7;
constexpr int instrset_SSE4_2 = 6;
constexpr int instrset_SSE4_1 = 5;
constexpr int instrset_SSSE3 = 4;
constexpr int instrset_SSE3 = 3;
constexpr int instrset_SSE2 = 2;
constexpr int instrset_SSE = 1;

} // namespace instrset
} // namespace util
