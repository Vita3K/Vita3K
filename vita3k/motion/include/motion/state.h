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

#include <motion/motion_input.h>

#include <array>
#include <mutex>

struct MotionSample {
    uint32_t counter = 0;
    uint64_t timestamp = 0;
    uint64_t hostTimestamp = 0;
    Util::Vec3f gyro{};
    Util::Vec3f accel{};
};

struct MotionState {
    static constexpr std::size_t MAX_SAMPLES = 0x40;

    std::mutex mutex;
    MotionInput motion_data;
    uint64_t last_gyro_timestamp = 0;
    uint64_t last_accel_timestamp = 0;

    uint32_t ring_buffer_size = 0;
    uint32_t current_buffer_index = 0;
    std::array<MotionSample, MAX_SAMPLES> ring_buffer_samples;

    bool is_sampling = false;
};
