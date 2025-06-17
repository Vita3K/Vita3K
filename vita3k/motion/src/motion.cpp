// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

#include <motion/event_handler.h>
#include <motion/functions.h>
#include <motion/state.h>

#include <ctrl/state.h>

#include <SDL3/SDL_gamepad.h>
#include <numbers>

SceFVector3 get_acceleration(const MotionState &state) {
    Util::Vec3f accelerometer = state.motion_data.GetAcceleration();
    return {
        accelerometer.x,
        accelerometer.y,
        accelerometer.z,
    };
}

SceFVector3 get_gyroscope(const MotionState &state) {
    Util::Vec3f gyroscope = state.motion_data.GetGyroscope() * 2.f * std::numbers::pi_v<float>;
    return {
        gyroscope.x,
        gyroscope.y,
        gyroscope.z,
    };
}

Util::Quaternion<SceFloat> get_orientation(const MotionState &state) {
    auto quat = state.motion_data.GetOrientation();
    return {
        { -quat.xyz[1], -quat.w, quat.xyz[0] },
        -quat.xyz[2],
    };
}

SceBool get_gyro_bias_correction(const MotionState &state) {
    return state.motion_data.IsGyroBiasEnabled();
}

void set_gyro_bias_correction(MotionState &state, SceBool setValue) {
    state.motion_data.EnableGyroBias(setValue);
}

SceBool get_tilt_correction(MotionState &state) {
    return state.motion_data.IsTiltCorrectionEnabled();
}

void set_tilt_correction(MotionState &state, SceBool setValue) {
    state.motion_data.EnableTiltCorrection(setValue);
}

SceBool get_deadband(MotionState &state) {
    return state.motion_data.IsDeadbandEnabled();
}

void set_deadband(MotionState &state, SceBool setValue) {
    state.motion_data.EnableDeadband(setValue);
}

SceFloat get_angle_threshold(const MotionState &state) {
    return state.motion_data.GetAngleThreshold();
}

void set_angle_threshold(MotionState &state, SceFloat setValue) {
    state.motion_data.SetAngleThreshold(setValue);
}

SceFVector3 get_basic_orientation(const MotionState &state) {
    return state.motion_data.GetBasicOrientation();
}

void handle_motion_event(EmuEnvState &emuenv, const SDL_GamepadSensorEvent &sensor) {
    if (!emuenv.motion.is_sampling)
        return;

    if (!emuenv.ctrl.has_motion_support)
        return;

    if (sensor.sensor == SDL_SENSOR_ACCEL) {
        Util::Vec3f accel{
            sensor.data[0],
            sensor.data[1],
            sensor.data[2],
        };
        accel /= -SDL_STANDARD_GRAVITY;
        std::swap(accel.y, accel.z);
        accel.y *= -1;
        emuenv.motion.motion_data.SetAcceleration(accel);
        emuenv.motion.motion_data.UpdateOrientation(sensor.sensor_timestamp - emuenv.motion.last_accel_timestamp);
        emuenv.motion.motion_data.UpdateBasicOrientation();
        emuenv.motion.last_accel_timestamp = sensor.sensor_timestamp;
        emuenv.motion.last_counter++;
    } else if (sensor.sensor == SDL_SENSOR_GYRO) {
        Util::Vec3f gyro{
            sensor.data[0],
            sensor.data[1],
            sensor.data[2],
        };
        gyro /= 2.f * std::numbers::pi_v<float>;
        std::swap(gyro.y, gyro.z);
        gyro.y *= -1;
        emuenv.motion.motion_data.SetGyroscope(gyro);
        emuenv.motion.motion_data.UpdateRotation(sensor.sensor_timestamp - emuenv.motion.last_gyro_timestamp);
        emuenv.motion.last_gyro_timestamp = sensor.sensor_timestamp;
        emuenv.motion.last_counter++;
    }
}

void refresh_motion(MotionState &state, CtrlState &ctrl_state) {
    if (!state.is_sampling)
        return;

    if (!ctrl_state.has_motion_support)
        return;

    state.last_counter++;
}
