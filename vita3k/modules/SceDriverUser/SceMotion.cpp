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

#include <module/module.h>

#include <ctrl/state.h>
#include <motion/functions.h>
#include <motion/motion.h>

#include <util/tracy.h>
TRACY_MODULE_NAME(SceMotion);

EXPORT(SceFloat, sceMotionGetAngleThreshold) {
    TRACY_FUNC(sceMotionGetAngleThreshold);
    return get_angle_threshold(emuenv.motion);
}

EXPORT(int, sceMotionGetBasicOrientation, SceFVector3 *basicOrientation) {
    TRACY_FUNC(sceMotionGetBasicOrientation, basicOrientation);
    if (!emuenv.motion.is_sampling) {
        return SCE_MOTION_ERROR_NOT_SAMPLING;
    }
    if (basicOrientation == nullptr) {
        return RET_ERROR(SCE_MOTION_ERROR_NULL_PARAMETER);
    }

    std::lock_guard<std::mutex> guard(emuenv.motion.mutex);

    *basicOrientation = get_basic_orientation(emuenv.motion);

    return SCE_MOTION_OK;
}

EXPORT(SceBool, sceMotionGetDeadband) {
    TRACY_FUNC(sceMotionGetDeadband);
    return get_deadband(emuenv.motion);
}

