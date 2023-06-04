// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "util/vector_math.h"

#include <array>

namespace Util {

template <typename T>
class Quaternion {
public:
    Vec3<T> xyz;
    T w{};

    [[nodiscard]] Quaternion<decltype(-T{})> Inverse() const {
        return {-xyz, w};
    }

    [[nodiscard]] Quaternion<decltype(T{} + T{})> operator+(const Quaternion& other) const {
        return {xyz + other.xyz, w + other.w};
    }

    [[nodiscard]] Quaternion<decltype(T{} - T{})> operator-(const Quaternion& other) const {
        return {xyz - other.xyz, w - other.w};
    }

    [[nodiscard]] Quaternion<decltype(T{} * T{} - T{} * T{})> operator*(
        const Quaternion& other) const {
        return {xyz * other.w + other.xyz * w + Cross(xyz, other.xyz),
                w * other.w - Dot(xyz, other.xyz)};
    }

    [[nodiscard]] Quaternion<T> Normalized() const {
        T length = std::sqrt(xyz.Length2() + w * w);
        return {xyz / length, w / length};
    }

    [[nodiscard]] std::array<decltype(-T{}), 16> ToMatrix() const {
        const T x2 = xyz[0] * xyz[0];
        const T y2 = xyz[1] * xyz[1];
        const T z2 = xyz[2] * xyz[2];

        const T xy = xyz[0] * xyz[1];
        const T wz = w * xyz[2];
        const T xz = xyz[0] * xyz[2];
        const T wy = w * xyz[1];
        const T yz = xyz[1] * xyz[2];
        const T wx = w * xyz[0];

        return {1.0f - 2.0f * (y2 + z2),
                2.0f * (xy + wz),
                2.0f * (xz - wy),
                0.0f,
                2.0f * (xy - wz),
                1.0f - 2.0f * (x2 + z2),
                2.0f * (yz + wx),
                0.0f,
                2.0f * (xz + wy),
                2.0f * (yz - wx),
                1.0f - 2.0f * (x2 + y2),
                0.0f,
                0.0f,
                0.0f,
                0.0f,
                1.0f};
    }
};

template <typename T>
[[nodiscard]] auto QuaternionRotate(const Quaternion<T>& q, const Vec3<T>& v) {
    return v + 2 * Cross(q.xyz, Cross(q.xyz, v) + v * q.w);
}

[[nodiscard]] inline Quaternion<float> MakeQuaternion(const Vec3<float>& axis, float angle) {
    return {axis * std::sin(angle / 2), std::cos(angle / 2)};
}

} // namespace Common
