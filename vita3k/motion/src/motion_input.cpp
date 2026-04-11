// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included

#include <algorithm>

#include "motion/motion_input.h"

MotionInput::MotionInput() {
    // Initialize PID constants with default values
    SetPID(0.3f, 0.005f, 0.0f);
    SetDeadband(0.007f);
    ResetQuaternion();
    ResetRotations();
}

void MotionInput::SetPID(SceFloat new_kp, SceFloat new_ki, SceFloat new_kd) {
    kp = new_kp;
    ki = new_ki;
    kd = new_kd;
}

void MotionInput::SetAcceleration(const Util::Vec3f &acceleration) {
    accel = acceleration;

    accel.x = std::clamp(accel.x, -AccelMaxValue, AccelMaxValue);
    accel.y = std::clamp(accel.y, -AccelMaxValue, AccelMaxValue);
    accel.z = std::clamp(accel.z, -AccelMaxValue, AccelMaxValue);
}

void MotionInput::SetGyroscope(const Util::Vec3f &gyroscope) {
    gyro = gyroscope;

    if (bias_enabled) {
        gyro -= gyro_bias;
    }

    if (deadband_enabled && gyro.Length2() < gyro_deadband2) {
        gyro = {};
    }

    gyro.x = std::clamp(gyro.x, -GyroMaxValue, GyroMaxValue);
    gyro.y = std::clamp(gyro.y, -GyroMaxValue, GyroMaxValue);
    gyro.z = std::clamp(gyro.z, -GyroMaxValue, GyroMaxValue);

    // Auto adjust drift to minimize drift
    if (!IsMoving(0.1f)) {
        gyro_bias = (gyro_bias * 0.9999f) + (gyroscope * 0.0001f);
    }

    // Enable gyro compensation if gyro is active
    if (gyro.Length2() > 0) {
        only_accelerometer = false;
    }
}

void MotionInput::SetQuaternion(const Util::Quaternion<SceFloat> &quaternion) {
    quat = quaternion;
}

void MotionInput::SetDeadband(SceFloat threshold) {
    gyro_deadband = threshold;
    gyro_deadband2 = threshold * threshold;
}

void MotionInput::SetAngleThreshold(SceFloat threshold) {
    angle_threshold = std::clamp(threshold, 0.0f, 45.0f);
}

void MotionInput::RotateYaw(SceFloat radians) {
    const Util::Quaternion<SceFloat> yaw_rotation = {
        { 0.0f, 0.0f, -std::sin(radians / 2) },
        std::cos(radians / 2),
    };
    quat = yaw_rotation * quat;
    quat = quat.Normalized();
}

void MotionInput::EnableGyroBias(bool enable) {
    bias_enabled = enable;
}

void MotionInput::EnableTiltCorrection(bool enable) {
    tilt_correction_enabled = enable;
}

void MotionInput::EnableDeadband(bool enable) {
    deadband_enabled = enable;
}

void MotionInput::EnableReset(bool reset) {
    reset_enabled = reset;
}

void MotionInput::ResetRotations() {
    rotations = {};
}

SceFloat MotionInput::GetAngleThreshold() const {
    return angle_threshold;
}

void MotionInput::ResetQuaternion() {
    quat = { { 0.0f, 0.0f, -1.0f }, 0.0f };
}

bool MotionInput::IsMoving(SceFloat sensitivity) const {
    return gyro.Length() >= sensitivity || accel.Length() <= 0.9f || accel.Length() >= 1.1f;
}

bool MotionInput::IsCalibrated(SceFloat sensitivity) const {
    return real_error.Length() < sensitivity;
}

SceBool MotionInput::IsGyroBiasEnabled() const {
    return bias_enabled;
}

SceBool MotionInput::IsTiltCorrectionEnabled() const {
    return tilt_correction_enabled;
}

SceBool MotionInput::IsDeadbandEnabled() const {
    return deadband_enabled;
}

void MotionInput::UpdateRotation(SceULong64 elapsed_time) {
    const auto sample_period = static_cast<SceFloat>(elapsed_time) / 1000000.0f;
    if (sample_period > 0.1f) {
        return;
    }
    rotations += gyro * sample_period;
}

