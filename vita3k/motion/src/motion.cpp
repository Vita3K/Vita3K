// Vita3K emulator project
// Copyright (C) 2023 Vita3K team
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

#include <motion/functions.h>
#include <motion/motion.h>
#include <motion/state.h>

SceFVector3 get_acceleration(const MotionState &state) {
    Util::Vec3f accelerometer =  state.motion_data.GetAcceleration();
    return {
        accelerometer.x,
        accelerometer.y,
        accelerometer.z,
    };
}

SceFVector3 get_gyroscope(const MotionState &state) {
    Util::Vec3f gyroscope = state.motion_data.GetGyroscope();
    return {
        gyroscope.x,
        gyroscope.y,
        gyroscope.z,
    };
}

SceFVector3 get_gyro_bias(const MotionState &state) {
    Util::Vec3f bias = state.motion_data.GetGyroscopeBias();
    return {
        bias.x,
        bias.y,
        bias.z,
    };
}

Util::Quaternion<SceFloat> get_quaternion(const MotionState &state) {
    return state.motion_data.GetQuaternion();
}

SceBool get_gyro_bias_correction(const MotionState &state) {
    return state.motion_data.IsGyroBiasEnabled();
}

SceFVector3 set_gyro_bias_correction(MotionState& state, SceBool setValue) {
    state.motion_data.EnableGyroBias(setValue);
}

void refresh_motion(MotionState &state) {
    SceULong64 elapsed_time = 5000;
    state.motion_data.UpdateRotation(elapsed_time);
    state.motion_data.UpdateOrientation(elapsed_time);
}