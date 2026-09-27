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
//
// CPU/system identification helpers. Structure adapted from RPCS3's
// util/sysinfo.cpp (GPLv2), trimmed to Vita3K's needs and extended with
// Android support.

#pragma once

#include <cstdint>
#include <string>

namespace util {

std::string get_cpu_brand();

int64_t get_cpu_clock_mhz();

std::string get_system_info();

uint64_t get_total_memory();

uint64_t get_process_memory_usage();

} // namespace util
