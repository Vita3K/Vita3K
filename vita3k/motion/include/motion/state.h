<<<<<<< Updated upstream
// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
=======
// RPCSV emulator project
// Copyright (C) 2025 RPCSV team
>>>>>>> Stashed changes
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

#include <SDL3/SDL_sensor.h>

#include <memory>
#include <mutex>

enum DeviceRotation : int32_t {
    ROTATION_UNKNOWN = -1,
    ROTATION_0,
    ROTATION_90,
    ROTATION_180,
    ROTATION_270
};

struct SDL_SensorDeleter {
    void operator()(SDL_Sensor *s) const { SDL_CloseSensor(s); }
};

using SDL_SensorPtr = std::unique_ptr<SDL_Sensor, SDL_SensorDeleter>;

struct MotionState {
    std::mutex mutex;
    MotionInput motion_data;
    uint32_t last_counter = 0;
    uint64_t last_gyro_timestamp = 0;
    uint64_t last_accel_timestamp = 0;
    DeviceRotation device_native_rotation = ROTATION_0;
    uint32_t device_accel_id = 0;
    uint32_t device_gyro_id = 0;
    SDL_SensorPtr device_accel;
    SDL_SensorPtr device_gyro;
    uint64_t last_updated_gyro_timestamp = 0;
    uint64_t last_updated_accel_timestamp = 0;

    bool has_device_motion_support = false;
    bool is_sampling = false;

    void init();
    void clear_device_motion_support();
    void refresh_device_motion_support();
    void stop_sensor_sampling();
    void start_sensor_sampling();
    void reset_runtime();
};
