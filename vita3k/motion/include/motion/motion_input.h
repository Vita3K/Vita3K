// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included

#pragma once

#include "util/quaternion.h"
#include "util/types.h"
#include "util/vector_math.h"

class MotionInput {
public:
    static constexpr float GyroMaxValue = 5.0f;
    static constexpr float AccelMaxValue = 7.0f;

    explicit MotionInput();

    MotionInput(const MotionInput &) = default;
    MotionInput &operator=(const MotionInput &) = default;

    MotionInput(MotionInput &&) = default;
    MotionInput &operator=(MotionInput &&) = default;

    void SetPID(SceFloat new_kp, SceFloat new_ki, SceFloat new_kd);
    void SetAcceleration(const Util::Vec3f &acceleration);
    void SetGyroscope(const Util::Vec3f &gyroscope);
    void SetQuaternion(const Util::Quaternion<SceFloat> &quaternion);
    void SetDeadband(SceFloat threshold);
    void SetAngleThreshold(SceFloat threshold);
    void RotateYaw(SceFloat radians);

    void EnableGyroBias(bool enable);
    void EnableTiltCorrection(bool enable);
    void EnableDeadband(bool enable);
    void EnableReset(bool reset);
    void ResetRotations();
    void ResetQuaternion();

    void UpdateRotation(SceULong64 elapsed_time);
    void UpdateOrientation(SceULong64 elapsed_time);
    void UpdateBasicOrientation();

    [[nodiscard]] Util::Quaternion<SceFloat> GetOrientation() const;
    [[nodiscard]] Util::Vec3f GetAcceleration() const;
    [[nodiscard]] Util::Vec3f GetGyroscope() const;
    [[nodiscard]] Util::Vec3f GetRotations() const;
    [[nodiscard]] SceFVector3 GetBasicOrientation() const;
    [[nodiscard]] SceFloat GetAngleThreshold() const;

    [[nodiscard]] bool IsMoving(SceFloat sensitivity) const;
    [[nodiscard]] bool IsCalibrated(SceFloat sensitivity) const;
    SceBool IsGyroBiasEnabled() const;
    SceBool IsTiltCorrectionEnabled() const;
    SceBool IsDeadbandEnabled() const;

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
    Util::Quaternion<SceFloat> quat{ { 0.0f, 0.0f, -1.0f }, 0.0f };

    // Number of full rotations in each axis
    Util::Vec3f rotations{};

    // Acceleration vector measurement in G force
    Util::Vec3f accel{};

    // Gyroscope vector measurement in radians/s.
    Util::Vec3f gyro{};

    // Vector to be subtracted from gyro measurements
    Util::Vec3f gyro_bias{};

    // Minimum gyro amplitude to detect if the device is moving
    SceFloat gyro_deadband = 0.0f;
    SceFloat gyro_deadband2 = 0.0f;

    // Minimum angle which basic motion orientation changes value
    SceFloat angle_threshold = 20.0f;

    // Current basic orientation of the device
    SceFVector3 basic_orientation = {};

    // It seems that the basic orientation is determined by gravity
    SceFVector3 basic_orientation_base = {};

    // Number of invalid sequential data
    SceFloat reset_counter = 0;

    // If the provided data is invalid the device will be autocalibrated
    bool reset_enabled = true;

    // If the provided gyro uses bias correction
    bool bias_enabled = true;

    // If the provided gyro uses gyro deadband
    bool deadband_enabled = true;

    // Corrects tilt based on accelerometer measurements
    bool tilt_correction_enabled = true;

    // Use accelerometer values to calculate position
    bool only_accelerometer = true;
};
