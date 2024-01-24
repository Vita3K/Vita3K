// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
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

#include <util/log.h>

#include <chrono>

namespace TimeLogger {
struct TimeLogger {
private:
    std::string _name;
    std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<long, std::ratio<1, 1000000000>>>::duration
        init_time
        = std::chrono::high_resolution_clock::now().time_since_epoch();

public:
    ~TimeLogger() {
        auto destroy_time = std::chrono::high_resolution_clock::now().time_since_epoch();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(destroy_time - init_time).count();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(destroy_time - init_time).count();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(destroy_time - init_time).count();

        LOG_TRACE("{} took {} ns | {} μs | {} ms", _name, nanos, micros, millis);
    }

    inline TimeLogger(std::string name) {
        _name = name;
    }
};
} // namespace TimeLogger
