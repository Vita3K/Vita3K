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

#include <util/types.h>

enum SceMotionErrorCode : uint32_t {
    SCE_MOTION_ERROR_DATA_INVALID = 0x80360200,
    SCE_MOTION_ERROR_READING = 0x80360201,
    SCE_MOTION_ERROR_NON_INIT_ERR = 0x80360202,
    SCE_MOTION_ERROR_STATE_INVALID = 0x80360203,
    SCE_MOTION_ERROR_CALIB_READ_FAIL = 0x80360204,
    SCE_MOTION_ERROR_OUT_OF_BOUNDS = 0x80360205,
    SCE_MOTION_ERROR_NOT_SAMPLING = 0x80360206,
    SCE_MOTION_ERROR_ALREADY_SAMPLING = 0x80360207,
    SCE_MOTION_ERROR_MEM_IN_USE = 0x80360208,
    SCE_MOTION_ERROR_NULL_PARAMETER = 0x80360209,
    SCE_MOTION_ERROR_ANGLE_OUT_OF_RANGE = 0x8036020A,
    SCE_MOTION_ERROR_INVALID_ARG = 0x8036020B
};

#define SCE_MOTION_MAX_NUM_STATES 64 //!<  Maximum number of states in the state buffer

#define SCE_MOTION_MAGNETIC_FIELD_UNSTABLE 0 //!<  Unstable magnetic field, reliability of NED matrix is very low
#define SCE_MOTION_MAGNETIC_FIELD_STABLE 1 //!<  Currently Not In Use
#define SCE_MOTION_MAGNETIC_FIELD_VERYSTABLE 2 //!<  Magnetic field is very stable and NED matrix can be used

#define SCE_MOTION_FORCED_STOP_POWER_CONFIG_CHANGE (1 << 0) //!<  Forced stop due to power configuration change

struct SceMotionState {
    SceUInt32 timestamp; //!<  Device Local Timestamp
    SceFVector3 acceleration; //!<  Stores the accelerometer readings
    SceFVector3 angularVelocity; //!<  Stores the angular velocity readings
    SceUInt8 reserve1[12]; //!<  Reserved data
    SceFQuaternion deviceQuat; //!<  Device orientation as a Quaternion <x,y,z,w>
    SceUMatrix4 rotationMatrix; //!<  Device orientation as a rotation matrix
    SceUMatrix4 nedMatrix; //!<  Magnetic North rotation matrix Col0: North Col1: East Col2: Down
    SceUInt8 reserve2[4]; //!<  Reserved data
    SceFVector3 basicOrientation; //!< Basic orientation of the device
    SceUInt64 hostTimestamp; //!< Time at which the sampled data packet is received from the device
    SceUInt8 reserve3[36]; //!<  Reserved data
    SceUInt8 magnFieldStability; //!<  Stability of the Magnetic North output
    SceUInt8 dataInfo; //!<  Information on the current data, Bit:0 - Determine if the motion data has been forcibly stopped
    SceUInt8 reserve4[2]; //!<  Reserved data
};

struct SceMotionSensorState {
    SceFVector3 accelerometer; //!<  Stores the accelerometer readings
    SceFVector3 gyro; //!<  Stores the angular velocity readings
    SceUInt8 reserve1[12]; //!<  Reserved data
    SceUInt32 timestamp; //!<  Sensor Packet Timestamp
    SceUInt32 counter; //!<  A counter to identify state
    SceUInt8 reserve2[4]; //!<  Reserved data
    SceUInt64 hostTimestamp; //!< Time at which the sampled data packet is received from the device
    SceUInt8 dataInfo; //!<  Information on the current data, bit:0 - Determine if the motion data has been forcibly stopped*/
    SceUInt8 reserve3[7]; //!<  Reserved data
};

struct SceMotionDeviceLocation {
    SceFVector3 accelerometer;
    SceFVector3 gyro;
    SceUInt8 reserve[24];
};
