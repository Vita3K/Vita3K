// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// Copyright 2014 Tony Wasserka
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the owner nor the names of its contributors may
//       be used to endorse or promote products derived from this software
//       without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <cmath>
#include <type_traits>
#include "util/types.h"

namespace Util {

template <typename T>
class Vec3;

template <typename T>
class Vec3 {
public:
    T x{};
    T y{};
    T z{};

    constexpr Vec3() = default;
    constexpr Vec3(const T& x_, const T& y_, const T& z_) : x(x_), y(y_), z(z_) {}

    template <typename T2>
    [[nodiscard]] constexpr Vec3<T2> Cast() const {
        return Vec3<T2>(static_cast<T2>(x), static_cast<T2>(y), static_cast<T2>(z));
    }

    [[nodiscard]] static constexpr Vec3 AssignToAll(const T& f) {
        return Vec3(f, f, f);
    }

    [[nodiscard]] constexpr Vec3<decltype(T{} + T{})> operator+(const Vec3& other) const {
        return {x + other.x, y + other.y, z + other.z};
    }

    constexpr Vec3& operator+=(const Vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    [[nodiscard]] constexpr Vec3<decltype(T{} - T{})> operator-(const Vec3& other) const {
        return {x - other.x, y - other.y, z - other.z};
    }

    constexpr Vec3& operator-=(const Vec3& other) {
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    template <typename U = T>
    [[nodiscard]] constexpr Vec3<std::enable_if_t<std::is_signed_v<U>, U>> operator-() const {
        return {-x, -y, -z};
    }

    [[nodiscard]] constexpr Vec3<decltype(T{} * T{})> operator*(const Vec3& other) const {
        return {x * other.x, y * other.y, z * other.z};
    }

    template <typename V>
    [[nodiscard]] constexpr Vec3<decltype(T{} * V{})> operator*(const V& f) const {
        using TV = decltype(T{} * V{});
        using C = std::common_type_t<T, V>;

        return {
            static_cast<TV>(static_cast<C>(x) * static_cast<C>(f)),
            static_cast<TV>(static_cast<C>(y) * static_cast<C>(f)),
            static_cast<TV>(static_cast<C>(z) * static_cast<C>(f)),
        };
    }

    template <typename V>
    constexpr Vec3& operator*=(const V& f) {
        *this = *this * f;
        return *this;
    }
    template <typename V>
    [[nodiscard]] constexpr Vec3<decltype(T{} / V{})> operator/(const V& f) const {
        using TV = decltype(T{} / V{});
        using C = std::common_type_t<T, V>;

        return {
            static_cast<TV>(static_cast<C>(x) / static_cast<C>(f)),
            static_cast<TV>(static_cast<C>(y) / static_cast<C>(f)),
            static_cast<TV>(static_cast<C>(z) / static_cast<C>(f)),
        };
    }

    template <typename V>
    constexpr Vec3& operator/=(const V& f) {
        *this = *this / f;
        return *this;
    }

    [[nodiscard]] constexpr T Length2() const {
        return x * x + y * y + z * z;
    }

    // Only implemented for T=float
    [[nodiscard]] SceFloat Length() const;
    [[nodiscard]] Vec3 Normalized() const;
    [[nodiscard]] SceFloat Normalize(); // returns the previous length, which is often useful

    [[nodiscard]] constexpr T& operator[](std::size_t i) {
        return *((&x) + i);
    }

    [[nodiscard]] constexpr const T& operator[](std::size_t i) const {
        return *((&x) + i);
    }

    constexpr void SetZero() {
        x = 0;
        y = 0;
        z = 0;
    }

    // Common aliases: UVW (texel coordinates), RGB (colors), STQ (texture coordinates)
    [[nodiscard]] constexpr T& u() {
        return x;
    }
    [[nodiscard]] constexpr T& v() {
        return y;
    }
    [[nodiscard]] constexpr T& w() {
        return z;
    }

    [[nodiscard]] constexpr T& r() {
        return x;
    }
    [[nodiscard]] constexpr T& g() {
        return y;
    }
    [[nodiscard]] constexpr T& b() {
        return z;
    }

    [[nodiscard]] constexpr T& s() {
        return x;
    }
    [[nodiscard]] constexpr T& t() {
        return y;
    }
    [[nodiscard]] constexpr T& q() {
        return z;
    }

    [[nodiscard]] constexpr const T& u() const {
        return x;
    }
    [[nodiscard]] constexpr const T& v() const {
        return y;
    }
    [[nodiscard]] constexpr const T& w() const {
        return z;
    }

    [[nodiscard]] constexpr const T& r() const {
        return x;
    }
    [[nodiscard]] constexpr const T& g() const {
        return y;
    }
    [[nodiscard]] constexpr const T& b() const {
        return z;
    }

    [[nodiscard]] constexpr const T& s() const {
        return x;
    }
    [[nodiscard]] constexpr const T& t() const {
        return y;
    }
    [[nodiscard]] constexpr const T& q() const {
        return z;
    }
};

template <typename T, typename V>
[[nodiscard]] constexpr Vec3<T> operator*(const V& f, const Vec3<T>& vec) {
    using C = std::common_type_t<T, V>;

    return Vec3<T>(static_cast<T>(static_cast<C>(f) * static_cast<C>(vec.x)),
                   static_cast<T>(static_cast<C>(f) * static_cast<C>(vec.y)),
                   static_cast<T>(static_cast<C>(f) * static_cast<C>(vec.z)));
}

template <>
inline SceFloat Vec3<SceFloat>::Length() const {
    return std::sqrt(x * x + y * y + z * z);
}

template <>
inline Vec3<SceFloat> Vec3<SceFloat>::Normalized() const {
    return *this / Length();
}

template <>
inline SceFloat Vec3<SceFloat>::Normalize() {
    SceFloat length = Length();
    *this /= length;
    return length;
}

using Vec3f = Vec3<SceFloat>;

template <typename T>
[[nodiscard]] constexpr decltype(T{} * T{} + T{} * T{}) Dot(const Vec3<T>& a, const Vec3<T>& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

template <typename T>
[[nodiscard]] constexpr Vec3<decltype(T{} * T{} - T{} * T{})> Cross(const Vec3<T>& a,
                                                                    const Vec3<T>& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// linear interpolation via SceFloat: 0.0=begin, 1.0=end
template <typename X>
[[nodiscard]] constexpr decltype(X{} * SceFloat{} + X{} * SceFloat{})
    Lerp(const X &begin, const X &end, const SceFloat t) {
    return begin * (1.f - t) + end * t;
}

// linear interpolation via int: 0=begin, base=end
template <typename X, int base>
[[nodiscard]] constexpr decltype((X{} * int{} + X{} * int{}) / base) LerpInt(const X& begin,
                                                                             const X& end,
                                                                             const int t) {
    return (begin * (base - t) + end * t) / base;
}

// bilinear interpolation. s is for interpolating x00-x01 and x10-x11, and t is for the second
// interpolation.
template <typename X>
[[nodiscard]] constexpr auto BilinearInterp(const X& x00, const X& x01, const X& x10, const X& x11,
    const SceFloat s, const SceFloat t) {
    auto y0 = Lerp(x00, x01, s);
    auto y1 = Lerp(x10, x11, s);
    return Lerp(y0, y1, t);
}

// Utility vector factories

template <typename T>
[[nodiscard]] constexpr Vec3<T> MakeVec(const T& x, const T& y, const T& z) {
    return Vec3<T>{x, y, z};
}

} // namespace Common
