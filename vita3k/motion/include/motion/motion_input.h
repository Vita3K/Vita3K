// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included

#pragma once

#include "util/quaternion.h"
#include "util/types.h"
#include "util/vector_math.h"
#include <array>

class MotionInput {
public:
    explicit MotionInput();

    MotionInput(const MotionInput &) = default;
    MotionInput &operator=(const MotionInput &) = default;

    MotionInput(MotionInput &&) = default;
    MotionInput &operator=(MotionInput &&) = default;

    void SetPID(SceFloat new_kp, SceFloat new_ki, SceFloat new_kd);
    void SetAcceleration(const Util::Vec3f &acceleration);
    void SetGyroscope(const Util::Vec3f &gyroscope);
    void SetQuaternion(const Util::Quaternion<SceFloat> &quaternion);
    void SetGyroThreshold(SceFloat threshold);

    void EnableGyroBias(bool enable);
    void EnableReset(bool reset);
    void ResetRotations();

    void UpdateRotation(SceULong64 elapsed_time);
    void UpdateOrientation(SceULong64 elapsed_time);

    [[nodiscard]] Util::Quaternion<SceFloat> GetOrientation() const;
    [[nodiscard]] Util::Vec3f GetAcceleration() const;
    [[nodiscard]] Util::Vec3f GetGyroscope() const;
    [[nodiscard]] Util::Vec3f GetRotations() const;

    [[nodiscard]] bool IsMoving(SceFloat sensitivity) const;
    [[nodiscard]] bool IsCalibrated(SceFloat sensitivity) const;
    SceBool IsGyroBiasEnabled() const;

private:
    void ResetOrientation();
    void SetOrientationFromAccelerometer();

    // PID constants
    SceFloat kp;
    SceFloat ki;
    SceFloat kd;

    // PID errors
    Util::Vec3f real_error;
    Util::Vec3f integral_error;
    Util::Vec3f derivative_error;

    // Quaternion containing the device orientation
    Util::Quaternion<SceFloat> quat{ { 0.0f, 0.0f, 1.0f }, 0.0f };

    // Number of full rotations in each axis
    Util::Vec3f rotations;

    // Acceleration vector measurement in G force
    Util::Vec3f accel;

    // Gyroscope vector measurement in radians/s.
    Util::Vec3f gyro;

    // Vector to be substracted from gyro measurements
    Util::Vec3f gyro_bias;

    // Minimum gyro amplitude to detect if the device is moving
    SceFloat gyro_threshold = 0.0f;

    // Number of invalid sequential data
    SceFloat reset_counter = 0;

    // If the provided data is invalid the device will be autocalibrated
    bool reset_enabled = true;

    // If the provided gyro uses bias correction
    bool bias_enabled = true;

    // Use accelerometer values to calculate position
    bool only_accelerometer = true;
};