// Based on Madgwick's implementation of Mayhony's AHRS algorithm.
// https://github.com/xioTechnologies/Open-Source-AHRS-With-x-IMU/blob/master/x-IMU%20IMU%20and%20AHRS%20Algorithms/x-IMU%20IMU%20and%20AHRS%20Algorithms/AHRS/MahonyAHRS.cs
void MotionInput::UpdateOrientation(SceULong64 elapsed_time) {
    if (!IsCalibrated(0.1f)) {
        ResetOrientation();
    }
    // Short name local variable for readability
    SceFloat q1 = quat.w;
    SceFloat q2 = quat.xyz[0];
    SceFloat q3 = quat.xyz[1];
    SceFloat q4 = quat.xyz[2];
    const auto sample_period = static_cast<SceFloat>(elapsed_time) / 1000000.0f;

    // Ignore invalid elapsed time
    if (sample_period > 0.1f) {
        return;
    }

    const auto normal_accel = accel.Normalized();
    auto rad_gyro = gyro * 3.1415926f * 2;
    const SceFloat swap = rad_gyro.x;
    rad_gyro.x = rad_gyro.z;
    rad_gyro.z = -rad_gyro.y;
    rad_gyro.y = swap;

    // Clear gyro values if there is no gyro present
    if (only_accelerometer) {
        rad_gyro.x = 0;
        rad_gyro.y = 0;
        rad_gyro.z = 0;
    }

    // Ignore drift correction if acceleration is not reliable
    if (tilt_correction_enabled && accel.Length() >= 0.75f && accel.Length() <= 1.25f) {
        const SceFloat ax = normal_accel.x;
        const SceFloat ay = normal_accel.z;
        const SceFloat az = -normal_accel.y;

        // Estimated direction of gravity
        const SceFloat vx = 2.0f * (q2 * q4 - q1 * q3);
        const SceFloat vy = 2.0f * (q1 * q2 + q3 * q4);
        const SceFloat vz = q1 * q1 - q2 * q2 - q3 * q3 + q4 * q4;

        // Error is cross product between estimated direction and measured direction of gravity
        const Util::Vec3f new_real_error = {
            az * vx - ax * vz,
            ay * vz - az * vy,
            ax * vy - ay * vx,
        };

        derivative_error = new_real_error - real_error;
        real_error = new_real_error;

        // Prevent integral windup
        if (ki != 0.0f && !IsCalibrated(0.05f)) {
            integral_error += real_error;
        } else {
            integral_error = {};
        }

        // Apply feedback terms
        if (!only_accelerometer) {
            rad_gyro += kp * real_error;
            rad_gyro += ki * integral_error;
            rad_gyro += kd * derivative_error;
        } else {
            // Give more weight to accelerometer values to compensate for the lack of gyro
            rad_gyro += 35.0f * kp * real_error;
            rad_gyro += 10.0f * ki * integral_error;
            rad_gyro += 10.0f * kd * derivative_error;

            // Emulate gyro values for games that need them
            gyro.x = -rad_gyro.y;
            gyro.y = rad_gyro.x;
            gyro.z = -rad_gyro.z;
            UpdateRotation(elapsed_time);
        }
    }

    const SceFloat gx = rad_gyro.y;
    const SceFloat gy = rad_gyro.x;
    const SceFloat gz = rad_gyro.z;

    // Integrate rate of change of quaternion
    const SceFloat pa = q2;
    const SceFloat pb = q3;
    const SceFloat pc = q4;
    q1 = q1 + (-q2 * gx - q3 * gy - q4 * gz) * (0.5f * sample_period);
    q2 = pa + (q1 * gx + pb * gz - pc * gy) * (0.5f * sample_period);
    q3 = pb + (q1 * gy - pa * gz + pc * gx) * (0.5f * sample_period);
    q4 = pc + (q1 * gz + pa * gy - pb * gx) * (0.5f * sample_period);

    quat.w = q1;
    quat.xyz[0] = q2;
    quat.xyz[1] = q3;
    quat.xyz[2] = q4;
    quat = quat.Normalized();
}

