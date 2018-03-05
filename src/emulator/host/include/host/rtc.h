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

#include <host/state.h>

#include <chrono>
#include <cstdint>

#define VITA_CLOCKS_PER_SEC 1000000
using VitaClocks = std::chrono::duration<std::uint64_t, std::ratio<1, VITA_CLOCKS_PER_SEC>>;

// Grabbed from JPSCP
// This is the # of microseconds between January 1, 0001 and January 1, 1970.
const auto rtcMagicOffset = 62135596800000000ULL;

inline std::uint64_t rtc_base_ticks()
{
    const auto now = std::chrono::system_clock::now();
    const auto now_timepoint = std::chrono::time_point_cast<VitaClocks>(now);
    const auto clocks_since_unix_time = now_timepoint.time_since_epoch().count();

    // host high_resolution_clock offset (implementations dependant)
    const auto host_clock_offset = std::chrono::high_resolution_clock::now().time_since_epoch().count();

    return rtcMagicOffset + clocks_since_unix_time - host_clock_offset;
}

inline std::uint64_t rtc_get_ticks(const HostState& host)
{
    const uint64_t base_ticks = host.kernel.base_tick.tick;

    const auto now = std::chrono::high_resolution_clock::now();
    const auto now_timepoint = std::chrono::time_point_cast<VitaClocks>(now);
    const uint64_t now_ticks = now_timepoint.time_since_epoch().count();

    return base_ticks + now_ticks;
}