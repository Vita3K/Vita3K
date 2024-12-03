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

#include <emuenv/state.h>
#include <motion/state.h>

SceFVector3 get_acceleration(const MotionState &state);
SceFVector3 get_gyroscope(const MotionState &state);
Util::Quaternion<SceFloat> get_orientation(const MotionState &state);
SceBool get_gyro_bias_correction(const MotionState &state);
void set_gyro_bias_correction(MotionState &state, SceBool setValue);
SceBool get_tilt_correction(MotionState &state);
void set_tilt_correction(MotionState &state, SceBool setValue);
SceBool get_deadband(MotionState &state);
void set_deadband(MotionState &state, SceBool setValue);
SceFloat get_angle_threshold(const MotionState &state);
void set_angle_threshold(MotionState &state, SceFloat setValue);
SceFVector3 get_basic_orientation(const MotionState &state);

void refresh_motion(MotionState &state, CtrlState &ctrl_state);