void MotionInput::UpdateBasicOrientation() {
    SceFloat angle = angle_threshold * 3.1415926f / 180.0f;
    SceFloat max_angle_threshold_cos = std::cos(angle);
    SceFloat max_angle_threshold_sin = std::sin(angle);

    auto unit_accel = accel.Normalized();
    Util::Vec3f unit_xy = { accel.x, accel.y, 0 };

    if (std::abs(unit_accel.z) > max_angle_threshold_cos) {
        basic_orientation.x = 0;
        basic_orientation.y = 0;
        basic_orientation.z = unit_accel.z > 0 ? -1.0f : 1.0f;
        basic_orientation_base = basic_orientation;
    } else if (std::abs(unit_accel.z) < max_angle_threshold_sin || !basic_orientation_base.z) {
        unit_xy = unit_xy.Normalized();
        if (basic_orientation.z && std::abs(unit_accel.x) >= std::abs(unit_accel.y) || std::abs(unit_xy.x) > max_angle_threshold_cos) {
            basic_orientation.x = accel.x > 0 ? -1.0f : 1.0f;
            basic_orientation.y = 0.0f;
            basic_orientation.z = 0.0f;
            basic_orientation_base = basic_orientation;
        } else if (basic_orientation.z && std::abs(unit_accel.x) < std::abs(unit_accel.y) || std::abs(unit_xy.y) > max_angle_threshold_cos) {
            basic_orientation.x = 0.0f;
            basic_orientation.y = accel.y > 0 ? -1.0f : 1.0f;
            basic_orientation.z = 0.0f;
            basic_orientation_base = basic_orientation;
        }
    }
    basic_orientation = basic_orientation_base;

    if (basic_orientation.z && std::abs(unit_accel.z) < max_angle_threshold_cos) {
        basic_orientation.y = -1;
    } else {
        unit_xy = unit_xy.Normalized();
        if (basic_orientation.x && std::abs(unit_xy.y) > max_angle_threshold_sin) {
            basic_orientation.y = -1;
        }
        if (basic_orientation.y && std::abs(unit_xy.x) > max_angle_threshold_sin) {
            basic_orientation.y = -1;
        }
    }
}

SceFVector3 MotionInput::GetBasicOrientation() const {
    return basic_orientation;
}

Util::Quaternion<float> MotionInput::GetOrientation() const {
    return quat;
}

Util::Vec3f MotionInput::GetAcceleration() const {
    return accel;
}

Util::Vec3f MotionInput::GetGyroscope() const {
    return gyro;
}

Util::Vec3f MotionInput::GetRotations() const {
    return rotations;
}

void MotionInput::ResetOrientation() {
    if (!reset_enabled || only_accelerometer) {
        return;
    }
    if (!IsMoving(0.5f) && accel.z <= -0.9f) {
        ++reset_counter;
        if (reset_counter > 900) {
            quat.w = 0;
            quat.xyz[0] = 0;
            quat.xyz[1] = 0;
            quat.xyz[2] = -1;
            SetOrientationFromAccelerometer();
            integral_error = {};
            reset_counter = 0;
        }
    } else {
        reset_counter = 0;
    }
}

void MotionInput::SetOrientationFromAccelerometer() {
    int iterations = 0;
    const SceFloat sample_period = 0.015f;

    const auto normal_accel = accel.Normalized();

    while (!IsCalibrated(0.01f) && ++iterations < 100) {
        // Short name local variable for readability
        SceFloat q1 = quat.w;
        SceFloat q2 = quat.xyz[0];
        SceFloat q3 = quat.xyz[1];
        SceFloat q4 = quat.xyz[2];

        Util::Vec3f rad_gyro;
        const SceFloat ax = normal_accel.x;
        const SceFloat ay = normal_accel.z;
        const SceFloat az = -normal_accel.y;

        // Estimated direction of gravity
        const SceFloat vx = 2.0f * (q2 * q4 - q1 * q3);
        const SceFloat vy = 2.0f * (q1 * q2 + q3 * q4);
        const SceFloat vz = q1 * q1 - q2 * q2 - q3 * q3 + q4 * q4;

        // Error is cross product between estimated direction and measured direction of gravity
        const Util::Vec3f new_real_error = {
            az * vx - ax * vz,
            ay * vz - az * vy,
            ax * vy - ay * vx,
        };

        derivative_error = new_real_error - real_error;
        real_error = new_real_error;

        rad_gyro += 10.0f * kp * real_error;
        rad_gyro += 5.0f * ki * integral_error;
        rad_gyro += 10.0f * kd * derivative_error;

        const SceFloat gx = rad_gyro.y;
        const SceFloat gy = rad_gyro.x;
        const SceFloat gz = rad_gyro.z;

        // Integrate rate of change of quaternion
        const SceFloat pa = q2;
        const SceFloat pb = q3;
        const SceFloat pc = q4;
        q1 = q1 + (-q2 * gx - q3 * gy - q4 * gz) * (0.5f * sample_period);
        q2 = pa + (q1 * gx + pb * gz - pc * gy) * (0.5f * sample_period);
        q3 = pb + (q1 * gy - pa * gz + pc * gx) * (0.5f * sample_period);
        q4 = pc + (q1 * gz + pa * gy - pb * gx) * (0.5f * sample_period);

        quat.w = q1;
        quat.xyz[0] = q2;
        quat.xyz[1] = q3;
        quat.xyz[2] = q4;
        quat = quat.Normalized();
    }
}