EXPORT(int, sceMotionGetDeadbandExt) {
    TRACY_FUNC(sceMotionGetDeadbandExt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionGetDeviceLocation, SceMotionDeviceLocation *devLocation) {
    TRACY_FUNC(sceMotionGetDeviceLocation, devLocation);
    return UNIMPLEMENTED();
}

EXPORT(SceBool, sceMotionGetGyroBiasCorrection) {
    TRACY_FUNC(sceMotionGetGyroBiasCorrection);
    return get_gyro_bias_correction(emuenv.motion);
}

EXPORT(SceBool, sceMotionGetMagnetometerState) {
    TRACY_FUNC(sceMotionGetMagnetometerState);
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionGetSensorState, SceMotionSensorState *sensorState, int numRecords) {
    TRACY_FUNC(sceMotionGetSensorState, sensorState, numRecords);
    if (!emuenv.motion.is_sampling) {
        return SCE_MOTION_ERROR_NOT_SAMPLING;
    }
    if (numRecords >= SCE_MOTION_MAX_NUM_STATES) {
        return SCE_MOTION_ERROR_OUT_OF_BOUNDS;
    }
    if (sensorState == nullptr) {
        return RET_ERROR(SCE_MOTION_ERROR_NULL_PARAMETER);
    }

    if ((emuenv.ctrl.has_motion_support || emuenv.motion.has_device_motion_support) && !emuenv.cfg.disable_motion) {
        std::lock_guard<std::mutex> guard(emuenv.motion.mutex);
        sensorState->accelerometer = get_acceleration(emuenv.motion);
        sensorState->gyro = get_gyroscope(emuenv.motion);

        sensorState->timestamp = emuenv.motion.last_accel_timestamp;
        sensorState->counter = emuenv.motion.last_counter;
        sensorState->hostTimestamp = sensorState->timestamp;
        sensorState->dataInfo = 0;
    } else {
        // some default values
        memset(sensorState, 0, sizeof(*sensorState));
        sensorState->accelerometer.z = -1.0;

        std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
        uint64_t timestamp = std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();
        sensorState->timestamp = timestamp;
        sensorState->hostTimestamp = timestamp;

        sensorState->counter = emuenv.motion.last_counter++;
    }

    for (int i = 1; i < numRecords; i++)
        sensorState[i] = sensorState[0];

    return SCE_MOTION_OK;
}

EXPORT(int, sceMotionGetState, SceMotionState *motionState) {
    TRACY_FUNC(sceMotionGetState, motionState);
    if (!emuenv.motion.is_sampling) {
        return SCE_MOTION_ERROR_NOT_SAMPLING;
    }
    if (motionState == nullptr) {
        return RET_ERROR(SCE_MOTION_ERROR_NULL_PARAMETER);
    }

    if ((emuenv.ctrl.has_motion_support || emuenv.motion.has_device_motion_support) && !emuenv.cfg.disable_motion) {
        std::lock_guard<std::mutex> guard(emuenv.motion.mutex);
        motionState->timestamp = emuenv.motion.last_accel_timestamp;

        motionState->acceleration = get_acceleration(emuenv.motion);
        motionState->angularVelocity = get_gyroscope(emuenv.motion);
        LOG_DEBUG("Accel: x={} y={} z={}", motionState->acceleration.x, motionState->acceleration.y,
            motionState->acceleration.z);
        LOG_DEBUG("Gyro: x={} y={} z={}", motionState->angularVelocity.x, motionState->angularVelocity.y, 
            motionState->angularVelocity.z);
        Util::Quaternion dev_quat = get_orientation(emuenv.motion);
        motionState->basicOrientation = get_basic_orientation(emuenv.motion);

        static_assert(sizeof(motionState->deviceQuat) == sizeof(dev_quat));
        memcpy(&motionState->deviceQuat, &dev_quat, sizeof(motionState->deviceQuat));

        *reinterpret_cast<decltype(dev_quat.ToMatrix()) *>(&motionState->rotationMatrix) = dev_quat.ToMatrix();
        // not right, but we can't do better without a magnetometer
        memcpy(&motionState->nedMatrix, &motionState->rotationMatrix, sizeof(motionState->nedMatrix));

        motionState->hostTimestamp = motionState->timestamp;
        // set it as unstable because we don't have one
        motionState->magnFieldStability = SCE_MOTION_MAGNETIC_FIELD_UNSTABLE;
        motionState->dataInfo = 0;
    } else {
        // put some default values
        memset(motionState, 0, sizeof(SceMotionState));

        std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
        uint64_t timestamp = std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();
        motionState->timestamp = timestamp;
        motionState->hostTimestamp = timestamp;

        motionState->acceleration.z = -1.0;
        motionState->deviceQuat.z = 1;
        for (int i = 0; i < 4; i++) {
            // identity matrices
            reinterpret_cast<float *>(&motionState->rotationMatrix.x.x)[i * 4 + i] = 1;
            reinterpret_cast<float *>(&motionState->nedMatrix.x.x)[i * 4 + i] = 1;
        }
    }

    CALL_EXPORT(sceMotionGetBasicOrientation, &motionState->basicOrientation);
    return SCE_MOTION_OK;
}

EXPORT(int, sceMotionGetStateExt) {
    TRACY_FUNC(sceMotionGetStateExt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionGetStateInternal) {
    TRACY_FUNC(sceMotionGetStateInternal);
    return UNIMPLEMENTED();
}

EXPORT(SceBool, sceMotionGetTiltCorrection) {
    TRACY_FUNC(sceMotionGetTiltCorrection);
    return get_tilt_correction(emuenv.motion);
}

EXPORT(int, sceMotionGetTiltCorrectionExt) {
    TRACY_FUNC(sceMotionGetTiltCorrectionExt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionInitLibraryExt) {
    TRACY_FUNC(sceMotionInitLibraryExt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionMagnetometerOff) {
    TRACY_FUNC(sceMotionMagnetometerOff);
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionMagnetometerOn) {
    TRACY_FUNC(sceMotionMagnetometerOn);
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionReset) {
    TRACY_FUNC(sceMotionReset);
    std::lock_guard<std::mutex> guard(emuenv.motion.mutex);
    emuenv.motion.motion_data.ResetQuaternion();
    emuenv.motion.motion_data.ResetRotations();
    return SCE_MOTION_OK;
}

EXPORT(int, sceMotionResetExt) {
    TRACY_FUNC(sceMotionResetExt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionRotateYaw, const float radians) {
    TRACY_FUNC(sceMotionRotateYaw, radians);
    emuenv.motion.motion_data.RotateYaw(radians);
    return SCE_MOTION_OK;
}

EXPORT(int, sceMotionSetAngleThreshold, SceFloat angle) {
    TRACY_FUNC(sceMotionSetAngleThreshold, angle);
    if (std::isnan(angle) || angle > 45.0f) {
        return SCE_MOTION_ERROR_ANGLE_OUT_OF_RANGE;
    }

    set_angle_threshold(emuenv.motion, angle);
    return SCE_MOTION_OK;
}

EXPORT(int, sceMotionSetDeadband, SceBool setValue) {
    TRACY_FUNC(sceMotionSetDeadband, setValue);
    set_deadband(emuenv.motion, setValue);

    return SCE_MOTION_OK;
}

EXPORT(int, sceMotionSetDeadbandExt) {
    TRACY_FUNC(sceMotionSetDeadbandExt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionSetGyroBiasCorrection, SceBool setValue) {
    TRACY_FUNC(sceMotionSetGyroBiasCorrection, setValue);
    set_gyro_bias_correction(emuenv.motion, setValue);

    return SCE_MOTION_OK;
}

EXPORT(int, sceMotionSetTiltCorrection, SceBool setValue) {
    TRACY_FUNC(sceMotionSetTiltCorrection, setValue);
    set_tilt_correction(emuenv.motion, setValue);

    return SCE_MOTION_OK;
}

EXPORT(int, sceMotionSetTiltCorrectionExt) {
    TRACY_FUNC(sceMotionSetTiltCorrectionExt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionStartSampling) {
    TRACY_FUNC(sceMotionStartSampling);
    if (emuenv.motion.is_sampling) {
        return SCE_MOTION_ERROR_ALREADY_SAMPLING;
    }

    emuenv.motion.start_sensor_sampling();
    return SCE_MOTION_OK;
}

EXPORT(int, sceMotionStartSamplingExt) {
    TRACY_FUNC(sceMotionStartSamplingExt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionStopSampling) {
    TRACY_FUNC(sceMotionStopSampling);
    if (!emuenv.motion.is_sampling) {
        return SCE_MOTION_ERROR_NOT_SAMPLING;
    }

    emuenv.motion.stop_sensor_sampling();
    return SCE_MOTION_OK;
}

EXPORT(int, sceMotionStopSamplingExt) {
    TRACY_FUNC(sceMotionStopSamplingExt);
    return UNIMPLEMENTED();
}

EXPORT(int, sceMotionTermLibraryExt) {
    TRACY_FUNC(sceMotionTermLibraryExt);
    return UNIMPLEMENTED();
}
